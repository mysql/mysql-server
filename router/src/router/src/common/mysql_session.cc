/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/mysql_session.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <map>
#include <queue>
#include <sstream>
#include <string>

#include "mysql.h"
#include "violite.h"

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/mysql_client_thread_token.h"
#define MYSQL_ROUTER_LOG_DOMAIN "sql"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

using namespace mysqlrouter;
using namespace std::string_literals;

/*static*/ const char MySQLSession::kSslModeDisabled[] = "DISABLED";
/*static*/ const char MySQLSession::kSslModePreferred[] = "PREFERRED";
/*static*/ const char MySQLSession::kSslModeRequired[] = "REQUIRED";
/*static*/ const char MySQLSession::kSslModeVerifyCa[] = "VERIFY_CA";
/*static*/ const char MySQLSession::kSslModeVerifyIdentity[] =
    "VERIFY_IDENTITY";

namespace {

class SSLSessionsCache {
 public:
  using EndpointId = std::string;
  using CachedType = std::string;

  static SSLSessionsCache &instance() {
    static SSLSessionsCache i;
    return i;
  }

  bool was_session_reused(MYSQL *con) {
    return mysql_get_ssl_session_reused(con);
  }

  void store_ssl_session(MYSQL *con, const EndpointId &endpoint_id) {
    unsigned int len = 0;
    std::lock_guard lk(mtx_);

    static_assert(kMaxEntriesPerEndpoint > 0);

    void *data = mysql_get_ssl_session_data(con, 0, &len);
    if (len == 0) {
      // we failed to get the ssl session data, nothing to store
      return;
    }

    if (cache_.count(endpoint_id) > 0 &&
        cache_[endpoint_id].size() >= kMaxEntriesPerEndpoint) {
      // cache is full, remove the oldest entry to make room for the new one
      cache_[endpoint_id].pop();
    }

    cache_[endpoint_id].emplace(reinterpret_cast<char *>(data), len);

    mysql_free_ssl_session_data(con, data);
  }

  void try_reuse_session(MYSQL *con, const EndpointId &endpoint_id) {
    std::lock_guard lk(mtx_);

    if (cache_.count(endpoint_id) == 0 || cache_[endpoint_id].empty()) {
      return;
    }

    const auto &sess_data = cache_[endpoint_id].front();
    mysql_options(con, MYSQL_OPT_SSL_SESSION_DATA, sess_data.c_str());

    // once the session data was reused remove it from the cache
    cache_[endpoint_id].pop();
  }

 private:
  SSLSessionsCache() = default;

  // disable copying
  SSLSessionsCache(SSLSessionsCache &) = delete;
  SSLSessionsCache *operator=(SSLSessionsCache &) = delete;

  std::map<EndpointId, std::queue<CachedType>> cache_;
  std::mutex mtx_;

  static constexpr size_t kMaxEntriesPerEndpoint{2};
};

using MysqlSessionTrackGet = int (*)(MYSQL *mysql,
                                     enum enum_session_state_type type,
                                     const char **data, size_t *length);

}  // namespace

MySQLSession::MySQLSession(std::unique_ptr<LoggingStrategy> logging_strategy)
    : logging_strategy_(std::move(logging_strategy)) {
  MySQLClientThreadToken api_token;

  connection_ = new MYSQL();
  connected_ = false;
  if (!mysql_init(connection_)) {
    // not supposed to happen
    throw std::logic_error("Error initializing MySQL connection structure");
  }

  log_filter_.add_default_sql_patterns();
}

MySQLSession::~MySQLSession() {
  mysql_close(connection_);

  delete connection_;
}

/*static*/
mysql_ssl_mode MySQLSession::parse_ssl_mode(std::string ssl_mode) {
  // we allow lowercase equivalents, to be consistent with mysql client
  std::transform(ssl_mode.begin(), ssl_mode.end(), ssl_mode.begin(), toupper);

  if (ssl_mode == kSslModeDisabled)
    return SSL_MODE_DISABLED;
  else if (ssl_mode == kSslModePreferred)
    return SSL_MODE_PREFERRED;
  else if (ssl_mode == kSslModeRequired)
    return SSL_MODE_REQUIRED;
  else if (ssl_mode == kSslModeVerifyCa)
    return SSL_MODE_VERIFY_CA;
  else if (ssl_mode == kSslModeVerifyIdentity)
    return SSL_MODE_VERIFY_IDENTITY;
  else
    throw std::logic_error(std::string("Unrecognised SSL mode '") + ssl_mode +
                           "'");
}

