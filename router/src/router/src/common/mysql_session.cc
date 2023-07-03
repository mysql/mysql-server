/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

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
#include <chrono>
#include <functional>
#include <map>
#include <queue>
#include <sstream>
#include <string>

#include <mysql.h>

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

  // archive options for future connection templating
  conn_params_.ssl_opts = {ssl_mode, tls_version, ssl_cipher, ca,
                           capath,   crl,         crlpath};
}

void MySQLSession::set_ssl_cert(const std::string &cert,
                                const std::string &key) {
  if (!set_option(SslCert(cert.c_str())) || !set_option(SslKey(key.c_str()))) {
    throw Error("Error setting client SSL certificate for connection: " +
                    std::string(mysql_error(connection_)),
                mysql_errno(connection_));
  }

  // archive options for future connection templating
  conn_params_.ssl_cert = {cert, key};
}

void MySQLSession::connect(const std::string &host, unsigned int port,
                           const std::string &username,
                           const std::string &password,
                           const std::string &unix_socket,
                           const std::string &default_schema,
                           int connect_timeout, int read_timeout) {
  unsigned int protocol = MYSQL_PROTOCOL_TCP;
  connected_ = false;

  // Following would fail only when invalid values are given. It is not possible
  // for the user to change these values.
  set_option(ConnectTimeout(connect_timeout));
  set_option(ReadTimeout(read_timeout));

  if (unix_socket.length() > 0) {
#ifdef _WIN32
    protocol = MYSQL_PROTOCOL_PIPE;
#else
    protocol = MYSQL_PROTOCOL_SOCKET;
#endif
  }
  set_option(Protocol(protocol));

  const unsigned long client_flags =
      (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
       CLIENT_MULTI_RESULTS);
  std::string endpoint_str = unix_socket.length() > 0
                                 ? unix_socket
                                 : host + ":" + std::to_string(port);

  const bool ssl_disabled = conn_params_.ssl_opts.ssl_mode == SSL_MODE_DISABLED;
  auto &ssl_sessions_cache = SSLSessionsCache::instance();

  if (!ssl_disabled) {
    ssl_sessions_cache.try_reuse_session(connection_, endpoint_str);
  }

  if (!mysql_real_connect(connection_, host.c_str(), username.c_str(),
                          password.c_str(), default_schema.c_str(), port,
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

  // archive options for future connection templating
  conn_params_.conn_opts = {
      host,        port,           username,        password,
      unix_socket, default_schema, connect_timeout, read_timeout};
}

void MySQLSession::connect_and_set_opts(
    const ConnectionParameters &conn_params) {
  // should only be used on fresh objects
  // assert(!connected_);

  // below methods can throw:
  //   MySQLSession::Error (std::runtime_error)
  //   std::invalid_argument (std::logic_error)

  set_ssl_options(conn_params.ssl_opts.ssl_mode,
                  conn_params.ssl_opts.tls_version,
                  conn_params.ssl_opts.ssl_cipher, conn_params.ssl_opts.ca,
                  conn_params.ssl_opts.capath, conn_params.ssl_opts.crl,
                  conn_params.ssl_opts.crlpath);

  if (!conn_params.ssl_cert.cert.empty() || !conn_params.ssl_cert.key.empty())
    set_ssl_cert(conn_params.ssl_cert.cert, conn_params.ssl_cert.key);

  connect(conn_params.conn_opts.host, conn_params.conn_opts.port,
          conn_params.conn_opts.username, conn_params.conn_opts.password,
          conn_params.conn_opts.unix_socket,
          conn_params.conn_opts.default_schema,
          conn_params.conn_opts.connect_timeout,
          conn_params.conn_opts.read_timeout);
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
    return stdx::make_unexpected(
        make_mysql_error_code(CR_COMMANDS_OUT_OF_SYNC));
  }

  auto query_res = mysql_real_query(connection_, q.data(), q.size());

  if (query_res != 0) {
    return stdx::make_unexpected(make_mysql_error_code(connection_));
  }

  mysql_result_type res{mysql_store_result(connection_)};
  if (!res) {
    // no error, but also no resultset
    if (mysql_errno(connection_) == 0) return {};

    return stdx::make_unexpected(make_mysql_error_code(connection_));
  }

  return res;
}

stdx::expected<MySQLSession::mysql_result_type, MysqlError>
MySQLSession::logged_real_query(const std::string &q) {
  using clock_type = std::chrono::steady_clock;

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
    if (!processor(outrow)) break;
  }
}

class RealResultRow : public MySQLSession::ResultRow {
 public:
  RealResultRow(MySQLSession::Row row, MYSQL_RES *res)
      : ResultRow(std::move(row)), res_(res) {}

  ~RealResultRow() override { mysql_free_result(res_); }

 private:
  MYSQL_RES *res_;
};

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

const char *MySQLSession::last_error() {
  return connection_ ? mysql_error(connection_) : nullptr;
}

unsigned int MySQLSession::last_errno() {
  return connection_ ? mysql_errno(connection_) : 0;
}

const char *MySQLSession::ssl_cipher() {
  return connection_ ? mysql_get_ssl_cipher(connection_) : nullptr;
}

void MySQLSession::LoggingStrategyDebugLogger::log(const std::string &msg) {
  log_debug("%s", msg.c_str());
}
