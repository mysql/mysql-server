/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

namespace classic_protocol {

/**
 * AuthMethod of classic protocol.
 *
 * classic proto supports negotiating the auth-method via capabilities and
 * auth-method names.
 */
class AuthMethod {
 public:
  AuthMethod(classic_protocol::capabilities::value_type capabilities,
             std::string auth_method_name)
      : capabilities_{capabilities},
        auth_method_name_{std::move(auth_method_name)} {}

  std::string name() const {
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
  const std::string auth_method_name_;
};

namespace message {

namespace server {
class Greeting {
 public:
  Greeting(uint8_t protocol_version, std::string version,
           uint32_t connection_id, std::string auth_method_data,
           classic_protocol::capabilities::value_type capabilities,
           uint8_t collation, classic_protocol::status::value_type status_flags,
           std::string auth_method_name)
      : protocol_version_{protocol_version},
        version_{std::move(version)},
        connection_id_{connection_id},
        auth_method_data_{std::move(auth_method_data)},
        capabilities_{capabilities},
        collation_{collation},
        status_flags_{status_flags},
        auth_method_name_{std::move(auth_method_name)} {}

  uint8_t protocol_version() const noexcept { return protocol_version_; }
  std::string version() const { return version_; }
  std::string auth_method_name() const { return auth_method_name_; }
  std::string auth_method_data() const { return auth_method_data_; }
  classic_protocol::capabilities::value_type capabilities() const noexcept {
    return capabilities_;
  }

  void capabilities(classic_protocol::capabilities::value_type caps) {
    capabilities_ = caps;
  }

  uint8_t collation() const noexcept { return collation_; }
  classic_protocol::status::value_type status_flags() const noexcept {
    return status_flags_;
  }
  uint32_t connection_id() const noexcept { return connection_id_; }

 private:
  uint8_t protocol_version_;
  std::string version_;
  uint32_t connection_id_;
  std::string auth_method_data_;
  classic_protocol::capabilities::value_type capabilities_;
  uint8_t collation_;
  classic_protocol::status::value_type status_flags_;
  std::string auth_method_name_;
};

inline bool operator==(const Greeting &a, const Greeting &b) {
  return (a.protocol_version() == b.protocol_version()) &&
         (a.version() == b.version()) &&
         (a.connection_id() == b.connection_id()) &&
         (a.auth_method_data() == b.auth_method_data()) &&
         (a.capabilities() == b.capabilities()) &&
         (a.collation() == b.collation()) &&
         (a.status_flags() == b.status_flags()) &&
         (a.auth_method_name() == b.auth_method_name());
}

class AuthMethodSwitch {
 public:
  AuthMethodSwitch() = default;

  AuthMethodSwitch(std::string auth_method, std::string auth_method_data)
      : auth_method_{std::move(auth_method)},
        auth_method_data_{std::move(auth_method_data)} {}

  std::string auth_method() const { return auth_method_; }
  std::string auth_method_data() const { return auth_method_data_; }