/*static*/
const char *MySQLSession::ssl_mode_to_string(mysql_ssl_mode ssl_mode) noexcept {
  switch (ssl_mode) {
    case SSL_MODE_DISABLED:
      return kSslModeDisabled;
    case SSL_MODE_PREFERRED:
      return kSslModePreferred;
    case SSL_MODE_REQUIRED:
      return kSslModeRequired;
    case SSL_MODE_VERIFY_CA:
      return kSslModeVerifyCa;
    case SSL_MODE_VERIFY_IDENTITY:
      return kSslModeVerifyIdentity;
  }

  return nullptr;
}

void MySQLSession::set_ssl_options(mysql_ssl_mode ssl_mode,
                                   const std::string &tls_version,
                                   const std::string &ssl_cipher,
                                   const std::string &ca,
                                   const std::string &capath,
                                   const std::string &crl,
                                   const std::string &crlpath) {
  if (!ssl_cipher.empty() && !set_option(SslCipher(ssl_cipher.c_str()))) {
    throw Error(("Error setting SSL_CIPHER option for MySQL connection: " +
                 std::string(mysql_error(connection_))),
                mysql_errno(connection_));
  }

  if (!tls_version.empty() && !set_option(TlsVersion(tls_version.c_str()))) {
    throw Error("Error setting TLS_VERSION option for MySQL connection",
                mysql_errno(connection_));
  }

  if (!ca.empty() && !set_option(SslCa(ca.c_str()))) {
    throw Error(("Error setting SSL_CA option for MySQL connection: " +
                 std::string(mysql_error(connection_))),
                mysql_errno(connection_));
  }

  if (!capath.empty() && !set_option(SslCaPath(capath.c_str()))) {
    throw Error(("Error setting SSL_CAPATH option for MySQL connection: " +
                 std::string(mysql_error(connection_))),
                mysql_errno(connection_));
  }

  if (!crl.empty() && !set_option(SslCrl(crl.c_str()))) {
    throw Error(("Error setting SSL_CRL option for MySQL connection: " +
                 std::string(mysql_error(connection_))),
                mysql_errno(connection_));
  }

  if (!crlpath.empty() && !set_option(SslCrlPath(crlpath.c_str()))) {
    throw Error(("Error setting SSL_CRLPATH option for MySQL connection: " +
                 std::string(mysql_error(connection_))),
                mysql_errno(connection_));
  }

  // this has to be the last option that gets set due to what appears to be a
  // bug in libmysql causing ssl_mode downgrade from REQUIRED if other options
  // (like tls_version) are also specified
  if (!set_option(SslMode(ssl_mode))) {
    const char *text = ssl_mode_to_string(ssl_mode);
    std::string msg = std::string("Setting SSL mode to '") + text +
                      "' on connection failed: " + mysql_error(connection_);
    throw Error(msg, mysql_errno(connection_));
  }
}

mysql_ssl_mode MySQLSession::ssl_mode() const {
  SslMode ssl_mode;
  if (!get_option(ssl_mode)) {
    assert(0 && "get_option<SslMode>() failed unexpectedly");
  }

  return mysql_ssl_mode(ssl_mode.value());
}

std::string MySQLSession::tls_version() const {
  TlsVersion tls_version;
  if (!get_option(tls_version)) {
    assert(0 && "get_option<TlsVersion>() failed unexpectedly");
  }

  return tls_version.value() ? tls_version.value() : "";
}

std::string MySQLSession::ssl_cipher() const {
  SslCipher ssl_cipher;
  if (!get_option(ssl_cipher)) {
    assert(0 && "get_option<SslCipher>() failed unexpectedly");
  }

  return ssl_cipher.value() ? ssl_cipher.value() : "";
}

