/*
  Copyright (c) 2016, 2020, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _ROUTER_MYSQL_SESSION_H_
#define _ROUTER_MYSQL_SESSION_H_

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <mysql.h>  // enum mysql_ssl_mode

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/log_filter.h"
#ifdef FRIEND_TEST
class MockMySQLSession;
#endif

namespace mysqlrouter {

class MysqlError {
 public:
  MysqlError(unsigned int code, std::string message, std::string sql_state)
      : code_{code},
        message_{std::move(message)},
        sql_state_{std::move(sql_state)} {}

  operator bool() { return code_ != 0; }

  std::string message() const { return message_; }
  std::string sql_state() const { return sql_state_; }
  unsigned int value() const { return code_; }

 private:
  unsigned int code_;
  std::string message_;
  std::string sql_state_;
};

class MySQLSession {
 public:
  static const int kDefaultConnectTimeout = 15;
  static const int kDefaultReadTimeout = 30;
  typedef std::vector<const char *> Row;
  typedef std::function<bool(const Row &)> RowProcessor;
  typedef std::function<void(unsigned, MYSQL_FIELD *)> FieldValidator;

  // text representations of SSL modes
  static const char kSslModeDisabled[];
  static const char kSslModePreferred[];
  static const char kSslModeRequired[];
  static const char kSslModeVerifyCa[];
  static const char kSslModeVerifyIdentity[];

  // this struct contains all parameters which would be needed if you wanted to
  // create a new connection with same parameters (server address, options,
  // etc)
  struct ConnectionParameters {
    struct SslOptions {
      mysql_ssl_mode ssl_mode;
      std::string tls_version;
      std::string ssl_cipher;
      std::string ca;
      std::string capath;
      std::string crl;
      std::string crlpath;
    } ssl_opts;
    struct SslCert {
      std::string cert;
      std::string key;
    } ssl_cert;
    struct ConnOptions {
      std::string host;
      unsigned int port;
      std::string username;
      std::string password;
      std::string unix_socket;
      std::string default_schema;
      int connect_timeout;
      int read_timeout;
    } conn_opts;
  };

  class Transaction {
   public:
    Transaction(MySQLSession *session) : session_(session) {
      session_->execute("START TRANSACTION");
    }

    ~Transaction() {
      if (session_) {
        try {
          session_->execute("ROLLBACK");
        } catch (...) {
          // ignore errors during rollback on d-tor
        }
      }
    }

    void commit() {
      session_->execute("COMMIT");
      session_ = nullptr;
    }

    void rollback() {
      session_->execute("ROLLBACK");
      session_ = nullptr;
    }

   private:
    MySQLSession *session_;
  };

  class Error : public std::runtime_error {
   public:
    // NOTE Not all calls to constructors provide the 3rd argument.  To save
    //      time, only the code where it was needed atm was upgraded from 2 to
    //      3 args; upgrade elsewhere if needed

    Error(const char *error, unsigned int code,
          const std::string message = "<not set>")
        : std::runtime_error(error), code_(code), message_(message) {}

    Error(const std::string &error, unsigned int code,
          const std::string &message = "<not set>")
        : std::runtime_error(error), code_(code), message_(message) {}

    unsigned int code() const { return code_; }
    std::string message() const { return message_; }

   private:
    const unsigned int code_;
    const std::string message_;
  };

  class ResultRow {
   public:
    ResultRow(Row row) : row_{std::move(row)} {}
    virtual ~ResultRow() {}
    size_t size() const { return row_.size(); }
    const char *&operator[](size_t i) { return row_[i]; }

   private:
    Row row_;
  };

  struct LoggingStrategy {
    virtual void log(const std::string &msg) = 0;
    virtual ~LoggingStrategy() = default;
  };

  struct LoggingStrategyNone : public LoggingStrategy {
    virtual void log(const std::string & /*msg*/) override {}
  };

  struct LoggingStrategyDebugLogger : public LoggingStrategy {
    virtual void log(const std::string &msg) override;
  };

  MySQLSession(std::unique_ptr<LoggingStrategy> &&logging_strategy =
                   std::make_unique<LoggingStrategyNone>());
  virtual ~MySQLSession();

  static mysql_ssl_mode parse_ssl_mode(
      std::string ssl_mode);  // throws std::logic_error
  static const char *ssl_mode_to_string(mysql_ssl_mode ssl_mode) noexcept;

  // throws Error, std::invalid_argument
  virtual void set_ssl_options(mysql_ssl_mode ssl_mode,
                               const std::string &tls_version,
                               const std::string &ssl_cipher,
                               const std::string &ca, const std::string &capath,
                               const std::string &crl,
                               const std::string &crlpath);

  // throws Error
  virtual void set_ssl_cert(const std::string &cert, const std::string &key);

  virtual void connect(const std::string &host, unsigned int port,
                       const std::string &username, const std::string &password,
                       const std::string &unix_socket,
                       const std::string &default_schema,
                       int connect_timeout = kDefaultConnectTimeout,
                       int read_timeout = kDefaultReadTimeout);  // throws Error
  virtual void disconnect();

  /**
   * This is an alternative way to initialise a new connection.  It calls
   * connect() and several other methods under the hood.  Along with its
   * counterpart `get_connection_parameters()`, it's useful for spawning
   * new connections using an existing connection as a template.
   *
   * @param conn_params Connection parameters
   *
   * @see get_connection_parameters()
   */
  virtual void connect_and_set_opts(const ConnectionParameters &conn_params);

  /**
   * Returns connection parameters which could be used as a template for
   * spawning new connections.
   *
   * @see connect_and_set_opts()
   *
   * @returns parameters used to create current connection
   */
  virtual ConnectionParameters get_connection_parameters() {
    return conn_params_;
  }

  virtual void execute(
      const std::string &query);  // throws Error, std::logic_error
  virtual void query(
      const std::string &query, const RowProcessor &processor,
      const FieldValidator &validator =
          null_field_validator);  // throws Error, std::logic_error
  virtual std::unique_ptr<MySQLSession::ResultRow> query_one(
      const std::string &query,
      const FieldValidator &validator = null_field_validator);  // throws Error

  virtual uint64_t last_insert_id() noexcept;

  virtual unsigned warning_count() noexcept;

  virtual std::string quote(const std::string &s, char qchar = '\'') noexcept;

  virtual bool is_connected() noexcept { return connection_ && connected_; }
  const std::string &get_address() noexcept { return connection_address_; }

  virtual const char *last_error();
  virtual unsigned int last_errno();

  virtual const char *ssl_cipher();

 protected:
  static const std::function<void(unsigned, MYSQL_FIELD *)>
      null_field_validator;
  std::unique_ptr<LoggingStrategy> logging_strategy_;

 private:
  ConnectionParameters conn_params_;

  MYSQL *connection_;
  bool connected_;
  std::string connection_address_;
  SQLLogFilter log_filter_;

  virtual MYSQL *raw_mysql() noexcept { return connection_; }

#ifdef FRIEND_TEST
  friend class ::MockMySQLSession;
#endif

  class MYSQL_RES_Deleter {
   public:
    void operator()(MYSQL_RES *res) { mysql_free_result(res); }
  };

  using mysql_result_type = std::unique_ptr<MYSQL_RES, MYSQL_RES_Deleter>;

  /**
   * run query.
   *
   * There are 3 cases:
   *
   * 1. query returns a resultset
   * 3. query returns no resultset
   * 2. query fails with an error
   *
   * @param q stmt to execute
   *
   * @returns resultset on sucess, MysqlError on error
   */
  stdx::expected<mysql_result_type, MysqlError> real_query(
      const std::string &q);

  /**
   * log query before running it.
   */
  stdx::expected<mysql_result_type, MysqlError> logged_real_query(
      const std::string &q);
};

}  // namespace mysqlrouter

#endif
