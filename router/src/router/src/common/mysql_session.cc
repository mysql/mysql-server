/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
#define MYSQL_ROUTER_LOG_DOMAIN "sql"
#include "mysql/harness/logging/logging.h"

#include <assert.h>  // <cassert> is flawed: assert() lands in global namespace on Ubuntu 14.04, not std::
#include <ctype.h>  // not <cctype> because we don't want std::toupper(), which causes problems with std::transform()
#include <mysql.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

IMPORT_LOG_FUNCTIONS()

using namespace mysqlrouter;

/*
   Mock recorder for MySQLSession
   ------------------------------

   In dev builds, #define MOCK_RECORDER below to enable recording of
   MySQL sessions for mock test writing purposes.

   When enabled, the MYSQL_ROUTER_RECORD_MOCK environment variable can be set
   to make MySQLSession dump all calls to it, along with its results, so that
   they can be replayed later with a MySQLSessionReplayer object.
 */
// #define MOCK_RECORDER
// #define MOCK_RECORDER_JSON

#if defined(NDEBUG)
#ifdef MOCK_RECORDER
#undef MOCK_RECORDER
#endif
#endif

/*static*/ const char MySQLSession::kSslModeDisabled[] = "DISABLED";
/*static*/ const char MySQLSession::kSslModePreferred[] = "PREFERRED";
/*static*/ const char MySQLSession::kSslModeRequired[] = "REQUIRED";
/*static*/ const char MySQLSession::kSslModeVerifyCa[] = "VERIFY_CA";
/*static*/ const char MySQLSession::kSslModeVerifyIdentity[] =
    "VERIFY_IDENTITY";

#ifdef MOCK_RECORDER
#ifdef MOCK_RECORDER_JSON

#define RAPIDJSON_HAS_STDSTRING 1

#include <cstdio>
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"

using StreamWriter =
    RAPIDJSON_NAMESPACE::PrettyWriter<RAPIDJSON_NAMESPACE::FileWriteStream>;

class Writer : public StreamWriter {
 public:
  Writer(std::FILE *f)
      : StreamWriter(f_stream_), f_stream_(f, buf_, sizeof(buf_)) {}

 private:
  RAPIDJSON_NAMESPACE::FileWriteStream f_stream_;
  char buf_[1024];
};

static const char *type_names[] = {
    "TINY",      "SHORT",      "LONG",     "INT24",    "LONGLONG",
    "DECIMAL",   "NEWDECIMAL", "FLOAT",    "DOUBLE",   "BIT",
    "TIMESTAMP", "DATE",       "TIME",     "DATETIME", "YEAR",
    "STRING",    "VAR_STRING", "BLOB",     "SET",      "ENUM",
    "GEOMETRY",  "NULL",       "TINYBLOB", "LONGBLOB", "MEDIUMBLOB",
};

int type_names_ndx(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
      return 0;
    case MYSQL_TYPE_SHORT:
      return 1;
    case MYSQL_TYPE_LONG:
      return 2;
    case MYSQL_TYPE_INT24:
      return 3;
    case MYSQL_TYPE_LONGLONG:
      return 4;
    case MYSQL_TYPE_DECIMAL:
      return 5;
    case MYSQL_TYPE_NEWDECIMAL:
      return 6;
    case MYSQL_TYPE_FLOAT:
      return 7;
    case MYSQL_TYPE_DOUBLE:
      return 8;
    case MYSQL_TYPE_BIT:
      return 9;
    case MYSQL_TYPE_TIMESTAMP:
      return 10;
    case MYSQL_TYPE_DATE:
      return 11;
    case MYSQL_TYPE_TIME:
      return 12;
    case MYSQL_TYPE_DATETIME:
      return 13;
    case MYSQL_TYPE_YEAR:
      return 14;
    case MYSQL_TYPE_STRING:
      return 15;
    case MYSQL_TYPE_VAR_STRING:
      return 16;
    case MYSQL_TYPE_BLOB:
      return 17;
    case MYSQL_TYPE_SET:
      return 18;
    case MYSQL_TYPE_ENUM:
      return 19;
    case MYSQL_TYPE_GEOMETRY:
      return 20;
    case MYSQL_TYPE_NULL:
      return 21;
    case MYSQL_TYPE_TINY_BLOB:
      return 22;
    case MYSQL_TYPE_LONG_BLOB:
      return 23;
    case MYSQL_TYPE_MEDIUM_BLOB:
      return 24;
    default:
      throw std::invalid_argument("invalid protocol field type: " +
                                  std::to_string(type));
  }
}