std::string MySQLSession::ssl_ca() const {
  SslCa ssl_ca;
  if (!get_option(ssl_ca)) {
    assert(0 && "get_option<SslCa>() failed unexpectedly");
  }

  return ssl_ca.value() ? ssl_ca.value() : "";
}

std::string MySQLSession::ssl_capath() const {
  SslCaPath ssl_capath;
  if (!get_option(ssl_capath)) {
    assert(0 && "get_option<SslCaPath>() failed unexpectedly");
  }

  return ssl_capath.value() ? ssl_capath.value() : "";
}

std::string MySQLSession::ssl_crl() const {
  SslCrl ssl_crl;
  if (!get_option(ssl_crl)) {
    assert(0 && "get_option<SslCrl>() failed unexpectedly");
  }

  return ssl_crl.value() ? ssl_crl.value() : "";
}

std::string MySQLSession::ssl_crlpath() const {
  SslCrlPath ssl_crlpath;
  if (!get_option(ssl_crlpath)) {
    assert(0 && "get_option<SslCrlPath>() failed unexpectedly");
  }

  return ssl_crlpath.value() ? ssl_crlpath.value() : "";
}

void MySQLSession::set_ssl_cert(const std::string &cert,
                                const std::string &key) {
  if (!set_option(SslCert(cert.c_str())) || !set_option(SslKey(key.c_str()))) {
    throw Error("Error setting client SSL certificate for connection: " +
                    std::string(mysql_error(connection_)),
                mysql_errno(connection_));
  }
}

std::string MySQLSession::ssl_cert() const {
  SslCert ssl_cert;
  if (!get_option(ssl_cert)) {
    assert(0 && "get_option<SslCert>() failed unexpectedly");
  }

  return ssl_cert.value() ? ssl_cert.value() : "";
}

std::string MySQLSession::ssl_key() const {
  SslKey ssl_key;
  if (!get_option(ssl_key)) {
    assert(0 && "get_option<SslKey>() failed unexpectedly");
  }

  return ssl_key.value() ? ssl_key.value() : "";
}

int MySQLSession::connect_timeout() const {
  ConnectTimeout connect_timeout;
  if (!get_option(connect_timeout)) {
    assert(0 && "get_option<ConnectTimeout>() failed unexpectedly");
  }

  return connect_timeout.value();
}

int MySQLSession::read_timeout() const {
  ReadTimeout read_timeout;
  if (!get_option(read_timeout)) {
    assert(0 && "get_option<ReadTimeout>() failed unexpectedly");
  }

  return read_timeout.value();
}

void MySQLSession::connect(const std::string &host, unsigned int port,
                           const std::string &username,
                           const std::string &password,
                           const std::string &unix_socket,
                           const std::string &default_schema,
                           int connect_timeout, int read_timeout,
                           unsigned long extra_client_flags) {
  unsigned int protocol = MYSQL_PROTOCOL_TCP;
  connected_ = false;

  // Following would fail only when invalid values are given. It is not possible
  // for the user to change these values.
  set_option(ConnectTimeout(connect_timeout));
  set_option(ReadTimeout(read_timeout));

  if (!unix_socket.empty()) {
#ifdef _WIN32
    protocol = MYSQL_PROTOCOL_PIPE;
#else
    protocol = MYSQL_PROTOCOL_SOCKET;
#endif
  }
  set_option(Protocol(protocol));

  const unsigned long client_flags =
      (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
       CLIENT_MULTI_RESULTS | extra_client_flags);
  std::string endpoint_str =
      !unix_socket.empty() ? unix_socket : host + ":" + std::to_string(port);

  const bool ssl_disabled = ssl_mode() == SSL_MODE_DISABLED;
  auto &ssl_sessions_cache = SSLSessionsCache::instance();

  if (!ssl_disabled) {
    ssl_sessions_cache.try_reuse_session(connection_, endpoint_str);
  }

  if (!mysql_real_connect(
          connection_, !unix_socket.empty() ? nullptr : host.c_str(),
          username.c_str(), password.c_str(), default_schema.c_str(), port,
          unix_socket.c_str(), client_flags)) {
    std::stringstream ss;
    ss << "Error connecting to MySQL server at " << endpoint_str;
    ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_)
       << ")";
    throw Error(ss.str(), mysql_errno(connection_));
  }

  if (!ssl_disabled) {
    ssl_sessions_cache.store_ssl_session(connection_, endpoint_str);
  }

  connected_ = true;
  connection_address_ = endpoint_str;

  // save the information about the endpoint we connected to
  connect_params_ = {host, port, unix_socket, default_schema};
}

