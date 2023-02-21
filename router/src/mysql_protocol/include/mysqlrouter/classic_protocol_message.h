/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_MESSAGE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_MESSAGE_H_

#include <cstddef>  // uint8_t
#include <optional>
#include <string>
#include <vector>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/flags.h"
#include "mysqlrouter/classic_protocol_constants.h"

namespace classic_protocol::message::client::impl {
class BinlogDump {
 public:
  // flags of message::client::BinlogDump
  enum class Flags : uint16_t {
    non_blocking = 1 << 0,
  };
};
}  // namespace classic_protocol::message::client::impl

namespace stdx {
// enable flag-ops for BinlogDump::Flags
template <>
struct is_flags<classic_protocol::message::client::impl::BinlogDump::Flags>
    : std::true_type {};
}  // namespace stdx
   //
namespace classic_protocol::message::client::impl {
class BinlogDumpGtid {
 public:
  // flags of message::client::BinlogDumpGtid
  enum class Flags : uint16_t {
    non_blocking = 1 << 0,
    through_position = 1 << 1,
    through_gtid = 1 << 2,
  };
};
}  // namespace classic_protocol::message::client::impl

namespace stdx {
// enable flag-ops for BinlogDumpGtid::Flags
template <>
struct is_flags<classic_protocol::message::client::impl::BinlogDumpGtid::Flags>
    : std::true_type {};
}  // namespace stdx

namespace classic_protocol {

namespace borrowable {
/**
 * AuthMethod of classic protocol.
 *
 * classic proto supports negotiating the auth-method via capabilities and
 * auth-method names.
 */
template <bool Borrowed>
class AuthMethod {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  AuthMethod(classic_protocol::capabilities::value_type capabilities,
             string_type auth_method_name)
      : capabilities_{capabilities},
        auth_method_name_{std::move(auth_method_name)} {}

  constexpr string_type name() const {
    if (auth_method_name_.empty() &&
        !capabilities_[classic_protocol::capabilities::pos::plugin_auth]) {
      if (capabilities_
              [classic_protocol::capabilities::pos::secure_connection]) {
        return "mysql_native_password";
      } else {
        return "old_password";
      }
    }

    return auth_method_name_;
  }

 private:
  const classic_protocol::capabilities::value_type capabilities_;
  const string_type auth_method_name_;
};

namespace message {

namespace server {

template <bool Borrowed>
class Greeting {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr Greeting(uint8_t protocol_version, string_type version,
                     uint32_t connection_id, string_type auth_method_data,
                     classic_protocol::capabilities::value_type capabilities,
                     uint8_t collation,
                     classic_protocol::status::value_type status_flags,
                     string_type auth_method_name)
      : protocol_version_{protocol_version},
        version_{std::move(version)},
        connection_id_{connection_id},
        auth_method_data_{std::move(auth_method_data)},
        capabilities_{capabilities},
        collation_{collation},
        status_flags_{status_flags},
        auth_method_name_{std::move(auth_method_name)} {}

  constexpr uint8_t protocol_version() const noexcept {
    return protocol_version_;
  }
  constexpr string_type version() const { return version_; }
  constexpr string_type auth_method_name() const { return auth_method_name_; }
  constexpr string_type auth_method_data() const { return auth_method_data_; }
  classic_protocol::capabilities::value_type capabilities() const noexcept {
    return capabilities_;
  }

  void capabilities(classic_protocol::capabilities::value_type caps) {
    capabilities_ = caps;
  }

  constexpr uint8_t collation() const noexcept { return collation_; }
  constexpr classic_protocol::status::value_type status_flags() const noexcept {
    return status_flags_;
  }
  constexpr uint32_t connection_id() const noexcept { return connection_id_; }

 private:
  uint8_t protocol_version_;
  string_type version_;
  uint32_t connection_id_;
  string_type auth_method_data_;
  classic_protocol::capabilities::value_type capabilities_;
  uint8_t collation_;
  classic_protocol::status::value_type status_flags_;
  string_type auth_method_name_;
};

template <bool Borrowed>
inline bool operator==(const Greeting<Borrowed> &a,
                       const Greeting<Borrowed> &b) {
  return (a.protocol_version() == b.protocol_version()) &&
         (a.version() == b.version()) &&
         (a.connection_id() == b.connection_id()) &&
         (a.auth_method_data() == b.auth_method_data()) &&
         (a.capabilities() == b.capabilities()) &&
         (a.collation() == b.collation()) &&
         (a.status_flags() == b.status_flags()) &&
         (a.auth_method_name() == b.auth_method_name());
}

template <bool Borrowed>
class AuthMethodSwitch {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr AuthMethodSwitch() = default;

  constexpr AuthMethodSwitch(string_type auth_method,
                             string_type auth_method_data)
      : auth_method_{std::move(auth_method)},
        auth_method_data_{std::move(auth_method_data)} {}

  string_type auth_method() const { return auth_method_; }
  string_type auth_method_data() const { return auth_method_data_; }

