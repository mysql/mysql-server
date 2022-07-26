/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <string>
#include <string_view>

#include <mysql.h>

#include "mysql/harness/stdx/expected.h"

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

static inline MysqlError make_mysql_error_code(unsigned int e) {
  return {e, ER_CLIENT(e), "HY000"};
}

static inline MysqlError make_mysql_error_code(MYSQL *m) {
  return {mysql_errno(m), mysql_error(m), mysql_sqlstate(m)};
}

static inline MysqlError make_mysql_error_code(MYSQL_STMT *st) {
  return {mysql_stmt_errno(st), mysql_stmt_error(st), mysql_stmt_sqlstate(st)};
}

template <int S>
constexpr enum_field_types buffer_type_from_type();

template <>
constexpr enum_field_types buffer_type_from_type<8>() {
  return FIELD_TYPE_LONGLONG;
}

template <>
constexpr enum_field_types buffer_type_from_type<4>() {
  return FIELD_TYPE_LONG;
}

template <>
constexpr enum_field_types buffer_type_from_type<2>() {
  return FIELD_TYPE_SHORT;
}

template <>
constexpr enum_field_types buffer_type_from_type<1>() {
  return FIELD_TYPE_TINY;
}

class NullParam : public MYSQL_BIND {
 public:
  constexpr NullParam()
      : MYSQL_BIND{
            nullptr,          // length
            nullptr,          // is_null,
            nullptr,          // buffer
            nullptr,          // error
            nullptr,          // row_ptr
            nullptr,          // store_param_func
            nullptr,          // fetch_result
            nullptr,          // skip_result
            0,                // buffer_length
            0,                // offset
            0,                // length_value
            0,                // param_number
            0,                // pack_length
            FIELD_TYPE_NULL,  // buffer_type
            false,            // error_value
            false,            // is_unsigned
            false,            // long_data_used
            false,            // is_null_value
            nullptr,          // extension
        } {}
};

class StringParam : public MYSQL_BIND {
 public:
  constexpr StringParam(const std::string_view sv)
      : MYSQL_BIND{
            nullptr,                                // length
            nullptr,                                // is_null,
            const_cast<char *>(sv.data()),          // buffer
            nullptr,                                // error
            nullptr,                                // row_ptr
            nullptr,                                // store_param_func
            nullptr,                                // fetch_result
            nullptr,                                // skip_result
            static_cast<unsigned long>(sv.size()),  // buffer_length
            0,                                      // offset
            0,                                      // length_value
            0,                                      // param_number
            0,                                      // pack_length
            FIELD_TYPE_STRING,                      // buffer_type
            false,                                  // error_value
            false,                                  // is_unsigned
            false,                                  // long_data_used
            false,                                  // is_null_value
            nullptr,                                // extension
        } {}

  StringParam(std::string &s, unsigned long *actual_length)
      : MYSQL_BIND{
            actual_length,                         // length
            nullptr,                               // is_null,
            s.empty() ? nullptr : &s.front(),      // buffer
            nullptr,                               // error
            nullptr,                               // row_ptr
            nullptr,                               // store_param_func
            nullptr,                               // fetch_result
            nullptr,                               // skip_result
            static_cast<unsigned long>(s.size()),  // buffer_length
            0,                                     // offset
            0,                                     // length_value
            0,                                     // param_number
            0,                                     // pack_length
            FIELD_TYPE_STRING,                     // buffer_type
            false,                                 // error_value
            false,                                 // is_unsigned
            false,                                 // long_data_used
            false,                                 // is_null_value
            nullptr,                               // extension
        } {}
};

template <class IntType>
class IntegerParam : public MYSQL_BIND {
 public:
  using value_type = IntType;