void MySQLSession::connect(const MySQLSession &other,
                           const std::string &username,
                           const std::string &password) {
  // below methods can throw:
  //   MySQLSession::Error (std::runtime_error)
  //   std::invalid_argument (std::logic_error)

  set_ssl_options(other.ssl_mode(), other.tls_version(), other.ssl_cipher(),
                  other.ssl_ca(), other.ssl_capath(), other.ssl_crl(),
                  other.ssl_crlpath());

  if (!other.ssl_cert().empty() || !other.ssl_key().empty()) {
    set_ssl_cert(other.ssl_cert(), other.ssl_key());
  }

  connect(other.connect_params_.host, other.connect_params_.port, username,
          password, other.connect_params_.unix_socket,
          other.connect_params_.unix_socket, other.connect_timeout(),
          other.read_timeout(), 0);
}

void MySQLSession::reset() {
  if (mysql_reset_connection(connection_)) {
    std::stringstream ss;
    ss << "Error while resetting session, MySQL server at "
       << connection_address_;
    ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_)
       << ")";
    throw Error(ss.str(), mysql_errno(connection_));
  }
}

void MySQLSession::change_user(const std::string &user,
                               const std::string &password,
                               const std::string &db) {
  if (mysql_change_user(connection_, user.c_str(), password.c_str(),
                        db.c_str())) {
    std::stringstream ss;
    ss << "Error while changing user, MySQL server at " << connection_address_;
    ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_)
       << ")";
    throw Error(ss.str(), mysql_errno(connection_));
  }
}

void MySQLSession::disconnect() {
  // close the socket and free internal data
  mysql_close(connection_);

  // initialize the connection handle again as _close() is also free()ing
  // a lot of internal data.
  MySQLClientThreadToken api_token;
  mysql_init(connection_);
  connected_ = false;
  connection_address_.clear();
}

const std::error_category &mysql_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "mysql_client"; }
    std::string message(int ev) const override { return ER_CLIENT(ev); }
  };

  static category_impl instance;
  return instance;
}

static MysqlError make_mysql_error_code(unsigned int e) {
  return {e, ER_CLIENT(e), "HY000"};
}

static MysqlError make_mysql_error_code(MYSQL *m) {
  return {mysql_errno(m), mysql_error(m), mysql_sqlstate(m)};
}

stdx::expected<MySQLSession::mysql_result_type, MysqlError>
MySQLSession::real_query(const std::string &q) {
  if (!connected_) {
    return stdx::unexpected(make_mysql_error_code(CR_COMMANDS_OUT_OF_SYNC));
  }

  auto query_res = mysql_real_query(connection_, q.data(), q.size());

  if (query_res != 0) {
    return stdx::unexpected(make_mysql_error_code(connection_));
  }

  mysql_result_type res{mysql_store_result(connection_)};
  if (!res) {
    // no error, but also no resultset
    if (mysql_errno(connection_) == 0) return {};

    return stdx::unexpected(make_mysql_error_code(connection_));
  }

  return res;
}

stdx::expected<MySQLSession::mysql_result_type, MysqlError>
MySQLSession::logged_real_query(const std::string &q) {
  using clock_type = std::chrono::steady_clock;

  if (!logging_strategy_) return real_query(q);

  auto start = clock_type::now();
  auto query_res = real_query(q);
  auto dur = clock_type::now() - start;
  auto msg =
      get_address() + " (" +
      std::to_string(
          std::chrono::duration_cast<std::chrono::microseconds>(dur).count()) +
      " us)> " + log_filter_.filter(q);
  if (query_res) {
    auto const *res = query_res.value().get();

    msg += " // OK";
    if (res) {
      msg += " " + std::to_string(res->row_count) + " row" +
             (res->row_count != 1 ? "s" : "");
    }
  } else {
    auto err = query_res.error();
    msg += " // ERROR: " + std::to_string(err.value()) + " " + err.message();
  }
  logging_strategy_->log(msg);

  return query_res;
}