 private:
  string_type auth_method_;
  string_type auth_method_data_;
};

template <bool Borrowed>
inline bool operator==(const AuthMethodSwitch<Borrowed> &a,
                       const AuthMethodSwitch<Borrowed> &b) {
  return (a.auth_method_data() == b.auth_method_data()) &&
         (a.auth_method() == b.auth_method());
}

/**
 * Opaque auth-method-data message.
 *
 * used for server messages the handshake phase that aren't
 *
 * - Ok
 * - Error
 * - AuthMethodSwitch
 *
 * like caching_sha2_password does:
 *
 * - 0x01 0x02 (send public key)
 * - 0x01 0x03 (send full handshake)
 * - 0x01 0x04 (fast path done)
 */
template <bool Borrowed>
class AuthMethodData {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr AuthMethodData(string_type auth_method_data)
      : auth_method_data_{std::move(auth_method_data)} {}

  constexpr string_type auth_method_data() const { return auth_method_data_; }

 private:
  string_type auth_method_data_;
};

template <bool Borrowed>
inline bool operator==(const AuthMethodData<Borrowed> &a,
                       const AuthMethodData<Borrowed> &b) {
  return a.auth_method_data() == b.auth_method_data();
}

/**
 * Ok message.
 *
 * - affected_rows
 * - last_insert_id
 * - status_flags
 * - warning_count
 * - optional message
 * - optional server-side tracked session_changes
 */
template <bool Borrowed>
class Ok {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr Ok() = default;

  constexpr Ok(uint64_t affected_rows, uint64_t last_insert_id,
               classic_protocol::status::value_type status_flags,
               uint16_t warning_count, string_type message = "",
               string_type session_changes = "")
      : status_flags_{status_flags},
        warning_count_{warning_count},
        last_insert_id_{last_insert_id},
        affected_rows_{affected_rows},
        message_{std::move(message)},
        session_changes_{std::move(session_changes)} {}

  constexpr void status_flags(classic_protocol::status::value_type flags) {
    status_flags_ = flags;
  }

  constexpr classic_protocol::status::value_type status_flags() const noexcept {
    return status_flags_;
  }

  constexpr void warning_count(uint16_t count) { warning_count_ = count; }
  constexpr uint16_t warning_count() const noexcept { return warning_count_; }

  constexpr void last_insert_id(uint64_t val) { last_insert_id_ = val; }
  constexpr uint64_t last_insert_id() const noexcept { return last_insert_id_; }

  constexpr void affected_rows(uint64_t val) { affected_rows_ = val; }
  constexpr uint64_t affected_rows() const noexcept { return affected_rows_; }

  constexpr void message(const string_type &msg) { message_ = msg; }
  constexpr string_type message() const { return message_; }

  constexpr void session_changes(const string_type &changes) {
    session_changes_ = changes;
  }
  /**
   * get session-changes.
   *
   * @returns encoded array of session_track::Field
   */
  string_type session_changes() const { return session_changes_; }

 private:
  classic_protocol::status::value_type status_flags_{};
  uint16_t warning_count_{};
  uint64_t last_insert_id_{};
  uint64_t affected_rows_{};

  string_type message_{};
  string_type session_changes_{};
};

template <bool Borrowed>
inline bool operator==(const Ok<Borrowed> &a, const Ok<Borrowed> &b) {
  return (a.status_flags() == b.status_flags()) &&
         (a.warning_count() == b.warning_count()) &&
         (a.last_insert_id() == b.last_insert_id()) &&
         (a.affected_rows() == b.affected_rows()) &&
         (a.message() == b.message()) &&
         (a.session_changes() == b.session_changes());
}

/**
 * End of Resultset message.
 */
template <bool Borrowed>
class Eof : public Ok<Borrowed> {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  using base__ = Ok<Borrowed>;

  using base__::base__;

  // 3.23-like constructor
  constexpr Eof() = default;

  // 4.1-like constructor
  constexpr Eof(classic_protocol::status::value_type status_flags,
                uint16_t warning_count)
      : base__(0, 0, status_flags, warning_count) {}

  constexpr Eof(classic_protocol::status::value_type status_flags,
                uint16_t warning_count, string_type message,
                string_type session_changes)
      : base__(0, 0, status_flags, warning_count, std::move(message),
               std::move(session_changes)) {}
};

/**
 * Error message.
 */
template <bool Borrowed>
class Error {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr Error() = default;
  /**
   * construct an Error message.
   *
   * @param error_code error code
   * @param message error message
   * @param sql_state SQL state
   */
  constexpr Error(uint16_t error_code, string_type message,
                  string_type sql_state = "HY000")
      : error_code_{error_code},
        message_{std::move(message)},
        sql_state_{std::move(sql_state)} {}

  constexpr uint16_t error_code() const noexcept { return error_code_; }
  constexpr void error_code(uint16_t code) { error_code_ = code; }
  constexpr string_type sql_state() const { return sql_state_; }
  constexpr void sql_state(const string_type &state) { sql_state_ = state; }
  constexpr string_type message() const { return message_; }
  constexpr void message(const string_type &msg) { message_ = msg; }

 private:
  uint16_t error_code_{0};
  string_type message_;
  string_type sql_state_;
};

template <bool Borrowed>
inline bool operator==(const Error<Borrowed> &a, const Error<Borrowed> &b) {
  return (a.error_code() == b.error_code()) &&
         (a.sql_state() == b.sql_state()) && (a.message() == b.message());
}

/**
 * ColumnCount message.
 */
class ColumnCount {
 public:
  /**
   * construct an ColumnCount message.
   *
   * @param count column count
   */
  constexpr ColumnCount(uint64_t count) : count_{count} {}