class MySQLServerMockRecorder {
 public:
  MySQLServerMockRecorder(FILE *f) : writer_(f) {
    writer_.StartObject();
    writer_.Key("stmts");
    writer_.StartArray();

    // we never close it ...
  }

  ~MySQLServerMockRecorder() {
    writer_.EndArray();
    writer_.EndObject();
  }

  void execute(const std::string &q) {
    exec_start_ = std::chrono::steady_clock::now();

    writer_.StartObject();

    writer_.Key("stmt");
    writer_.String(q);
  }

  void query(const std::string &q) { execute(q); }

  void query_one(const std::string &q) { execute(q); }

  void write_exec_time() {
    writer_.Key("exec_time");
    writer_.Double(std::chrono::duration<double, std::milli>(
                       std::chrono::steady_clock::now() - exec_start_)
                       .count());
  }

  void execute_done(uint64_t last_insert_id, uint64_t warning_count) {
    write_exec_time();

    writer_.Key("ok");
    writer_.StartObject();
    if (last_insert_id) {
      writer_.Key("last_insert_id");
      writer_.Uint(last_insert_id);
    }
    if (warning_count) {
      writer_.Key("warning_count");
      writer_.Uint(warning_count);
    }
    writer_.EndObject();

    writer_.EndObject();
  }

  void result_error(const char *error, unsigned int code, const char *sql_state,
                    MySQLSession &) {
    write_exec_time();

    writer_.Key("error");
    writer_.StartObject();
    writer_.Key("code");
    writer_.Uint(code);

    writer_.Key("message");
    writer_.String(error);

    writer_.Key("sql_state");
    writer_.String(sql_state);

    writer_.EndObject();

    writer_.EndObject();
  }

  void result_rows_begin(unsigned int num_fields, MYSQL_FIELD *fields) {
    write_exec_time();

    nfields_ = num_fields;

    writer_.Key("result");

    writer_.StartObject();
    writer_.Key("columns");

    writer_.StartArray();
    for (unsigned int i = 0; i < num_fields; i++) {
      writer_.StartObject();
      writer_.Key("name");
      writer_.String(fields[i].name);

      writer_.Key("type");
      // the compiler should be smart enough to convert the strlen() into a
      // sizeof()
      writer_.String(type_names[type_names_ndx(fields[i].type)],
                     strlen(type_names[type_names_ndx(fields[i].type)]));
      writer_.EndObject();
    }
    writer_.EndArray();

    writer_.Key("rows");
    writer_.StartArray();
  }

  void result_rows_add(MYSQL_ROW row, MySQLSession &) {
    writer_.StartArray();
    for (unsigned int i = 0; i < nfields_; i++) {
      if (row[i])
        writer_.String(row[i]);
      else
        writer_.Null();
    }
    writer_.EndArray();
  }

  void result_rows_end() {
    writer_.EndArray();
    writer_.EndObject();
    writer_.EndObject();
  }

 private:
  std::chrono::steady_clock::time_point exec_start_;
  unsigned int nfields_;
  Writer writer_;
};

static MySQLServerMockRecorder g_mock_recorder(fopen(
    getenv("MYSQL_ROUTER_RECORD_MOCK") ? getenv("MYSQL_ROUTER_RECORD_MOCK")
                                       : "/dev/null",
    "wb"));