uint64_t MySQLSession::prepare(const std::string &query) {
  auto current = last_stmt_id++;
  auto stmt = stmts_[current] = mysql_stmt_init(connection_);
  if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
    // non zero is an error.
    auto err_no = mysql_stmt_errno(stmt);
    auto err_msg = mysql_stmt_error(stmt);
    std::stringstream ss;
    ss << "Error preparing MySQL query \"" << log_filter_.filter(query);
    ss << "\": " << err_msg << " (" << err_no << ")";
    prepare_remove(current);
    throw Error(ss.str(), err_no, err_msg);
  }

  return current;
}

char *allocate_buffer(MYSQL_BIND *bind) {
  const int k_string_size = 1000;
  char *buffer = nullptr;
  switch (bind->buffer_type) {
    case MYSQL_TYPE_STRING:
      bind->buffer = buffer = new char[bind->buffer_length = 2 * k_string_size];
      break;

    case MYSQL_TYPE_LONGLONG:
      bind->buffer = buffer =
          new char[bind->buffer_length = 2 * sizeof(long long)];
      break;

    case MYSQL_TYPE_DOUBLE:
      bind->buffer = buffer =
          new char[bind->buffer_length = 2 * sizeof(double)];
      break;
    case MYSQL_TYPE_BOOL:
      bind->buffer = buffer = new char[bind->buffer_length = 2 * sizeof(int)];
      break;
    case MYSQL_TYPE_LONG:
      bind->buffer = buffer = new char[bind->buffer_length = 2 * sizeof(long)];
      break;
    case MYSQL_TYPE_TIMESTAMP:
      bind->buffer = buffer = new char[bind->buffer_length = 2 * k_string_size];
      break;

    default:
      assert(nullptr && "should not happen");
  }
  return buffer;
}

class StmtResultRow : public MySQLSession::ResultRow {
 public:
  StmtResultRow(MySQLSession::Row row, unsigned long *sizes)
      : ResultRow(row), sizes_{sizes} {}
  virtual size_t get_data_size(size_t i) const override { return sizes_[i]; }

 private:
  unsigned long *sizes_;
};

void MySQLSession::throw_mysqlerror(MYSQL_STMT *stmt, uint64_t ps_id) {
  // non zero is an error.
  auto err_no = mysql_stmt_errno(stmt);
  auto err_msg = mysql_stmt_error(stmt);
  std::stringstream ss;
  ss << "Error executing prepared statement with id:" << ps_id;
  ss << "\": " << err_msg << " (" << err_no << ")";
  throw Error(ss.str(), err_no, err_msg);
}