  constexpr uint64_t count() const noexcept { return count_; }

 private:
  uint64_t count_;
};

constexpr inline bool operator==(const ColumnCount &a, const ColumnCount &b) {
  return (a.count() == b.count());
}

template <bool Borrowed>
class ColumnMeta {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr ColumnMeta(string_type catalog, string_type schema,
                       string_type table, string_type orig_table,
                       string_type name, string_type orig_name,
                       uint16_t collation, uint32_t column_length, uint8_t type,
                       classic_protocol::column_def::value_type flags,
                       uint8_t decimals)
      : catalog_{std::move(catalog)},
        schema_{std::move(schema)},
        table_{std::move(table)},
        orig_table_{std::move(orig_table)},
        name_{std::move(name)},
        orig_name_{std::move(orig_name)},
        collation_{collation},
        column_length_{column_length},
        type_{type},
        flags_{flags},
        decimals_{decimals} {}

  constexpr string_type catalog() const { return catalog_; }
  constexpr string_type schema() const { return schema_; }
  constexpr string_type table() const { return table_; }
  constexpr string_type orig_table() const { return orig_table_; }
  constexpr string_type name() const { return name_; }
  constexpr string_type orig_name() const { return orig_name_; }
  constexpr uint16_t collation() const { return collation_; }
  constexpr uint32_t column_length() const { return column_length_; }
  constexpr uint8_t type() const { return type_; }
  constexpr classic_protocol::column_def::value_type flags() const {
    return flags_;
  }
  constexpr uint8_t decimals() const { return decimals_; }

 private:
  string_type catalog_;
  string_type schema_;
  string_type table_;
  string_type orig_table_;
  string_type name_;
  string_type orig_name_;
  uint16_t collation_;
  uint32_t column_length_;
  uint8_t type_;
  classic_protocol::column_def::value_type flags_;
  uint8_t decimals_;
};

template <bool Borrowed>
inline bool operator==(const ColumnMeta<Borrowed> &a,
                       const ColumnMeta<Borrowed> &b) {
  return (a.catalog() == b.catalog()) && (a.schema() == b.schema()) &&
         (a.table() == b.table()) && (a.orig_table() == b.orig_table()) &&
         (a.name() == b.name()) && (a.orig_name() == b.orig_name()) &&
         (a.collation() == b.collation()) &&
         (a.column_length() == b.column_length()) && (a.type() == b.type()) &&
         (a.flags() == b.flags()) && (a.decimals() == b.decimals());
}

/**
 * Row in a resultset.
 *
 * each Row is sent as its own frame::Frame
 *
 * each Field in a row may either be NULL or a std::string.
 */
template <bool Borrowed>
class Row {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  using value_type = std::optional<string_type>;
  using const_iterator = typename std::vector<value_type>::const_iterator;

  Row(std::vector<value_type> fields) : fields_{std::move(fields)} {}

  auto begin() const { return fields_.begin(); }
  auto end() const { return fields_.end(); }

 private:
  std::vector<value_type> fields_;
};

template <bool Borrowed>
inline bool operator==(const Row<Borrowed> &a, const Row<Borrowed> &b) {
  auto a_iter = a.begin();
  const auto a_end = a.end();
  auto b_iter = b.begin();
  const auto b_end = b.end();

  for (; a_iter != a_end && b_iter != b_end; ++a_iter, ++b_iter) {
    if (*a_iter != *b_iter) return false;
  }

  return true;
}

template <bool Borrowed>
class ResultSet {
 public:
  ResultSet(std::vector<ColumnMeta<Borrowed>> column_metas,
            std::vector<Row<Borrowed>> rows)
      : column_metas_{std::move(column_metas)}, rows_{std::move(rows)} {}

  std::vector<ColumnMeta<Borrowed>> column_metas() const {
    return column_metas_;
  }
  std::vector<Row<Borrowed>> rows() const { return rows_; }

 private:
  std::vector<ColumnMeta<Borrowed>> column_metas_;
  std::vector<Row<Borrowed>> rows_;
};

/**
 * StmtPrepareOk message.
 *
 * response to a client::StmtPrepare
 */
class StmtPrepareOk {
 public:
  /**
   * create a Ok message for a client::StmtPrepare.
   *
   * @param stmt_id id of the statement
   * @param column_count number of columns the prepared stmt will return
   * @param param_count number of parameters the prepared stmt contained
   * @param warning_count number of warnings the prepared stmt created
   * @param with_metadata 0 if no metadata shall be sent for "param_count" and
   * "column_count".
   */
  constexpr StmtPrepareOk(uint32_t stmt_id, uint16_t column_count,
                          uint16_t param_count, uint16_t warning_count,
                          uint8_t with_metadata)
      : statement_id_{stmt_id},
        warning_count_{warning_count},
        param_count_{param_count},
        column_count_{column_count},
        with_metadata_{with_metadata} {}

  constexpr uint32_t statement_id() const noexcept { return statement_id_; }
  constexpr uint16_t warning_count() const noexcept { return warning_count_; }