 private:
  std::string auth_method_;
  std::string auth_method_data_;
};

inline bool operator==(const AuthMethodSwitch &a, const AuthMethodSwitch &b) {
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
class AuthMethodData {
 public:
  AuthMethodData(std::string auth_method_data)
      : auth_method_data_{std::move(auth_method_data)} {}

  std::string auth_method_data() const { return auth_method_data_; }

 private:
  std::string auth_method_data_;
};

inline bool operator==(const AuthMethodData &a, const AuthMethodData &b) {
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
class Ok {
 public:
  Ok() = default;

  Ok(uint64_t affected_rows, uint64_t last_insert_id,
     classic_protocol::status::value_type status_flags, uint16_t warning_count,
     std::string message = "", std::string session_changes = "")
      : status_flags_{status_flags},
        warning_count_{warning_count},
        last_insert_id_{last_insert_id},
        affected_rows_{affected_rows},
        message_{std::move(message)},
        session_changes_{std::move(session_changes)} {}

  classic_protocol::status::value_type status_flags() const noexcept {
    return status_flags_;
  }
  uint16_t warning_count() const noexcept { return warning_count_; }
  uint64_t last_insert_id() const noexcept { return last_insert_id_; }
  uint64_t affected_rows() const noexcept { return affected_rows_; }

  std::string message() const { return message_; }

  /**
   * get session-changes.
   *
   * @returns encoded array of session_track::Field
   */
  std::string session_changes() const { return session_changes_; }

 private:
  classic_protocol::status::value_type status_flags_{};
  uint16_t warning_count_{};
  uint64_t last_insert_id_{};
  uint64_t affected_rows_{};

  std::string message_{};
  std::string session_changes_{};
};

inline bool operator==(const Ok &a, const Ok &b) {
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
class Eof : public Ok {
 public:
  using Ok::Ok;

  // 3.23-like constructor
  Eof() : Ok(0, 0, 0, 0) {}

  // 4.1-like constructor
  Eof(classic_protocol::status::value_type status_flags, uint16_t warning_count)
      : Ok(0, 0, status_flags, warning_count) {}

  Eof(classic_protocol::status::value_type status_flags, uint16_t warning_count,
      std::string message, std::string session_changes)
      : Ok(0, 0, status_flags, warning_count, std::move(message),
           std::move(session_changes)) {}
};

/**
 * Error message.
 */
class Error {
 public:
  /**
   * construct an Error message.
   *
   * @param error_code error code
   * @param message error message
   * @param sql_state SQL state
   */
  Error(uint16_t error_code, std::string message,
        std::string sql_state = "HY000")
      : error_code_{error_code},
        message_{std::move(message)},
        sql_state_{std::move(sql_state)} {}

  uint16_t error_code() const noexcept { return error_code_; }
  std::string sql_state() const { return sql_state_; }
  std::string message() const { return message_; }

 private:
  uint16_t error_code_;
  std::string message_;
  std::string sql_state_;
};

inline bool operator==(const Error &a, const Error &b) {
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

class ColumnMeta {
 public:
  ColumnMeta(std::string catalog, std::string schema, std::string table,
             std::string orig_table, std::string name, std::string orig_name,
             uint16_t collation, uint32_t column_length, uint8_t type,
             classic_protocol::column_def::value_type flags, uint8_t decimals)
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

  std::string catalog() const { return catalog_; }
  std::string schema() const { return schema_; }
  std::string table() const { return table_; }
  std::string orig_table() const { return orig_table_; }
  std::string name() const { return name_; }
  std::string orig_name() const { return orig_name_; }
  uint16_t collation() const { return collation_; }
  uint32_t column_length() const { return column_length_; }
  uint8_t type() const { return type_; }
  classic_protocol::column_def::value_type flags() const { return flags_; }
  uint8_t decimals() const { return decimals_; }

 private:
  std::string catalog_;
  std::string schema_;
  std::string table_;
  std::string orig_table_;
  std::string name_;
  std::string orig_name_;
  uint16_t collation_;
  uint32_t column_length_;
  uint8_t type_;
  classic_protocol::column_def::value_type flags_;
  uint8_t decimals_;
};

inline bool operator==(const ColumnMeta &a, const ColumnMeta &b) {
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
class Row {
 public:
  using value_type = std::optional<std::string>;
  using const_iterator = typename std::vector<value_type>::const_iterator;

  Row(std::vector<value_type> fields) : fields_{std::move(fields)} {}

  auto begin() const { return fields_.begin(); }
  auto end() const { return fields_.end(); }

 private:
  std::vector<value_type> fields_;
};

inline bool operator==(const Row &a, const Row &b) {
  auto a_iter = a.begin();
  const auto a_end = a.end();
  auto b_iter = b.begin();
  const auto b_end = b.end();

  for (; a_iter != a_end && b_iter != b_end; ++a_iter, ++b_iter) {
    if (*a_iter != *b_iter) return false;
  }

  return true;
}

class ResultSet {
 public:
  ResultSet(std::vector<ColumnMeta> column_metas, std::vector<Row> rows)
      : column_metas_{std::move(column_metas)}, rows_{std::move(rows)} {}

  std::vector<ColumnMeta> column_metas() const { return column_metas_; }
  std::vector<Row> rows() const { return rows_; }

 private:
  std::vector<ColumnMeta> column_metas_;
  std::vector<Row> rows_;
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
  StmtPrepareOk(uint32_t stmt_id, uint16_t column_count, uint16_t param_count,
                uint16_t warning_count, uint8_t with_metadata)
      : statement_id_{stmt_id},
        warning_count_{warning_count},
        param_count_{param_count},
        column_count_{column_count},
        with_metadata_{with_metadata} {}

  uint32_t statement_id() const noexcept { return statement_id_; }
  uint16_t warning_count() const noexcept { return warning_count_; }

  uint16_t column_count() const { return column_count_; }
  uint16_t param_count() const { return param_count_; }
  uint8_t with_metadata() const { return with_metadata_; }

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
class StmtRow : public Row {
 public:
  StmtRow(std::vector<field_type::value_type> types,
          std::vector<value_type> fields)
      : Row{std::move(fields)}, types_{std::move(types)} {}

  std::vector<field_type::value_type> types() const { return types_; }

 private:
  std::vector<field_type::value_type> types_;
};

class SendFileRequest {
 public:
  /**
   * construct a SendFileRequest message.
   *
   * @param filename filename
   */
  SendFileRequest(std::string filename) : filename_{std::move(filename)} {}

  std::string filename() const { return filename_; }

 private:
  std::string filename_;
};

inline bool operator==(const SendFileRequest &a, const SendFileRequest &b) {
  return a.filename() == b.filename();
}

class Statistics {
 public:
  /**
   * construct a Statistics message.
   *
   * @param stats statistics
   */
  Statistics(std::string stats) : stats_{std::move(stats)} {}

  std::string stats() const { return stats_; }

 private:
  std::string stats_;
};

inline bool operator==(const Statistics &a, const Statistics &b) {
  return a.stats() == b.stats();
}

}  // namespace server

namespace client {

class Greeting {
 public:
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
  Greeting(classic_protocol::capabilities::value_type capabilities,
           uint32_t max_packet_size, uint8_t collation, std::string username,
           std::string auth_method_data, std::string schema,
           std::string auth_method_name, std::string attributes)
      : capabilities_{capabilities},
        max_packet_size_{max_packet_size},
        collation_{collation},
        username_{std::move(username)},
        auth_method_data_{std::move(auth_method_data)},
        schema_{std::move(schema)},
        auth_method_name_{std::move(auth_method_name)},
        attributes_{std::string(attributes)} {}

  classic_protocol::capabilities::value_type capabilities() const {
    return capabilities_;
  }

  void capabilities(classic_protocol::capabilities::value_type caps) {
    capabilities_ = caps;
  }

  uint32_t max_packet_size() const noexcept { return max_packet_size_; }
  void max_packet_size(uint32_t sz) noexcept { max_packet_size_ = sz; }

  uint8_t collation() const noexcept { return collation_; }
  void collation(uint8_t coll) noexcept { collation_ = coll; }

  std::string username() const { return username_; }
  void username(const std::string &v) { username_ = v; }

  std::string auth_method_data() const { return auth_method_data_; }
  void auth_method_data(const std::string &v) { auth_method_data_ = v; }

  std::string schema() const { return schema_; }
  void schema(const std::string &schema) { schema_ = schema; }

  /**
   * name of the auth-method that was explicitly set.
   *
   * use classic_protocol::AuthMethod() to get the effective auth-method
   * which may be announced though capability flags (like if
   * capabilities::plugin_auth wasn't set)
   */
  std::string auth_method_name() const { return auth_method_name_; }

  void auth_method_name(const std::string &name) { auth_method_name_ = name; }

  // [key, value]* in Codec<wire::VarString> encoding
  std::string attributes() const { return attributes_; }

  void attributes(const std::string &attrs) { attributes_ = attrs; }

 private:
  classic_protocol::capabilities::value_type capabilities_;
  uint32_t max_packet_size_;
  uint8_t collation_;
  std::string username_;
  std::string auth_method_data_;
  std::string schema_;
  std::string auth_method_name_;
  std::string attributes_;
};

inline bool operator==(const Greeting &a, const Greeting &b) {
  return (a.capabilities() == b.capabilities()) &&
         (a.max_packet_size() == b.max_packet_size()) &&
         (a.collation() == b.collation()) && (a.username() == b.username()) &&
         (a.auth_method_data() == b.auth_method_data()) &&
         (a.schema() == b.schema()) &&
         (a.auth_method_name() == b.auth_method_name()) &&
         (a.attributes() == b.attributes());
}

class Query {
 public:
  /**
   * construct a Query message.
   *
   * @param statement statement to prepare
   */
  Query(std::string statement) : statement_{std::move(statement)} {}

  std::string statement() const { return statement_; }

 private:
  std::string statement_;
};

inline bool operator==(const Query &a, const Query &b) {
  return a.statement() == b.statement();
}

class ListFields {
 public:
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
  ListFields(std::string table_name, std::string wildcard)
      : table_name_{std::move(table_name)}, wildcard_{std::move(wildcard)} {}

  std::string table_name() const { return table_name_; }
  std::string wildcard() const { return wildcard_; }

 private:
  std::string table_name_;
  std::string wildcard_;
};

inline bool operator==(const ListFields &a, const ListFields &b) {
  return a.table_name() == b.table_name() && a.wildcard() == b.wildcard();
}

class InitSchema {
 public:
  /**
   * construct a InitSchema message.
   *
   * @param schema schema to change to
   */
  InitSchema(std::string schema) : schema_{std::move(schema)} {}

  std::string schema() const { return schema_; }

 private:
  std::string schema_;
};

inline bool operator==(const InitSchema &a, const InitSchema &b) {
  return a.schema() == b.schema();
}

class ChangeUser {
 public:
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
  ChangeUser(std::string username, std::string auth_method_data,
             std::string schema, uint16_t collation,
             std::string auth_method_name, std::string attributes)
      : username_{std::move(username)},
        auth_method_data_{std::move(auth_method_data)},
        schema_{std::move(schema)},
        collation_{collation},
        auth_method_name_{std::move(auth_method_name)},
        attributes_{std::move(attributes)} {}

  uint8_t collation() const noexcept { return collation_; }
  std::string username() const { return username_; }
  std::string auth_method_data() const { return auth_method_data_; }
  std::string schema() const { return schema_; }
  std::string auth_method_name() const { return auth_method_name_; }

  // [key, value]* in Codec<wire::VarString> encoding
  std::string attributes() const { return attributes_; }

 private:
  std::string username_;
  std::string auth_method_data_;
  std::string schema_;
  uint16_t collation_;
  std::string auth_method_name_;
  std::string attributes_;
};

inline bool operator==(const ChangeUser &a, const ChangeUser &b) {
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
  Reload(classic_protocol::reload_cmds::value_type cmds) : cmds_{cmds} {}

  classic_protocol::reload_cmds::value_type cmds() const { return cmds_; }

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

class SendFile {
 public:
  /**
   * construct a SendFile message.
   *
   * @param payload payload
   */
  SendFile(std::string payload) : payload_{std::move(payload)} {}

  std::string payload() const { return payload_; }

 private:
  std::string payload_;
};

inline bool operator==(const SendFile &a, const SendFile &b) {
  return a.payload() == b.payload();
}

class StmtPrepare {
 public:
  /**
   * construct a PrepareStmt message.
   *
   * @param statement statement to prepare
   */
  StmtPrepare(std::string statement) : statement_{std::move(statement)} {}

  std::string statement() const { return statement_; }

 private:
  std::string statement_;
};

inline bool operator==(const StmtPrepare &a, const StmtPrepare &b) {
  return a.statement() == b.statement();
}

/**
 * append data to a parameter of a prepared statement.
 */
class StmtParamAppendData {
 public:
  /**
   * construct an append-data-to-parameter message.
   *
   * @param statement_id statement-id to close
   * @param param_id parameter-id to append data to
   * @param data data to append to param_id of statement_id
   */
  StmtParamAppendData(uint32_t statement_id, uint16_t param_id,
                      std::string data)
      : statement_id_{statement_id},
        param_id_{param_id},
        data_{std::move(data)} {}

  uint32_t statement_id() const { return statement_id_; }
  uint16_t param_id() const { return param_id_; }
  std::string data() const { return data_; }

 private:
  uint32_t statement_id_;
  uint16_t param_id_;
  std::string data_;
};

inline bool operator==(const StmtParamAppendData &a,
                       const StmtParamAppendData &b) {
  return a.statement_id() == b.statement_id() && a.param_id() == b.param_id() &&
         a.data() == b.data();
}

/**
 * execute a prepared statement.
 *
 * 'values' raw bytes as encoded by the binary codec
 */
class StmtExecute {
 public:
  using value_type = std::optional<std::string>;

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
              std::vector<classic_protocol::field_type::value_type> types,
              std::vector<value_type> values)
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
  std::vector<classic_protocol::field_type::value_type> types() const {
    return types_;
  }
  std::vector<value_type> values() const { return values_; }

 private:
  uint32_t statement_id_;
  classic_protocol::cursor::value_type flags_;
  uint32_t iteration_count_;
  bool new_params_bound_;
  std::vector<classic_protocol::field_type::value_type> types_;
  std::vector<value_type> values_;
};

inline bool operator==(const StmtExecute &a, const StmtExecute &b) {
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

class AuthMethodData {
 public:
  /**
   * send data for the current auth-method to server.
   *
   * @param auth_method_data data of the auth-method
   */
  AuthMethodData(std::string auth_method_data)
      : auth_method_data_{std::move(auth_method_data)} {}

  std::string auth_method_data() const { return auth_method_data_; }

 private:
  std::string auth_method_data_;
};

inline bool operator==(const AuthMethodData &a, const AuthMethodData &b) {
  return a.auth_method_data() == b.auth_method_data();
}

// switch to Clone Protocol.
//
// response: server::Ok -> clone protocol
// response: server::Error
//
// no content
class Clone {};
}  // namespace client
}  // namespace message
}  // namespace classic_protocol

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

namespace classic_protocol::message::client {
class BinlogDump {
 public:
  using Flags = typename impl::BinlogDump::Flags;

  BinlogDump(stdx::flags<Flags> flags, uint32_t server_id, std::string filename,
             uint32_t position)
      : position_{position},
        flags_{flags},
        server_id_{server_id},
        filename_{std::move(filename)} {}

  [[nodiscard]] stdx::flags<Flags> flags() const { return flags_; }
  [[nodiscard]] uint32_t server_id() const { return server_id_; }
  [[nodiscard]] std::string filename() const { return filename_; }
  [[nodiscard]] uint64_t position() const { return position_; }

 private:
  uint32_t position_;
  stdx::flags<Flags> flags_;
  uint32_t server_id_;
  std::string filename_;
};
}  // namespace classic_protocol::message::client

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

namespace classic_protocol::message::client {

class BinlogDumpGtid {
 public:
  using Flags = typename impl::BinlogDumpGtid::Flags;

  BinlogDumpGtid(stdx::flags<Flags> flags, uint32_t server_id,
                 std::string filename, uint64_t position, std::string sids)
      : flags_{flags},
        server_id_{server_id},
        filename_{std::move(filename)},
        position_{position},
        sids_{std::move(sids)} {}

  [[nodiscard]] stdx::flags<Flags> flags() const { return flags_; }
  [[nodiscard]] uint32_t server_id() const { return server_id_; }
  [[nodiscard]] std::string filename() const { return filename_; }
  [[nodiscard]] uint64_t position() const { return position_; }
  [[nodiscard]] std::string sids() const { return sids_; }

 private:
  stdx::flags<Flags> flags_;
  uint32_t server_id_;
  std::string filename_;
  uint64_t position_;
  std::string sids_;
};

class RegisterReplica {
 public:
  RegisterReplica(uint32_t server_id, std::string hostname,
                  std::string username, std::string password, uint16_t port,
                  uint32_t replication_rank, uint32_t master_id)
      : server_id_{server_id},
        hostname_{std::move(hostname)},
        username_{std::move(username)},
        password_{std::move(password)},
        port_{port},
        replication_rank_{replication_rank},
        master_id_{master_id} {}

  [[nodiscard]] uint32_t server_id() const { return server_id_; }
  [[nodiscard]] std::string hostname() const { return hostname_; }
  [[nodiscard]] std::string username() const { return username_; }
  [[nodiscard]] std::string password() const { return password_; }
  [[nodiscard]] uint16_t port() const { return port_; }
  [[nodiscard]] uint32_t replication_rank() const { return replication_rank_; }
  [[nodiscard]] uint32_t master_id() const { return master_id_; }

 private:
  uint32_t server_id_;
  std::string hostname_;
  std::string username_;
  std::string password_;
  uint16_t port_;
  uint32_t replication_rank_;
  uint32_t master_id_;
};

}  // namespace classic_protocol::message::client

#endif