#else
class GoogleMockRecorder {
 public:
  GoogleMockRecorder() : record_(false) {
    const char *outfile = std::getenv("MYSQL_ROUTER_RECORD_MOCK");
    if (outfile) {
      std::cerr << "Enabled mock recording...\n";
      record_ = true;
      outf_.open(outfile, std::ofstream::trunc | std::ofstream::out);
    }
  }

  void execute(const std::string &q) {
    outf_ << "  m.expect_execute(\"" << q << "\");\n";
  }

  void query(const std::string &q) {
    outf_ << "  m.expect_query(\"" << q << "\");\n";
  }

  void query_one(const std::string &q) {
    outf_ << "  m.expect_query_one(\"" << q << "\");\n";
  }

  void execute_done(uint64_t last_insert_id, uint64_t /* warning_count */) {
    outf_ << "  m.then_ok(" << last_insert_id << ");\n";
  }

  void result_error(const char *error, unsigned int code,
                    const char * /* sql_state */, MySQLSession &s) {
    outf_ << "  m.then_error(" << s.quote(error, '\"') << ", " << code
          << ");\n\n";
  }

  void result_rows_begin(unsigned int num_fields, MYSQL_FIELD *fields) {
    nfields_ = num_fields;
    need_comma_ = false;
    outf_ << "  m.then_return(" << num_fields << ", {\n";
    outf_ << "      // ";
    for (unsigned int i = 0; i < num_fields; i++) {
      if (i > 0) outf_ << ", ";
      outf_ << fields[i].name;
    }
    outf_ << "\n";
  }

  void result_rows_add(MYSQL_ROW row, MySQLSession &s) {
    if (need_comma_) {
      outf_ << ",\n";
    }
    need_comma_ = true;
    outf_ << "      {";
    for (unsigned int i = 0; i < nfields_; i++) {
      if (i > 0) outf_ << ", ";
      if (row[i])
        outf_ << "m.string_or_null(" << s.quote(row[i], '\"') << ")";
      else
        outf_ << "m.string_or_null()";
    }
    outf_ << "}";
  }

  void result_rows_end() {
    if (need_comma_) outf_ << "\n";
    outf_ << "    });\n\n";
  }

 private:
  std::ofstream outf_;
  unsigned int nfields_;
  bool need_comma_;
  bool record_;
};

static GoogleMockRecorder g_mock_recorder;
#endif

#define MOCK_REC_EXECUTE(q) g_mock_recorder.execute(q)
#define MOCK_REC_OK(lid, warning_count) \
  g_mock_recorder.execute_done(lid, warning_count)
#define MOCK_REC_QUERY(q) g_mock_recorder.query(q)
#define MOCK_REC_QUERY_ONE(q) g_mock_recorder.query_one(q)
#define MOCK_REC_ERROR(e, c, s, m) g_mock_recorder.result_error(e, c, s, m)
#define MOCK_REC_BEGIN(nf, f) g_mock_recorder.result_rows_begin(nf, f)
#define MOCK_REC_END() g_mock_recorder.result_rows_end()
#define MOCK_REC_ROW(r, m) g_mock_recorder.result_rows_add(r, m)
#else  // !MOCK_RECORDER
#define MOCK_REC_EXECUTE(q) \
  do {                      \
  } while (0)
#define MOCK_REC_OK(lid, warning_count) \
  do {                                  \
  } while (0)
#define MOCK_REC_QUERY(q) \
  do {                    \
  } while (0)
#define MOCK_REC_QUERY_ONE(q) \
  do {                        \
  } while (0)
#define MOCK_REC_ERROR(e, c, s, m) \
  do {                             \
  } while (0)
#define MOCK_REC_BEGIN(nf, f) \
  do {                        \
  } while (0)
#define MOCK_REC_END() \
  do {                 \
  } while (0)
#define MOCK_REC_ROW(r, m) \
  do {                     \
  } while (0)
#endif  // !MOCK_RECORDER