  constexpr uint16_t column_count() const { return column_count_; }
  constexpr uint16_t param_count() const { return param_count_; }
  constexpr uint8_t with_metadata() const { return with_metadata_; }

 private:
  uint32_t statement_id_;
  uint16_t warning_count_;
  uint16_t param_count_;
  uint16_t column_count_;
  uint8_t with_metadata_{1};
};

inline bool operator==(const StmtPrepareOk &a, const StmtPrepareOk &b) {
  return (a.statement_id() == b.statement_id()) &&
         (a.column_count() == b.column_count()) &&
         (a.param_count() == b.param_count()) &&
         (a.warning_count() == b.warning_count());
}

/**
 * StmtRow message.
 *
 * holds the same information as a Row.
 *
 * needs 'types' to be able to encode a Field of the Row.
 */
template <bool Borrowed>
class StmtRow : public Row<Borrowed> {
 public:
  using base_ = Row<Borrowed>;

  using value_type = typename base_::value_type;

  StmtRow(std::vector<field_type::value_type> types,
          std::vector<value_type> fields)
      : base_{std::move(fields)}, types_{std::move(types)} {}

  std::vector<field_type::value_type> types() const { return types_; }

 private:
  std::vector<field_type::value_type> types_;
};

template <bool Borrowed>
class SendFileRequest {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;
  /**
   * construct a SendFileRequest message.
   *
   * @param filename filename
   */
  constexpr SendFileRequest(string_type filename)
      : filename_{std::move(filename)} {}

  constexpr string_type filename() const { return filename_; }

 private:
  string_type filename_;
};

template <bool Borrowed>
constexpr bool operator==(const SendFileRequest<Borrowed> &a,
                          const SendFileRequest<Borrowed> &b) {
  return a.filename() == b.filename();
}

template <bool Borrowed>
class Statistics {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * construct a Statistics message.
   *
   * @param stats statistics
   */
  constexpr Statistics(string_type stats) : stats_{std::move(stats)} {}

  constexpr string_type stats() const { return stats_; }

 private:
  string_type stats_;
};

template <bool Borrowed>
constexpr bool operator==(const Statistics<Borrowed> &a,
                          const Statistics<Borrowed> &b) {
  return a.stats() == b.stats();
}

}  // namespace server

namespace client {

template <bool Borrowed>
class Greeting {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * construct a client::Greeting message.
   *
   * @param capabilities protocol capabilities of the client
   * @param max_packet_size max size of the frame::Frame client wants to send
   * @param collation initial collation of connection
   * @param username username to authenticate as
   * @param auth_method_data auth-method specific data like hashed password
   * @param schema initial schema of the newly authenticated session
   * @param auth_method_name auth-method the data is for
   * @param attributes session-attributes
   */
  constexpr Greeting(classic_protocol::capabilities::value_type capabilities,
                     uint32_t max_packet_size, uint8_t collation,
                     string_type username, string_type auth_method_data,
                     string_type schema, string_type auth_method_name,
                     string_type attributes)
      : capabilities_{capabilities},
        max_packet_size_{max_packet_size},
        collation_{collation},
        username_{std::move(username)},
        auth_method_data_{std::move(auth_method_data)},
        schema_{std::move(schema)},
        auth_method_name_{std::move(auth_method_name)},
        attributes_{std::move(attributes)} {}

  constexpr classic_protocol::capabilities::value_type capabilities() const {
    return capabilities_;
  }

  constexpr void capabilities(classic_protocol::capabilities::value_type caps) {
    capabilities_ = caps;
  }

  constexpr uint32_t max_packet_size() const noexcept {
    return max_packet_size_;
  }
  constexpr void max_packet_size(uint32_t sz) noexcept {
    max_packet_size_ = sz;
  }

  constexpr uint8_t collation() const noexcept { return collation_; }
  constexpr void collation(uint8_t coll) noexcept { collation_ = coll; }

  constexpr string_type username() const { return username_; }
  constexpr void username(const string_type &v) { username_ = v; }

  constexpr string_type auth_method_data() const { return auth_method_data_; }
  constexpr void auth_method_data(const string_type &v) {
    auth_method_data_ = v;
  }

  constexpr string_type schema() const { return schema_; }
  constexpr void schema(const string_type &schema) { schema_ = schema; }

  /**
   * name of the auth-method that was explicitly set.
   *
   * use classic_protocol::AuthMethod() to get the effective auth-method
   * which may be announced though capability flags (like if
   * capabilities::plugin_auth wasn't set)
   */
  constexpr string_type auth_method_name() const { return auth_method_name_; }

  constexpr void auth_method_name(const string_type &name) {
    auth_method_name_ = name;
  }

  // [key, value]* in Codec<wire::VarString> encoding
  constexpr string_type attributes() const { return attributes_; }

  constexpr void attributes(const string_type &attrs) { attributes_ = attrs; }

