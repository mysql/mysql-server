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

#ifndef _ROUTER_MYSQL_SESSION_H_
#define _ROUTER_MYSQL_SESSION_H_

#include "mysqlrouter/router_export.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <mysql.h>  // enum mysql_ssl_mode

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/log_filter.h"

namespace mysqlrouter {

class MysqlError {
 public:
  MysqlError() = default;
  MysqlError(unsigned int code, std::string message, std::string sql_state)
      : code_{code},
        message_{std::move(message)},
        sql_state_{std::move(sql_state)} {}

  operator bool() { return code_ != 0; }

  std::string message() const { return message_; }
  std::string sql_state() const { return sql_state_; }
  unsigned int value() const { return code_; }

 private:
  unsigned int code_{0};
  std::string message_;
  std::string sql_state_;
};

namespace impl {
/**
 * gettable, settable option for mysql_option's.
 *
 * adapts scalar types like int/bool/... mysql_option's to
 * mysql_options()/mysql_get_option().
 *
 * - mysql_options() expects a '&int'
 * - mysql_get_option() expects a '&int'
 */
template <mysql_option Opt, class ValueType>
class Option {
 public:
  using value_type = ValueType;

  constexpr Option() = default;
  constexpr explicit Option(value_type v) : v_{std::move(v)} {}

  // get the option id
  constexpr mysql_option option() const noexcept { return Opt; }

  // get address of the storage.
  constexpr const void *data() const { return std::addressof(v_); }

  // get address of the storage.
  constexpr void *data() { return std::addressof(v_); }

  // set the value of the option
  constexpr void value(value_type v) { v_ = v; }

  // get the value of the option
  constexpr value_type value() const { return v_; }

 private:
  value_type v_{};
};

/**
 * gettable, settable option for 'const char *' based mysql_option's.
 *
 * adapts 'const char *' based mysql_option to
 * mysql_options()/mysql_get_option().
 *
 * - mysql_options() expects a 'const char *'
 * - mysql_get_option() expects a '&(const char *)'
 */
template <mysql_option Opt>
class Option<Opt, const char *> {
 public:
  using value_type = const char *;

  Option() = default;
  constexpr explicit Option(value_type v) : v_{std::move(v)} {}

  constexpr mysql_option option() const noexcept { return Opt; }

  constexpr const void *data() const { return v_; }

  constexpr void *data() { return std::addressof(v_); }

  constexpr void value(value_type v) { v_ = v; }

  constexpr value_type value() const { return v_; }

 private:
  value_type v_{};
};

template <mysql_option Opt>
class Option<Opt, std::nullptr_t> {
 public:
  using value_type = std::nullptr_t;

  Option() = default;
  // accept a void *, but ignore it.
  constexpr explicit Option(value_type) {}

  constexpr mysql_option option() const noexcept { return Opt; }

  constexpr const void *data() const { return nullptr; }

  constexpr void *data() { return nullptr; }

  constexpr value_type value() const { return nullptr; }
};
}  // namespace impl

// mysql_options() may be used with MYSQL * == nullptr to get global values.

class ROUTER_LIB_EXPORT MySQLSession {
 public:
  static constexpr int kDefaultConnectTimeout = 5;
  static constexpr int kDefaultReadTimeout = 30;
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

  //
  // mysql_option's
  //
  // (sorted by appearance in documentation)

  // type for mysql_option's which set/get a 'bool'
  template <mysql_option Opt>
  using BooleanOption = impl::Option<Opt, bool>;

  // type for mysql_option's which set/get a 'unsigned int'
  template <mysql_option Opt>
  using IntegerOption = impl::Option<Opt, unsigned int>;

  // type for mysql_option's which set/get a 'unsigned long'
  template <mysql_option Opt>
  using LongOption = impl::Option<Opt, unsigned long>;

  // type for mysql_option's which set/get a 'const char *'
  template <mysql_option Opt>
  using ConstCharOption = impl::Option<Opt, const char *>;