void MySQLSession::prepare_execute(uint64_t ps_id,
                                   std::vector<enum_field_types> pt,
                                   const ResultRowProcessor &processor,
                                   const FieldValidator &validator) {
  std::vector<std::unique_ptr<char[]>> buffers;
  std::unique_ptr<MYSQL_BIND[]> ps_params{new MYSQL_BIND[pt.size() + 1]};
  auto bind = ps_params.get();
  auto stmt = stmts_[ps_id];

  memset(ps_params.get(), 0, sizeof(MYSQL_BIND) * pt.size());

  for (auto type : pt) {
    bind->buffer_type = type;
    buffers.emplace_back(allocate_buffer(bind));
    ++bind;
  }

  if (mysql_stmt_bind_param(stmt, ps_params.get())) {
    // non zero is an error.
    auto err_no = mysql_stmt_errno(stmt);
    auto err_msg = mysql_stmt_error(stmt);
    std::stringstream ss;
    ss << "Binding output-parameters for stmt id:" << ps_id;
    ss << "\": " << err_msg << " (" << err_no << ")";
    throw Error(ss.str(), err_no, err_msg);
  }

  // non zero is an error.
  if (mysql_stmt_execute(stmt)) {
    throw_mysqlerror(stmt, ps_id);
  }

  int status;
  do {
    unsigned int nfields = mysql_stmt_field_count(stmt);
    MYSQL_RES *rs_metadata = mysql_stmt_result_metadata(stmt);
    MYSQL_FIELD *fields =
        rs_metadata ? mysql_fetch_fields(rs_metadata) : nullptr;
    std::unique_ptr<MYSQL_BIND[]> my_bind{new MYSQL_BIND[nfields]};
    std::unique_ptr<unsigned long[]> length{new unsigned long[nfields]};
    std::unique_ptr<bool[]> is_null{new bool[nfields]};
    memset(my_bind.get(), 0, sizeof(MYSQL_BIND) * nfields);

    validator(nfields, fields);
    for (unsigned int i = 0; i < nfields; ++i) {
      size_t max_length =
          std::max<unsigned long>(fields[i].max_length + 1, 1000);
      my_bind[i].buffer_type = MYSQL_TYPE_STRING;
      my_bind[i].buffer = new char[max_length];
      my_bind[i].buffer_length = static_cast<unsigned long>(max_length);
      my_bind[i].is_null = &is_null[i];
      my_bind[i].length = &length[i];
    }

    mysql_stmt_bind_result(stmt, my_bind.get());

    std::vector<const char *> outrow;
    outrow.resize(nfields);
    while (true) {
      status = mysql_stmt_fetch(stmt);
      if (status == 1 || status == MYSQL_NO_DATA) break;
      for (unsigned int i = 0; i < nfields; i++) {
        outrow[i] = *my_bind[i].is_null
                        ? nullptr
                        : reinterpret_cast<const char *>(my_bind[i].buffer);
      }
      StmtResultRow stmt_result{outrow, length.get()};
      if (!processor(stmt_result)) break;
    }
    mysql_free_result(rs_metadata);
    for (unsigned int i = 0; i < nfields; ++i) {
      delete[] reinterpret_cast<char *>(my_bind[i].buffer);
    }
    status = mysql_stmt_next_result(stmt);
  } while (status == 0);

  if (status == MYSQL_NO_DATA) return;
  if (status == CR_NO_RESULT_SET) return;

  throw_mysqlerror(stmt, ps_id);
}

void MySQLSession::prepare_remove(uint64_t ps_id) {
  mysql_stmt_close(stmts_[ps_id]);
  stmts_.erase(ps_id);
}

void MySQLSession::execute(const std::string &q) {
  auto query_res = logged_real_query(q);

  if (!query_res) {
    auto ec = query_res.error();

    std::stringstream ss;
    ss << "Error executing MySQL query \"" << log_filter_.filter(q);
    ss << "\": " << ec.message() << " (" << ec.value() << ")";
    throw Error(ss.str(), ec.value(), ec.message());
  }

  // in case we got a result, just let it get freed.
}

/*
  Execute query on the session and iterate the results with the given
  callback.

  The processor callback is called with a vector of strings, which contain the
  values of each field of a row. It is called once per row.
  If the processor returns false, the result row iteration stops.
 */
// throws MySQLSession::Error, std::logic_error, whatever processor() throws,
// ...?
void MySQLSession::query(
    const std::string &q, const RowProcessor &processor,
    const FieldValidator &validator /*=null_field_validator*/) {
  ResultRowProcessor callback = [&processor](const ResultRow &rr) {
    return processor(rr.row_);
  };
  query(q, callback, validator);
}

class RealResultRow : public MySQLSession::ResultRow {
 public:
  RealResultRow(MySQLSession::Row row, MYSQL_RES *res, bool delete_res = true)
      : ResultRow(std::move(row)), res_(res), delete_res_{delete_res} {}

  ~RealResultRow() override {
    if (delete_res_) mysql_free_result(res_);
  }
  size_t get_data_size(size_t i) const override {
    log_debug("Session::get_data_size");
    return mysql_fetch_lengths(res_)[i];
  }

 private:
  MYSQL_RES *res_;
  bool delete_res_;
};