 private:
  classic_protocol::capabilities::value_type capabilities_;
  uint32_t max_packet_size_;
  uint8_t collation_;
  string_type username_;
  string_type auth_method_data_;
  string_type schema_;
  string_type auth_method_name_;
  string_type attributes_;
};

template <bool Borrowed>
constexpr bool operator==(const Greeting<Borrowed> &a,
                          const Greeting<Borrowed> &b) {
  return (a.capabilities() == b.capabilities()) &&
         (a.max_packet_size() == b.max_packet_size()) &&
         (a.collation() == b.collation()) && (a.username() == b.username()) &&
         (a.auth_method_data() == b.auth_method_data()) &&
         (a.schema() == b.schema()) &&
         (a.auth_method_name() == b.auth_method_name()) &&
         (a.attributes() == b.attributes());
}

template <bool Borrowed>
class Query {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * construct a Query message.
   *
   * @param statement statement to prepare
   */
  constexpr Query(string_type statement) : statement_{std::move(statement)} {}

  constexpr string_type statement() const { return statement_; }

 private:
  string_type statement_;
};

template <bool Borrowed>
constexpr bool operator==(const Query<Borrowed> &a, const Query<Borrowed> &b) {
  return a.statement() == b.statement();
}

template <bool Borrowed>
class ListFields {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * list columns of a table.
   *
   * If 'wildcard' is empty the server will execute:
   *
   * SHOW COLUMNS FROM table_name
   *
   * Otherwise:
   *
   * SHOW COLUMNS FROM table_name LIKE wildcard
   *
   * @param table_name name of table to list
   * @param wildcard wildcard
   */
  constexpr ListFields(string_type table_name, string_type wildcard)
      : table_name_{std::move(table_name)}, wildcard_{std::move(wildcard)} {}

  constexpr string_type table_name() const { return table_name_; }
  constexpr string_type wildcard() const { return wildcard_; }

 private:
  string_type table_name_;
  string_type wildcard_;
};

template <bool Borrowed>
constexpr bool operator==(const ListFields<Borrowed> &a,
                          const ListFields<Borrowed> &b) {
  return a.table_name() == b.table_name() && a.wildcard() == b.wildcard();
}

template <bool Borrowed>
class InitSchema {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * construct a InitSchema message.
   *
   * @param schema schema to change to
   */
  constexpr InitSchema(string_type schema) : schema_{std::move(schema)} {}

  constexpr string_type schema() const { return schema_; }

 private:
  string_type schema_;
};

template <bool Borrowed>
constexpr bool operator==(const InitSchema<Borrowed> &a,
                          const InitSchema<Borrowed> &b) {
  return a.schema() == b.schema();
}

template <bool Borrowed>
class ChangeUser {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;
  /**
   * construct a ChangeUser message.
   *
   * @param username username to change to
   * @param auth_method_data auth-method specific data like hashed password
   * @param schema initial schema of the newly authenticated session
   * @param auth_method_name auth-method the data is for
   * @param collation collation
   * @param attributes session-attributes
   */
  constexpr ChangeUser(string_type username, string_type auth_method_data,
                       string_type schema, uint16_t collation,
                       string_type auth_method_name, string_type attributes)
      : username_{std::move(username)},
        auth_method_data_{std::move(auth_method_data)},
        schema_{std::move(schema)},
        collation_{collation},
        auth_method_name_{std::move(auth_method_name)},
        attributes_{std::move(attributes)} {}

  constexpr uint8_t collation() const noexcept { return collation_; }
  constexpr string_type username() const { return username_; }
  constexpr string_type auth_method_data() const { return auth_method_data_; }
  constexpr string_type schema() const { return schema_; }
  constexpr string_type auth_method_name() const { return auth_method_name_; }

  // [key, value]* in Codec<wire::VarString> encoding
  constexpr string_type attributes() const { return attributes_; }

 private:
  string_type username_;
  string_type auth_method_data_;
  string_type schema_;
  uint16_t collation_;
  string_type auth_method_name_;
  string_type attributes_;
};

template <bool Borrowed>
constexpr bool operator==(const ChangeUser<Borrowed> &a,
                          const ChangeUser<Borrowed> &b) {
  return (a.collation() == b.collation()) && (a.username() == b.username()) &&
         (a.auth_method_data() == b.auth_method_data()) &&
         (a.schema() == b.schema()) &&
         (a.auth_method_name() == b.auth_method_name()) &&
         (a.attributes() == b.attributes());
}

// no content
class ResetConnection {};

constexpr bool operator==(const ResetConnection &, const ResetConnection &) {
  return true;
}

// no content
class Statistics {};

constexpr bool operator==(const Statistics &, const Statistics &) {
  return true;
}

class Reload {
 public:
  /**
   * construct a Reload message.
   *
   * @param cmds what to reload
   */
  constexpr Reload(classic_protocol::reload_cmds::value_type cmds)
      : cmds_{cmds} {}

  constexpr classic_protocol::reload_cmds::value_type cmds() const {
    return cmds_;
  }

 private:
  classic_protocol::reload_cmds::value_type cmds_;
};

inline bool operator==(const Reload &a, const Reload &b) {
  return a.cmds() == b.cmds();
}

class Kill {
 public:
  /**
   * construct a Kill message.
   *
   * @param connection_id payload
   */
  constexpr Kill(uint32_t connection_id) : connection_id_{connection_id} {}

  constexpr uint32_t connection_id() const { return connection_id_; }