  using DefaultAuthentication = ConstCharOption<MYSQL_DEFAULT_AUTH>;
  using EnableCleartextPlugin = BooleanOption<MYSQL_ENABLE_CLEARTEXT_PLUGIN>;
  using InitCommand = ConstCharOption<MYSQL_INIT_COMMAND>;
  using BindAddress = ConstCharOption<MYSQL_OPT_BIND>;
  using CanHandleExpiredPasswords =
      BooleanOption<MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS>;
  using Compress = BooleanOption<MYSQL_OPT_COMPRESS>;
  using CompressionAlgorithms =
      ConstCharOption<MYSQL_OPT_COMPRESSION_ALGORITHMS>;
  using ConnectAttributeReset = BooleanOption<MYSQL_OPT_CONNECT_ATTR_RESET>;
  using ConnectAttributeDelete = BooleanOption<MYSQL_OPT_CONNECT_ATTR_DELETE>;
  using ConnectTimeout = IntegerOption<MYSQL_OPT_CONNECT_TIMEOUT>;
  using GetServerPublicKey = BooleanOption<MYSQL_OPT_GET_SERVER_PUBLIC_KEY>;
  using LoadDataLocalDir = ConstCharOption<MYSQL_OPT_LOAD_DATA_LOCAL_DIR>;
  using LocalInfile = IntegerOption<MYSQL_OPT_LOCAL_INFILE>;
  using MaxAllowedPacket = LongOption<MYSQL_OPT_MAX_ALLOWED_PACKET>;
  using NamedPipe = BooleanOption<MYSQL_OPT_NAMED_PIPE>;
  using NetBufferLength = LongOption<MYSQL_OPT_NET_BUFFER_LENGTH>;
  using OptionalResultsetMetadata =
      BooleanOption<MYSQL_OPT_OPTIONAL_RESULTSET_METADATA>;
  // TCP/UnixSocket/...
  using Protocol = IntegerOption<MYSQL_OPT_PROTOCOL>;
  using ReadTimeout = IntegerOption<MYSQL_OPT_READ_TIMEOUT>;
  using Reconnect = BooleanOption<MYSQL_OPT_RECONNECT>;
  using RetryCount = IntegerOption<MYSQL_OPT_RETRY_COUNT>;
  using SslCa = ConstCharOption<MYSQL_OPT_SSL_CA>;
  using SslCaPath = ConstCharOption<MYSQL_OPT_SSL_CAPATH>;
  using SslCert = ConstCharOption<MYSQL_OPT_SSL_CERT>;
  using SslCipher = ConstCharOption<MYSQL_OPT_SSL_CIPHER>;
  using SslCrl = ConstCharOption<MYSQL_OPT_SSL_CRL>;
  using SslCrlPath = ConstCharOption<MYSQL_OPT_SSL_CRLPATH>;
  using SslFipsMode = IntegerOption<MYSQL_OPT_SSL_FIPS_MODE>;
  using SslKey = ConstCharOption<MYSQL_OPT_SSL_KEY>;
  using SslMode = IntegerOption<MYSQL_OPT_SSL_MODE>;
  using TlsCipherSuites = ConstCharOption<MYSQL_OPT_TLS_CIPHERSUITES>;
  using TlsVersion = ConstCharOption<MYSQL_OPT_TLS_VERSION>;
  using WriteTimeout = IntegerOption<MYSQL_OPT_WRITE_TIMEOUT>;
  using ZstdCompressionLevel = IntegerOption<MYSQL_OPT_ZSTD_COMPRESSION_LEVEL>;

  using PluginDir = ConstCharOption<MYSQL_PLUGIN_DIR>;
  using ReportDataTruncation = BooleanOption<MYSQL_REPORT_DATA_TRUNCATION>;
  using ServerPluginKey = ConstCharOption<MYSQL_SERVER_PUBLIC_KEY>;
  using ReadDefaultFile = ConstCharOption<MYSQL_READ_DEFAULT_FILE>;
  using ReadDefaultGroup = ConstCharOption<MYSQL_READ_DEFAULT_GROUP>;
  using CharsetDir = ConstCharOption<MYSQL_SET_CHARSET_DIR>;
  using CharsetName = ConstCharOption<MYSQL_SET_CHARSET_NAME>;
  using SharedMemoryBasename = ConstCharOption<MYSQL_SHARED_MEMORY_BASE_NAME>;

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
    virtual ~ResultRow() = default;
    size_t size() const { return row_.size(); }
    const char *&operator[](size_t i) { return row_[i]; }