  constexpr IntegerParam(value_type *v)
      : MYSQL_BIND{
            nullptr,                                      // length
            nullptr,                                      // is_null,
            static_cast<void *>(v),                       // buffer
            nullptr,                                      // error
            nullptr,                                      // row_ptr
            nullptr,                                      // store_param_func
            nullptr,                                      // fetch_result
            nullptr,                                      // skip_result
            sizeof(*v),                                   // buffer_length
            0,                                            // offset
            0,                                            // length_value
            0,                                            // param_number
            0,                                            // pack_length
            buffer_type_from_type<sizeof(value_type)>(),  // buffer_type
            false,                                        // error_value
            std::is_unsigned<value_type>::value,          // is_unsigned
            false,                                        // long_data_used
            false,                                        // is_null_value
            nullptr,                                      // extension
        } {}
};

template <class T>
IntegerParam<T> make_integer_param(T *v) {
  return IntegerParam<T>{v};
}

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

class MysqlClient {
 public:
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

  void username(std::string name) { username_ = std::move(name); }
  void password(std::string pass) { password_ = std::move(pass); }

  void flags(unsigned long f) { flags_ = f; }

  stdx::expected<void, MysqlError> connect(std::string hostname,
                                           uint16_t port = 3306) {
    const auto r =
        mysql_real_connect(m_.get(), hostname.c_str(), username_.c_str(),
                           password_.c_str(), nullptr, port, nullptr, flags_);

    if (r == nullptr) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<void, MysqlError> reset_connection() {
    const auto r = mysql_reset_connection(m_.get());

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<void, MysqlError> change_user(const std::string &username,
                                               const std::string &password,
                                               const std::string &schema) {
    const auto r = mysql_change_user(m_.get(), username.c_str(),
                                     password.c_str(), schema.c_str());

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<void, MysqlError> ping() {
    const auto r = mysql_ping(m_.get());

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<void, MysqlError> refresh() {
    const auto r = mysql_refresh(m_.get(), 0);

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<void, MysqlError> reload() {
    const auto r = mysql_reload(m_.get());

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<void, MysqlError> shutdown() {
    const auto r = mysql_shutdown(m_.get(), SHUTDOWN_DEFAULT);

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<void, MysqlError> kill(uint32_t id) {
    const auto r = mysql_kill(m_.get(), id);

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<std::string, MysqlError> stat() {
    const auto r = mysql_stat(m_.get());

    if (r == nullptr) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {r};
  }

  /**
   * set a mysql option.
   *
   * @code
   * auto res = set_option(ConnectTimeout(10));
   * @endcode
   *
   * @note on error the MysqlError may not always contain the right
   * error-code.
   *
   * @param [in] opt option to set.
   * @returns a MysqlError on error
   * @retval true on success
   */
  template <class SettableMysqlOption>
  stdx::expected<void, MysqlError> set_option(const SettableMysqlOption &opt) {
    auto m = m_.get();
    if (0 != mysql_options(m, opt.option(), opt.data())) {
      return stdx::make_unexpected(make_mysql_error_code(m));
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
    auto m = m_.get();
    if (0 != mysql_get_option(m, opt.option(), opt.data())) {
      return false;
    }

    return true;
  }

  class Statement {
   public:
    Statement(MYSQL *m) : m_{m} {}

    class Rows {
     public:
      Rows(MYSQL *m) : res_{mysql_use_result(m)} {}
      Rows(MYSQL_RES *res) : res_{res} {}

      Rows(const Rows &) = delete;
      Rows(Rows &&other) : res_{std::exchange(other.res_, nullptr)} {}
      Rows &operator=(const Rows &) = delete;
      Rows &operator=(Rows &&other) {
        res_ = std::exchange(other.res_, nullptr);

        return *this;
      }

      ~Rows() {
        if (res_) {
          // drain the rows that may still be in flight.
          while (mysql_fetch_row(res_))
            ;
          mysql_free_result(res_);
        }
      }

      class Iterator {
       public:
        using value_type = MYSQL_ROW;

        Iterator(MYSQL_RES *res) : res_{res} {
          if (res_ == nullptr) return;

          current_row_ = mysql_fetch_row(res_);
          if (current_row_ == nullptr) {
            res_ = nullptr;
          }
        }
        Iterator &operator++() {
          current_row_ = mysql_fetch_row(res_);
          if (current_row_ == nullptr) {
            res_ = nullptr;
          }

          return *this;
        }
        bool operator!=(const Iterator &other) { return res_ != other.res_; }

        value_type operator*() { return current_row_; }

       private:
        MYSQL_RES *res_;

        value_type current_row_;
      };

      using iterator = Iterator;
      iterator begin() { return {res_}; }
      iterator end() { return {nullptr}; }

     private:
      MYSQL_RES *res_;
    };

    class ResultSet {
     public:
      ResultSet(MYSQL *m) : m_{m} {}

      Rows rows() const { return m_; }

      unsigned int field_count() const { return mysql_field_count(m_); }

      uint64_t affected_rows() const { return mysql_affected_rows(m_); }
      uint64_t insert_id() const { return mysql_insert_id(m_); }

     private:
      MYSQL *m_;
    };

    class Result {
     public:
      class Iterator {
       public:
        using reference = ResultSet;

        Iterator(MYSQL *m) : m_{m} {}
        Iterator &operator++() {
          if (0 != mysql_next_result(m_)) {
            m_ = nullptr;
          }

          return *this;
        }
        bool operator!=(const Iterator &other) { return m_ != other.m_; }

        reference operator*() { return {m_}; }

       private:
        MYSQL *m_;
      };

      using iterator = Iterator;

      Result(MYSQL *m) : m_{m} {}

      iterator begin() { return {m_}; }
      iterator end() { return {nullptr}; }

     private:
      MYSQL *m_;
    };

    template <class T, class N>
    typename std::enable_if<
        std::conjunction<std::is_same<typename T::value_type, MYSQL_BIND>,
                         std::is_same<decltype(std::declval<T>().data()),
                                      typename T::value_type *>,
                         std::is_same<typename N::value_type, const char *>,
                         std::is_same<decltype(std::declval<N>().data()),
                                      typename N::value_type *>>::value,
        stdx::expected<void, MysqlError>>::type
    bind_params(const T &params, const N &names) {
      auto r = mysql_bind_param(m_, params.size(),
                                const_cast<MYSQL_BIND *>(params.data()),
                                const_cast<const char **>(names.data()));

      if (r != 0) {
        return stdx::make_unexpected(make_mysql_error_code(m_));
      }

      return {};
    }

    stdx::expected<Result, MysqlError> query(const std::string &stmt) {
      auto r = mysql_real_query(m_, stmt.c_str(), stmt.size());

      if (r != 0) {
        return stdx::make_unexpected(make_mysql_error_code(m_));
      }

      return {m_};
    }

    uint32_t field_count() const { return mysql_field_count(m_); }

   private:
    MYSQL *m_;
  };

  stdx::expected<Statement::Result, MysqlError> query(const std::string &stmt) {
    return Statement(m_.get()).query(stmt);
  }

  stdx::expected<void, MysqlError> use_schema(const std::string &schema) {
    const auto r = mysql_select_db(m_.get(), schema.c_str());

    if (r != 0) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {};
  }

  stdx::expected<Statement::Rows, MysqlError> list_dbs() {
    const auto res = mysql_list_dbs(m_.get(), nullptr);

    if (res == nullptr) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {res};
  }

  stdx::expected<Statement::Result, MysqlError> list_fields(
      std::string tablename) {
    const auto res = mysql_list_fields(m_.get(), tablename.c_str(), nullptr);

    if (res == nullptr) {
      return stdx::make_unexpected(make_mysql_error_code(m_.get()));
    }

    return {m_.get()};
  }

  template <class T, class N>
  typename std::enable_if<
      std::conjunction<std::is_same<typename T::value_type, MYSQL_BIND>,
                       std::is_same<decltype(std::declval<T>().data()),
                                    typename T::value_type *>,
                       std::is_same<typename N::value_type, const char *>,
                       std::is_same<decltype(std::declval<N>().data()),
                                    typename N::value_type *>>::value,
      stdx::expected<Statement::Result, MysqlError>>::type
  query(const std::string &stmt, const T &params, const N &names) {
    Statement st(m_.get());

    const auto bind_res = st.bind_params(params, names);
    if (!bind_res) return bind_res.get_unexpected();

    return st.query(stmt);
  }

  class PreparedStatement {
   public:
    PreparedStatement(MYSQL *m) : st_{mysql_stmt_init(m)} {}

    template <enum_stmt_attr_type T, class V>
    class IntegerAttribute {
     public:
      using value_type = V;

      IntegerAttribute(value_type v) : v_{v} {}

      enum_stmt_attr_type type() const { return T; }

      void *data() { return &v_; }

     private:
      value_type v_;
    };

    using UpdateMaxLength = IntegerAttribute<STMT_ATTR_UPDATE_MAX_LENGTH, bool>;
    using CursorType = IntegerAttribute<STMT_ATTR_CURSOR_TYPE, unsigned long>;
    using PrefetchRows =
        IntegerAttribute<STMT_ATTR_PREFETCH_ROWS, unsigned long>;

    template <class SettableAttribute>
    stdx::expected<void, MysqlError> set_attr(SettableAttribute attr) {
      const auto r = mysql_stmt_attr_set(st_.get(), attr.type(), attr.data());

      if (r != 0) {
        return stdx::make_unexpected(make_mysql_error_code(st_.get()));
      }

      return {};
    }

    stdx::expected<void, MysqlError> prepare(const std::string &stmt) {
      auto r = mysql_stmt_prepare(st_.get(), stmt.c_str(), stmt.size());

      if (r != 0) {
        return stdx::make_unexpected(make_mysql_error_code(st_.get()));
      }
      return {};
    }

    uint32_t param_count() const {
      auto *stmt = st_.get();
      return mysql_stmt_param_count(stmt);
    }

    template <class T>
    typename std::enable_if<
        std::conjunction<std::is_same<typename T::value_type, MYSQL_BIND>,
                         std::is_same<decltype(std::declval<T>().data()),
                                      typename T::value_type *>>::value,
        stdx::expected<void, MysqlError>>::type
    bind_params(const T &params) {
      auto *stmt = st_.get();

      if (params.size() != param_count()) {
        return stdx::make_unexpected(make_mysql_error_code(1));
      }

      auto r =
          mysql_stmt_bind_param(stmt, const_cast<MYSQL_BIND *>(params.data()));

      if (r != 0) {
        return stdx::make_unexpected(make_mysql_error_code(stmt));
      }

      return {};
    }

    stdx::expected<void, MysqlError> append_param_data(unsigned int param_num,
                                                       std::string_view data) {
      return append_param_data(param_num, data.data(), data.size());
    }

    stdx::expected<void, MysqlError> append_param_data(unsigned int param_num,
                                                       const char *data,
                                                       unsigned long data_len) {
      auto *stmt = st_.get();

      auto res = mysql_stmt_send_long_data(stmt, param_num, data, data_len);
      if (res != 0) return stdx::make_unexpected(make_mysql_error_code(stmt));

      return {};
    }

    class FetchStatus {
     public:
      FetchStatus(int status) : status_{status} {}

      int status() const { return status_; }

     private:
      int status_;
    };

    class Rows {
     public:
      Rows(MYSQL_STMT *st) : st_{st} {}

      class Iterator {
       public:
        using value_type = FetchStatus;

        Iterator(MYSQL_STMT *st) : st_{st} {
          if (st_ == nullptr) return;

          fetch_res_ = mysql_stmt_fetch(st_);
          if (fetch_res_.status() == 1 ||
              fetch_res_.status() == MYSQL_NO_DATA) {
            st_ = nullptr;
          }
        }
        Iterator &operator++() {
          fetch_res_ = mysql_stmt_fetch(st_);
          if (fetch_res_.status() == 1 ||
              fetch_res_.status() == MYSQL_NO_DATA) {
            st_ = nullptr;
          }

          return *this;
        }
        bool operator!=(const Iterator &other) { return st_ != other.st_; }

        value_type operator*() { return fetch_res_; }

       private:
        MYSQL_STMT *st_;

        FetchStatus fetch_res_{0};
      };

      using iterator = Iterator;
      iterator begin() { return {st_}; }
      iterator end() { return {nullptr}; }

     private:
      MYSQL_STMT *st_;
    };

    class ResultSet {
     public:
      ResultSet(MYSQL_STMT *st) : st_{st} {}

      ResultSet(const ResultSet &) = delete;
      ResultSet(ResultSet &&other) : st_{std::exchange(other.st_, nullptr)} {}
      ResultSet &operator=(const ResultSet &) = delete;
      ResultSet &operator=(ResultSet &&other) {
        st_ = std::exchange(other.st_, nullptr);

        return *this;
      }

      ~ResultSet() { mysql_stmt_free_result(st_); }

      Rows rows() const { return {st_}; }

      // fetch one row.
      stdx::expected<void, MysqlError> fetch() const {
        const auto r = mysql_stmt_fetch(st_);

        if (r == 1) {
          return stdx::make_unexpected(make_mysql_error_code(st_));
        }
        return {};
      }

      template <class T>
      typename std::enable_if<
          std::conjunction<std::is_same<typename T::value_type, MYSQL_BIND>,
                           std::is_same<decltype(std::declval<T>().data()),
                                        typename T::value_type *>>::value,
          stdx::expected<void, MysqlError>>::type
      bind_result(T &params) {
        if (params.size() != field_count()) {
          return stdx::make_unexpected(make_mysql_error_code(1));
        }

        auto r = mysql_stmt_bind_result(st_, params.data());
        if (r != 0) {
          return stdx::make_unexpected(make_mysql_error_code(st_));
        }

        return {};
      }

      unsigned int field_count() const { return mysql_stmt_field_count(st_); }

      uint64_t affected_rows() const { return mysql_stmt_affected_rows(st_); }
      uint64_t insert_id() const { return mysql_stmt_insert_id(st_); }

     private:
      MYSQL_STMT *st_;
    };

    class Result {
     public:
      class Iterator {
       public:
        using reference = ResultSet;

        Iterator(MYSQL_STMT *st) : st_{st} {}
        Iterator &operator++() {
          if (0 != mysql_stmt_next_result(st_)) {
            st_ = nullptr;
          }

          return *this;
        }
        bool operator!=(const Iterator &other) { return st_ != other.st_; }

        reference operator*() { return {st_}; }

       private:
        MYSQL_STMT *st_;
      };

      using iterator = Iterator;

      Result(MYSQL_STMT *st) : st_{st} {}

      iterator begin() { return {st_}; }
      iterator end() { return {nullptr}; }

     private:
      MYSQL_STMT *st_;
    };

    stdx::expected<Result, MysqlError> execute() {
      auto r = mysql_stmt_execute(st_.get());

      if (r != 0) {
        return stdx::make_unexpected(make_mysql_error_code(st_.get()));
      }

      return {st_.get()};
    }

    stdx::expected<void, MysqlError> reset() {
      auto r = mysql_stmt_reset(st_.get());

      if (r != 0) {
        return stdx::make_unexpected(make_mysql_error_code(st_.get()));
      }
      return {};
    }

    class StmtDeleter {
     public:
      void operator()(MYSQL_STMT *st) { mysql_stmt_close(st); }
    };

    std::unique_ptr<MYSQL_STMT, StmtDeleter> st_{mysql_stmt_init(nullptr)};
  };

  stdx::expected<PreparedStatement, MysqlError> prepare(
      const std::string &stmt) {
    PreparedStatement st(m_.get());

    auto res = st.prepare(stmt);

    if (!res) return res.get_unexpected();

    return st;
  }

 private:
  std::string username_;
  std::string password_;

  class MysqlDeleter {
   public:
    void operator()(MYSQL *m) {
      m->free_me = true;
      mysql_close(m);
    }
  };

  std::unique_ptr<MYSQL, MysqlDeleter> m_{mysql_init(nullptr)};

  unsigned long flags_{};
};