 private:
  uint32_t connection_id_;
};

constexpr bool operator==(const Kill &a, const Kill &b) {
  return a.connection_id() == b.connection_id();
}

template <bool Borrowed>
class SendFile {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * construct a SendFile message.
   *
   * @param payload payload
   */
  constexpr SendFile(string_type payload) : payload_{std::move(payload)} {}

  constexpr string_type payload() const { return payload_; }

 private:
  string_type payload_;
};

template <bool Borrowed>
inline bool operator==(const SendFile<Borrowed> &a,
                       const SendFile<Borrowed> &b) {
  return a.payload() == b.payload();
}

template <bool Borrowed>
class StmtPrepare {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;
  /**
   * construct a PrepareStmt message.
   *
   * @param statement statement to prepare
   */
  constexpr StmtPrepare(string_type statement)
      : statement_{std::move(statement)} {}

  constexpr string_type statement() const { return statement_; }

 private:
  string_type statement_;
};

template <bool Borrowed>
inline bool operator==(const StmtPrepare<Borrowed> &a,
                       const StmtPrepare<Borrowed> &b) {
  return a.statement() == b.statement();
}

/**
 * append data to a parameter of a prepared statement.
 */
template <bool Borrowed>
class StmtParamAppendData {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * construct an append-data-to-parameter message.
   *
   * @param statement_id statement-id to close
   * @param param_id parameter-id to append data to
   * @param data data to append to param_id of statement_id
   */
  constexpr StmtParamAppendData(uint32_t statement_id, uint16_t param_id,
                                string_type data)
      : statement_id_{statement_id},
        param_id_{param_id},
        data_{std::move(data)} {}

  constexpr uint32_t statement_id() const { return statement_id_; }
  constexpr uint16_t param_id() const { return param_id_; }
  constexpr string_type data() const { return data_; }

 private:
  uint32_t statement_id_;
  uint16_t param_id_;
  string_type data_;
};

template <bool Borrowed>
inline bool operator==(const StmtParamAppendData<Borrowed> &a,
                       const StmtParamAppendData<Borrowed> &b) {
  return a.statement_id() == b.statement_id() && a.param_id() == b.param_id() &&
         a.data() == b.data();
}

/**
 * execute a prepared statement.
 *
 * 'values' raw bytes as encoded by the binary codec
 */
template <bool Borrowed>
class StmtExecute {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  using value_type = std::optional<string_type>;

  struct ParamDef {
    ParamDef() = default;

    ParamDef(uint16_t type_and_flags_) : type_and_flags(type_and_flags_) {}

    friend bool operator==(const ParamDef &lhs, const ParamDef &rhs) {
      return lhs.type_and_flags == rhs.type_and_flags;
    }

    uint16_t type_and_flags{};
  };

  /**
   * construct a ExecuteStmt message.
   *
   * @param statement_id statement id
   * @param flags cursor flags
   * @param iteration_count iteration_count
   * @param new_params_bound new params bound
   * @param types field types of the parameters
   * @param values binary-encoded values without length-bytes
   */
  StmtExecute(uint32_t statement_id, classic_protocol::cursor::value_type flags,
              uint32_t iteration_count, bool new_params_bound,
              std::vector<ParamDef> types, std::vector<value_type> values)
      : statement_id_{statement_id},
        flags_{flags},
        iteration_count_{iteration_count},
        new_params_bound_{new_params_bound},
        types_{std::move(types)},
        values_{std::move(values)} {}

  uint32_t statement_id() const noexcept { return statement_id_; }
  classic_protocol::cursor::value_type flags() const noexcept { return flags_; }
  uint32_t iteration_count() const noexcept { return iteration_count_; }
  bool new_params_bound() const noexcept { return new_params_bound_; }
  std::vector<ParamDef> types() const { return types_; }
  std::vector<value_type> values() const { return values_; }

 private:
  uint32_t statement_id_;
  classic_protocol::cursor::value_type flags_;
  uint32_t iteration_count_;
  bool new_params_bound_;
  std::vector<ParamDef> types_;
  std::vector<value_type> values_;
};

template <bool Borrowed>
inline bool operator==(const StmtExecute<Borrowed> &a,
                       const StmtExecute<Borrowed> &b) {
  return a.statement_id() == b.statement_id() && a.flags() == b.flags() &&
         a.iteration_count() == b.iteration_count() &&
         a.new_params_bound() == b.new_params_bound() &&
         a.types() == b.types() && a.values() == b.values();
}

/**
 * close a prepared statement.
 */
class StmtClose {
 public:
  /**
   * construct a StmtClose message.
   *
   * @param statement_id statement-id to close
   */
  constexpr StmtClose(uint32_t statement_id) : statement_id_{statement_id} {}

  constexpr uint32_t statement_id() const { return statement_id_; }

 private:
  uint32_t statement_id_;
};

constexpr bool operator==(const StmtClose &a, const StmtClose &b) {
  return a.statement_id() == b.statement_id();
}

/**
 * reset a prepared statement.
 */
class StmtReset {
 public:
  /**
   * construct a ResetStmt message.
   *
   * @param statement_id statement-id to close
   */
  constexpr StmtReset(uint32_t statement_id) : statement_id_{statement_id} {}

  constexpr uint32_t statement_id() const { return statement_id_; }