MySQLSession::MySQLSession() {
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
  const char *text = NULL;

  // The better way would be to do away with text variable and return kSslMode*
  // directly from each case. Unfortunately, Clang 3.4 doesn't like it:
  //   control reaches end of non-void function [-Werror=return-type]
  // even though it knows all cases are handled (issues another warning if any
  // one is removed).
  switch (ssl_mode) {
    case SSL_MODE_DISABLED:
      text = kSslModeDisabled;
      break;
    case SSL_MODE_PREFERRED:
      text = kSslModePreferred;
      break;
    case SSL_MODE_REQUIRED:
      text = kSslModeRequired;
      break;
    case SSL_MODE_VERIFY_CA:
      text = kSslModeVerifyCa;
      break;
    case SSL_MODE_VERIFY_IDENTITY:
      text = kSslModeVerifyIdentity;
      break;
  }

  return text;
}

bool MySQLSession::check_for_yassl(MYSQL *connection) {
  static bool check_done = false;
  static bool is_yassl = false;
  if (!check_done) {
    const char *old_version{nullptr};
    // the assumption is that yaSSL does not support this version
    const char *kTlsNoYassl = "TLSv1.2";

    if (mysql_get_option(connection, MYSQL_OPT_TLS_VERSION, &old_version)) {
      throw Error("Error checking for SSL implementation",
                  mysql_errno(connection));
    }
    int res = mysql_options(connection, MYSQL_OPT_TLS_VERSION, kTlsNoYassl);
    is_yassl = (res != 0);
    if (mysql_options(connection, MYSQL_OPT_TLS_VERSION, old_version)) {
      throw Error("Error checking for SSL implementation",
                  mysql_errno(connection));
    }
    check_done = true;
  }

  return is_yassl;
}

void MySQLSession::set_ssl_options(mysql_ssl_mode ssl_mode,
                                   const std::string &tls_version,
                                   const std::string &ssl_cipher,
                                   const std::string &ca,
                                   const std::string &capath,
                                   const std::string &crl,
                                   const std::string &crlpath) {
  if (check_for_yassl(connection_)) {
    if ((ssl_mode >= SSL_MODE_VERIFY_CA) || (!ca.empty()) ||
        (!capath.empty()) || (!crl.empty()) || (!crlpath.empty())) {
      throw std::invalid_argument(
          "Certificate Verification is disabled in this build of the MySQL "
          "Router. \n"
          "The following parameters are not supported: \n"
          " --ssl-mode=VERIFY_CA, --ssl-mode=VERIFY_IDENTITY, \n"
          " --ssl-ca, --ssl-capath, --ssl-crl, --ssl-crlpath \n"
          "Please check documentation for the details.");
    }
  }

  if (!ssl_cipher.empty() && mysql_options(connection_, MYSQL_OPT_SSL_CIPHER,
                                           ssl_cipher.c_str()) != 0) {
    throw Error(("Error setting SSL_CIPHER option for MySQL connection: " +
                 std::string(mysql_error(connection_)))
                    .c_str(),
                mysql_errno(connection_));
  }

  if (!tls_version.empty() && mysql_options(connection_, MYSQL_OPT_TLS_VERSION,
                                            tls_version.c_str()) != 0) {
    throw Error("Error setting TLS_VERSION option for MySQL connection",
                mysql_errno(connection_));
  }

  if (!ca.empty() &&
      mysql_options(connection_, MYSQL_OPT_SSL_CA, ca.c_str()) != 0) {
    throw Error(("Error setting SSL_CA option for MySQL connection: " +
                 std::string(mysql_error(connection_)))
                    .c_str(),
                mysql_errno(connection_));
  }

  if (!capath.empty() &&
      mysql_options(connection_, MYSQL_OPT_SSL_CAPATH, capath.c_str()) != 0) {
    throw Error(("Error setting SSL_CAPATH option for MySQL connection: " +
                 std::string(mysql_error(connection_)))
                    .c_str(),
                mysql_errno(connection_));
  }

  if (!crl.empty() &&
      mysql_options(connection_, MYSQL_OPT_SSL_CRL, crl.c_str()) != 0) {
    throw Error(("Error setting SSL_CRL option for MySQL connection: " +
                 std::string(mysql_error(connection_)))
                    .c_str(),
                mysql_errno(connection_));
  }

  if (!crlpath.empty() &&
      mysql_options(connection_, MYSQL_OPT_SSL_CRLPATH, crlpath.c_str()) != 0) {
    throw Error(("Error setting SSL_CRLPATH option for MySQL connection: " +
                 std::string(mysql_error(connection_)))
                    .c_str(),
                mysql_errno(connection_));
  }

  // this has to be the last option that gets set due to what appears to be a
  // bug in libmysql causing ssl_mode downgrade from REQUIRED if other options
  // (like tls_version) are also specified
  if (mysql_options(connection_, MYSQL_OPT_SSL_MODE, &ssl_mode) != 0) {
    const char *text = ssl_mode_to_string(ssl_mode);
    std::string msg = std::string("Setting SSL mode to '") + text +
                      "' on connection failed: " + mysql_error(connection_);
    throw Error(msg.c_str(), mysql_errno(connection_));
  }
}