   private:
    Row row_;
  };

  struct ROUTER_LIB_EXPORT LoggingStrategy {
    LoggingStrategy() = default;

    LoggingStrategy(const LoggingStrategy &) = default;
    LoggingStrategy(LoggingStrategy &&) = default;

    LoggingStrategy &operator=(const LoggingStrategy &) = default;
    LoggingStrategy &operator=(LoggingStrategy &&) = default;

    virtual ~LoggingStrategy() = default;

    virtual void log(const std::string &msg) = 0;
  };

  struct ROUTER_LIB_EXPORT LoggingStrategyNone : public LoggingStrategy {
    virtual void log(const std::string & /*msg*/) override {}
  };

  struct ROUTER_LIB_EXPORT LoggingStrategyDebugLogger : public LoggingStrategy {
    virtual void log(const std::string &msg) override;
  };

  MySQLSession(std::unique_ptr<LoggingStrategy> logging_strategy =
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

  /**
   * set a mysql option.
   *
   * @code
   * auto res = set_option(ConnectTimeout(10));
   * @endcode
   *
   * @note on error the MysqlError may not always contain the right error-code.
   *
   * @param [in] opt option to set.
   * @returns a MysqlError on error
   * @retval true on success
   */
  template <class SettableMysqlOption>
  stdx::expected<void, MysqlError> set_option(const SettableMysqlOption &opt) {
    if (0 != mysql_options(connection_, opt.option(), opt.data())) {
      return stdx::make_unexpected(MysqlError(mysql_errno(connection_),
                                              mysql_error(connection_),
                                              mysql_sqlstate(connection_)));
    }

    return {};
  }

  /**
   * get a mysql option.
   *
   * @code
   * ConnectTimeout opt_connect_timeout;
   * auto res = get_option(opt_connect_timeout);
   * if (res) {
   *   std::cerr << opt_connect_timeout.value() << std::endl;
   * }
   * @endcode
   *
   * @param [in,out] opt option to query.
   * @retval true on success.
   * @retval false if option is not known.
   */
  template <class GettableMysqlOption>
  bool get_option(GettableMysqlOption &opt) {
    if (0 != mysql_get_option(connection_, opt.option(), opt.data())) {
      return false;
    }

    return true;
  }

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
      const FieldValidator &validator);  // throws Error, std::logic_error
  virtual std::unique_ptr<MySQLSession::ResultRow> query_one(
      const std::string &query,
      const FieldValidator &validator);  // throws Error
                                         //
  void query(const std::string &stmt, const RowProcessor &processor) {
    return query(stmt, processor, [](unsigned, MYSQL_FIELD *) {});
  }

  std::unique_ptr<MySQLSession::ResultRow> query_one(const std::string &stmt) {
    return query_one(stmt, [](unsigned, MYSQL_FIELD *) {});
  }

  virtual uint64_t last_insert_id() noexcept;

  virtual unsigned warning_count() noexcept;

  virtual std::string quote(const std::string &s, char qchar = '\'') const;

  virtual bool is_connected() noexcept { return connection_ && connected_; }
  const std::string &get_address() noexcept { return connection_address_; }

  virtual const char *last_error();
  virtual unsigned int last_errno();

  virtual const char *ssl_cipher();

 protected:
  std::unique_ptr<LoggingStrategy> logging_strategy_;

 private:
  ConnectionParameters conn_params_;

  MYSQL *connection_;
  bool connected_;
  std::string connection_address_;
  SQLLogFilter log_filter_;

  virtual MYSQL *raw_mysql() noexcept { return connection_; }

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
   * @returns resultset on success, MysqlError on error
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