 private:
  uint32_t statement_id_;
};

constexpr bool operator==(const StmtReset &a, const StmtReset &b) {
  return a.statement_id() == b.statement_id();
}

/**
 * fetch rows from an executed statement.
 */
class StmtFetch {
 public:
  /**
   * construct a ResetStmt message.
   *
   * @param statement_id statement-id to close
   * @param row_count statement-id to close
   */
  constexpr StmtFetch(uint32_t statement_id, uint32_t row_count)
      : statement_id_{statement_id}, row_count_{row_count} {}

  constexpr uint32_t statement_id() const { return statement_id_; }
  constexpr uint32_t row_count() const { return row_count_; }

 private:
  uint32_t statement_id_;
  uint32_t row_count_;
};

constexpr bool operator==(const StmtFetch &a, const StmtFetch &b) {
  return a.statement_id() == b.statement_id() && a.row_count() == b.row_count();
}

/**
 * set options on the current connection.
 */
class SetOption {
 public:
  /**
   * construct a SetOption message.
   *
   * @param option options to set
   */
  constexpr SetOption(uint16_t option) : option_{option} {}

  constexpr uint16_t option() const { return option_; }

 private:
  uint16_t option_;
};

constexpr bool operator==(const SetOption &a, const SetOption &b) {
  return a.option() == b.option();
}

// no content
class Quit {};

constexpr bool operator==(const Quit &, const Quit &) { return true; }

// no content
class Ping {};

constexpr bool operator==(const Ping &, const Ping &) { return true; }

template <bool Borrowed>
class AuthMethodData {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  /**
   * send data for the current auth-method to server.
   *
   * @param auth_method_data data of the auth-method
   */
  constexpr AuthMethodData(string_type auth_method_data)
      : auth_method_data_{std::move(auth_method_data)} {}

  constexpr string_type auth_method_data() const { return auth_method_data_; }

 private:
  string_type auth_method_data_;
};

template <bool Borrowed>
constexpr bool operator==(const AuthMethodData<Borrowed> &a,
                          const AuthMethodData<Borrowed> &b) {
  return a.auth_method_data() == b.auth_method_data();
}

// switch to Clone Protocol.
//
// response: server::Ok -> clone protocol
// response: server::Error
//
// no content
class Clone {};

template <bool Borrowed>
class BinlogDump {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  using Flags =
      typename classic_protocol::message::client::impl::BinlogDump::Flags;

  constexpr BinlogDump(stdx::flags<Flags> flags, uint32_t server_id,
                       string_type filename, uint32_t position)
      : position_{position},
        flags_{flags},
        server_id_{server_id},
        filename_{std::move(filename)} {}

  [[nodiscard]] constexpr stdx::flags<Flags> flags() const { return flags_; }
  [[nodiscard]] constexpr uint32_t server_id() const { return server_id_; }
  [[nodiscard]] constexpr string_type filename() const { return filename_; }
  [[nodiscard]] constexpr uint64_t position() const { return position_; }

 private:
  uint32_t position_;
  stdx::flags<Flags> flags_;
  uint32_t server_id_;
  string_type filename_;
};

template <bool Borrowed>
class BinlogDumpGtid {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  using Flags =
      typename classic_protocol::message::client::impl::BinlogDumpGtid::Flags;

  constexpr BinlogDumpGtid(stdx::flags<Flags> flags, uint32_t server_id,
                           string_type filename, uint64_t position,
                           string_type sids)
      : flags_{flags},
        server_id_{server_id},
        filename_{std::move(filename)},
        position_{position},
        sids_{std::move(sids)} {}

  [[nodiscard]] constexpr stdx::flags<Flags> flags() const { return flags_; }
  [[nodiscard]] constexpr uint32_t server_id() const { return server_id_; }
  [[nodiscard]] constexpr string_type filename() const { return filename_; }
  [[nodiscard]] constexpr uint64_t position() const { return position_; }
  [[nodiscard]] constexpr string_type sids() const { return sids_; }

 private:
  stdx::flags<Flags> flags_;
  uint32_t server_id_;
  string_type filename_;
  uint64_t position_;
  string_type sids_;
};

template <bool Borrowed>
class RegisterReplica {
 public:
  using string_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr RegisterReplica(uint32_t server_id, string_type hostname,
                            string_type username, string_type password,
                            uint16_t port, uint32_t replication_rank,
                            uint32_t master_id)
      : server_id_{server_id},
        hostname_{std::move(hostname)},
        username_{std::move(username)},
        password_{std::move(password)},
        port_{port},
        replication_rank_{replication_rank},
        master_id_{master_id} {}

  [[nodiscard]] constexpr uint32_t server_id() const { return server_id_; }
  [[nodiscard]] constexpr string_type hostname() const { return hostname_; }
  [[nodiscard]] constexpr string_type username() const { return username_; }
  [[nodiscard]] constexpr string_type password() const { return password_; }
  [[nodiscard]] constexpr uint16_t port() const { return port_; }
  [[nodiscard]] constexpr uint32_t replication_rank() const {
    return replication_rank_;
  }
  [[nodiscard]] constexpr uint32_t master_id() const { return master_id_; }