void MySQLSession::set_ssl_cert(const std::string &cert,
                                const std::string &key) {
  if (mysql_options(connection_, MYSQL_OPT_SSL_CERT, cert.c_str()) != 0 ||
      mysql_options(connection_, MYSQL_OPT_SSL_KEY, key.c_str()) != 0) {
    throw Error(("Error setting client SSL certificate for connection: " +
                 std::string(mysql_error(connection_)))
                    .c_str(),
                mysql_errno(connection_));
  }
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
  mysql_options(connection_, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);
  mysql_options(connection_, MYSQL_OPT_READ_TIMEOUT, &read_timeout);

  if (unix_socket.length() > 0) {
#ifdef _WIN32
    protocol = MYSQL_PROTOCOL_PIPE;
#else
    protocol = MYSQL_PROTOCOL_SOCKET;
#endif
  }
  mysql_options(connection_, MYSQL_OPT_PROTOCOL,
                reinterpret_cast<char *>(&protocol));

  const unsigned long client_flags =
      (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
       CLIENT_MULTI_RESULTS);
  std::string tmp_conn_addr = unix_socket.length() > 0
                                  ? unix_socket
                                  : host + ":" + std::to_string(port);
  if (!mysql_real_connect(connection_, host.c_str(), username.c_str(),
                          password.c_str(), default_schema.c_str(), port,
                          unix_socket.c_str(), client_flags)) {
    std::stringstream ss;
    ss << "Error connecting to MySQL server at " << tmp_conn_addr;
    ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_)
       << ")";
    throw Error(ss.str().c_str(), mysql_errno(connection_));
  }
  connected_ = true;
  connection_address_ = tmp_conn_addr;
}

void MySQLSession::disconnect() {
  // close the socket and free internal data
  mysql_close(connection_);

  // initialize the connection handle again as _close() is also free()ing
  // a lot of internal data.
  mysql_init(connection_);
  connected_ = false;
  connection_address_.clear();
}

void MySQLSession::execute(const std::string &q) {
  log_debug("Executing query: %s", log_filter_.filter(q).c_str());
  std::shared_ptr<void> exit_guard(
      nullptr, [](void *) { log_debug("Done executing query"); });

  if (connected_) {
    MOCK_REC_EXECUTE(q);
    if (mysql_real_query(connection_, q.data(), q.length()) != 0) {
      std::stringstream ss;
      ss << "Error executing MySQL query";
      ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_)
         << ")";
      MOCK_REC_ERROR(mysql_error(connection_), mysql_errno(connection_),
                     mysql_sqlstate(connection_), *this);
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
    MYSQL_RES *res = mysql_store_result(connection_);
    MOCK_REC_OK(mysql_insert_id(connection_), mysql_warning_count(connection_));
    if (res) mysql_free_result(res);
  } else
    throw std::logic_error("Not connected");
}

/*
  Execute query on the session and iterate the results with the given callback.

  The processor callback is called with a vector of strings, which conain the
  values of each field of a row. It is called once per row.
  If the processor returns false, the result row iteration stops.
 */