void MySQLSession::query(const std::string &q,
                         const ResultRowProcessor &processor,
                         const FieldValidator &validator) {
  auto query_res = logged_real_query(q);

  if (!query_res) {
    auto ec = query_res.error();

    std::stringstream ss;
    ss << "Error executing MySQL query \"" << log_filter_.filter(q);
    ss << "\": " << ec.message() << " (" << ec.value() << ")";
    throw Error(ss.str(), ec.value(), ec.message());
  }

  // no resultset
  if (!query_res.value()) return;

  auto *res = query_res.value().get();

  // get column info and give it to field validator,
  // which should throw if it doesn't like the columns
  unsigned int nfields = mysql_num_fields(res);

  MYSQL_FIELD *fields = mysql_fetch_fields(res);
  validator(nfields, fields);

  std::vector<const char *> outrow;
  outrow.resize(nfields);
  while (MYSQL_ROW row = mysql_fetch_row(res)) {
    for (unsigned int i = 0; i < nfields; i++) {
      outrow[i] = row[i];
    }
    RealResultRow result{outrow, res, false};
    if (!processor(result)) break;
  }
}

std::unique_ptr<MySQLSession::ResultRow> MySQLSession::query_one(
    const std::string &q,
    const FieldValidator &validator /*= null_field_validator*/) {
  auto query_res = logged_real_query(q);

  if (!query_res) {
    auto ec = query_res.error();

    std::stringstream ss;
    ss << "Error executing MySQL query \"" << log_filter_.filter(q);
    ss << "\": " << ec.message() << " (" << ec.value() << ")";
    throw Error(ss.str(), ec.value(), ec.message());
  }

  // no resultset
  if (!query_res.value()) return {};

  auto *res = query_res.value().get();

  // get column info and give it to field validator,
  // which should throw if it doesn't like the columns
  unsigned int nfields = mysql_num_fields(res);
  MYSQL_FIELD *fields = mysql_fetch_fields(res);
  validator(nfields, fields);

  if (nfields == 0) return {};

  if (MYSQL_ROW row = mysql_fetch_row(res)) {
    std::vector<const char *> outrow(nfields);

    for (unsigned int i = 0; i < nfields; i++) {
      outrow[i] = row[i];
    }

    return std::make_unique<RealResultRow>(outrow, query_res.value().release());
  }

  return {};
}

uint64_t MySQLSession::last_insert_id() noexcept {
  return mysql_insert_id(connection_);
}

uint64_t MySQLSession::affected_rows() noexcept {
  return mysql_affected_rows(connection_);
}

bool MySQLSession::ping() { return 0 == mysql_ping(connection_); }

bool MySQLSession::has_data_on_socket() {
  return 0 < vio_io_wait(connection_->net.vio, VIO_IO_EVENT_READ, 0);
}

std::vector<std::string> MySQLSession::get_session_tracker_data(
    enum enum_session_state_type type) {
  std::vector<std::string> result;
  const char *data;
  size_t data_length;

  MysqlSessionTrackGet st_get = mysql_session_track_get_first;

  while (!st_get(connection_, type, &data, &data_length)) {
    result.emplace_back(data, data_length);
    st_get = mysql_session_track_get_next;
  }

  return result;
}

unsigned MySQLSession::warning_count() noexcept {
  return mysql_warning_count(connection_);
}

std::string MySQLSession::quote(const std::string &s, char qchar) const {
  std::string r;
  r.resize(s.length() * 2 + 3);
  r[0] = qchar;
  unsigned long len = mysql_real_escape_string_quote(
      connection_, &r[1], s.c_str(), s.length(), qchar);
  r.resize(len + 2);
  r[len + 1] = qchar;
  return r;
}

const char *MySQLSession::last_sqlstate() {
  return connection_ ? mysql_sqlstate(connection_) : nullptr;
}
const char *MySQLSession::last_error() {
  return connection_ ? mysql_error(connection_) : nullptr;
}

unsigned int MySQLSession::last_errno() {
  return connection_ ? mysql_errno(connection_) : 0;
}

const char *MySQLSession::ssl_cipher() {
  return connection_ ? mysql_get_ssl_cipher(connection_) : nullptr;
}

bool MySQLSession::is_ssl_session_reused() {
  return connection_ ? mysql_get_ssl_session_reused(connection_) : false;
}

unsigned long MySQLSession::server_version() {
  return connection_ ? mysql_get_server_version(connection_) : 0;
}

void MySQLSession::LoggingStrategyDebugLogger::log(const std::string &msg) {
  log_debug("%s", msg.c_str());
}