 private:
  uint32_t server_id_;
  string_type hostname_;
  string_type username_;
  string_type password_;
  uint16_t port_;
  uint32_t replication_rank_;
  uint32_t master_id_;
};

}  // namespace client
}  // namespace message
}  // namespace borrowable

namespace message {
namespace server {
using Ok = borrowable::message::server::Ok<false>;
using Error = borrowable::message::server::Error<false>;
using Eof = borrowable::message::server::Eof<false>;
using Greeting = borrowable::message::server::Greeting<false>;
using ColumnCount = borrowable::message::server::ColumnCount;
using ColumnMeta = borrowable::message::server::ColumnMeta<false>;
using AuthMethodSwitch = borrowable::message::server::AuthMethodSwitch<false>;
using AuthMethodData = borrowable::message::server::AuthMethodData<false>;
using SendFileRequest = borrowable::message::server::SendFileRequest<false>;
using Row = borrowable::message::server::Row<false>;
using StmtRow = borrowable::message::server::StmtRow<false>;
using StmtPrepareOk = borrowable::message::server::StmtPrepareOk;
using Statistics = borrowable::message::server::Statistics<false>;
}  // namespace server

namespace client {
using Greeting = borrowable::message::client::Greeting<false>;
using AuthMethodData = borrowable::message::client::AuthMethodData<false>;
using InitSchema = borrowable::message::client::InitSchema<false>;
using ListFields = borrowable::message::client::ListFields<false>;
using Query = borrowable::message::client::Query<false>;
using RegisterReplica = borrowable::message::client::RegisterReplica<false>;
using Ping = borrowable::message::client::Ping;
using Kill = borrowable::message::client::Kill;
using ChangeUser = borrowable::message::client::ChangeUser<false>;
using Reload = borrowable::message::client::Reload;
using ResetConnection = borrowable::message::client::ResetConnection;
using Quit = borrowable::message::client::Quit;
using StmtPrepare = borrowable::message::client::StmtPrepare<false>;
using StmtExecute = borrowable::message::client::StmtExecute<false>;
using StmtReset = borrowable::message::client::StmtReset;
using StmtClose = borrowable::message::client::StmtClose;
using StmtParamAppendData =
    borrowable::message::client::StmtParamAppendData<false>;
using SetOption = borrowable::message::client::SetOption;
using StmtFetch = borrowable::message::client::StmtFetch;
using Statistics = borrowable::message::client::Statistics;
using SendFile = borrowable::message::client::SendFile<false>;
using Clone = borrowable::message::client::Clone;
using BinlogDump = borrowable::message::client::BinlogDump<false>;
using BinlogDumpGtid = borrowable::message::client::BinlogDumpGtid<false>;
}  // namespace client
}  // namespace message

namespace borrowed {
namespace message {
namespace server {
using Ok = borrowable::message::server::Ok<true>;
using Error = borrowable::message::server::Error<true>;
using Eof = borrowable::message::server::Eof<true>;
using Greeting = borrowable::message::server::Greeting<true>;
using ColumnCount = borrowable::message::server::ColumnCount;
using ColumnMeta = borrowable::message::server::ColumnMeta<true>;
using AuthMethodSwitch = borrowable::message::server::AuthMethodSwitch<true>;
using AuthMethodData = borrowable::message::server::AuthMethodData<true>;
using SendFileRequest = borrowable::message::server::SendFileRequest<true>;
using Row = borrowable::message::server::Row<true>;
using StmtRow = borrowable::message::server::StmtRow<true>;
using StmtPrepareOk = borrowable::message::server::StmtPrepareOk;
using Statistics = borrowable::message::server::Statistics<true>;
}  // namespace server

namespace client {
using Greeting = borrowable::message::client::Greeting<true>;
using AuthMethodData = borrowable::message::client::AuthMethodData<true>;
using Query = borrowable::message::client::Query<true>;
using InitSchema = borrowable::message::client::InitSchema<true>;
using ListFields = borrowable::message::client::ListFields<true>;
using RegisterReplica = borrowable::message::client::RegisterReplica<true>;
using Ping = borrowable::message::client::Ping;
using Kill = borrowable::message::client::Kill;
using ChangeUser = borrowable::message::client::ChangeUser<true>;
using Reload = borrowable::message::client::Reload;
using ResetConnection = borrowable::message::client::ResetConnection;
using Quit = borrowable::message::client::Quit;
using StmtPrepare = borrowable::message::client::StmtPrepare<true>;
using StmtExecute = borrowable::message::client::StmtExecute<true>;
using StmtReset = borrowable::message::client::StmtReset;
using StmtClose = borrowable::message::client::StmtClose;
using StmtParamAppendData =
    borrowable::message::client::StmtParamAppendData<true>;
using SetOption = borrowable::message::client::SetOption;
using StmtFetch = borrowable::message::client::StmtFetch;
using Statistics = borrowable::message::client::Statistics;
using SendFile = borrowable::message::client::SendFile<true>;
using Clone = borrowable::message::client::Clone;
using BinlogDump = borrowable::message::client::BinlogDump<true>;
using BinlogDumpGtid = borrowable::message::client::BinlogDumpGtid<true>;
}  // namespace client
}  // namespace message
}  // namespace borrowed

}  // namespace classic_protocol

#endif