// throws MySQLSession::Error, std::logic_error, whatever processor() throws,
// ...?
void MySQLSession::query(const std::string &q, const RowProcessor &processor) {
  log_debug("Executing query: %s", log_filter_.filter(q).c_str());
  std::shared_ptr<void> exit_guard(
      nullptr, [](void *) { log_debug("Done executing query"); });
  if (connected_) {
    MOCK_REC_QUERY(q);
    if (mysql_real_query(connection_, q.data(), q.length()) != 0) {
      std::stringstream ss;
      ss << "Error executing MySQL query";
      ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_)
         << ")";
      MOCK_REC_ERROR(mysql_error(connection_), mysql_errno(connection_),
                     mysql_sqlstate(connection_), *this);
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
    MYSQL_RES *res = mysql_store_result(connection_);
    if (res) {
      unsigned int nfields = mysql_num_fields(res);
      MOCK_REC_BEGIN(nfields, mysql_fetch_fields(res));
      std::vector<const char *> outrow;
      outrow.resize(nfields);
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res))) {
        MOCK_REC_ROW(row, *this);
        for (unsigned int i = 0; i < nfields; i++) {
          outrow[i] = row[i];
        }
        try {
          if (!processor(outrow)) break;
        } catch (...) {
          mysql_free_result(res);
          throw;
        }
      }
      MOCK_REC_END();
      mysql_free_result(res);
    } else {
      std::stringstream ss;
      ss << "Error fetching query results: ";
      ss << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
      MOCK_REC_ERROR(mysql_error(connection_), mysql_errno(connection_),
                     mysql_sqlstate(connection_), *this);
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
  } else
    throw std::logic_error("Not connected");
}

class RealResultRow : public MySQLSession::ResultRow {
 public:
  RealResultRow(const MySQLSession::Row &row, MYSQL_RES *res) : res_(res) {
    row_ = row;
  }

  virtual ~RealResultRow() { mysql_free_result(res_); }

 private:
  MYSQL_RES *res_;
};

MySQLSession::ResultRow *MySQLSession::query_one(const std::string &q) {
  if (connection_) {
    MOCK_REC_QUERY_ONE(q);
    if (mysql_real_query(connection_, q.data(), q.length()) != 0) {
      std::stringstream ss;
      ss << "Error executing MySQL query";
      ss << ": " << mysql_error(connection_) << " (" << mysql_errno(connection_)
         << ")";
      MOCK_REC_ERROR(mysql_error(connection_), mysql_errno(connection_),
                     mysql_sqlstate(connection_), *this);
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
    MYSQL_RES *res = mysql_store_result(connection_);
    if (res) {
      std::vector<const char *> outrow;
      MYSQL_ROW row;
      unsigned int nfields = mysql_num_fields(res);
      MOCK_REC_BEGIN(nfields, mysql_fetch_fields(res));
      if ((row = mysql_fetch_row(res))) {
        MOCK_REC_ROW(row, *this);
        outrow.resize(nfields);
        for (unsigned int i = 0; i < nfields; i++) {
          outrow[i] = row[i];
        }
      }
      MOCK_REC_END();
      if (outrow.empty()) {
        mysql_free_result(res);
        return nullptr;
      }
      return new RealResultRow(outrow, res);
    } else {
      std::stringstream ss;
      ss << "Error fetching query results: ";
      ss << mysql_error(connection_) << " (" << mysql_errno(connection_) << ")";
      MOCK_REC_ERROR(mysql_error(connection_), mysql_errno(connection_),
                     mysql_sqlstate(connection_), *this);
      throw Error(ss.str().c_str(), mysql_errno(connection_));
    }
  }
  throw Error("Not connected", 0);  // TODO: query() returns std::logic_error()
                                    // in such case, should probably be the same
}

uint64_t MySQLSession::last_insert_id() noexcept {
  return mysql_insert_id(connection_);
}

std::string MySQLSession::quote(const std::string &s, char qchar) noexcept {
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
