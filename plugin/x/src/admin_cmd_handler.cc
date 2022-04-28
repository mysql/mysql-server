/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/admin_cmd_handler.h"

#include <algorithm>
#include <memory>
#include <set>

#include "my_dbug.h"  // NOLINT(build/include_subdir)

#include "plugin/x/protocol/encoders/encoding_xrow.h"
#include "plugin/x/src/admin_cmd_arguments.h"
#include "plugin/x/src/admin_cmd_index.h"
#include "plugin/x/src/helper/get_system_variable.h"
#include "plugin/x/src/helper/sql_commands.h"
#include "plugin/x/src/helper/string_case.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/notice_configuration.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/mysql_function_names.h"
#include "plugin/x/src/ngs/client_list.h"
#include "plugin/x/src/ngs/notice_descriptor.h"
#include "plugin/x/src/ngs/protocol/column_info_builder.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

const char *const Admin_command_handler::k_mysqlx_namespace = "mysqlx";

namespace details {

class Notice_configuration_commiter {
 public:
  using Notice_type = ngs::Notice_type;

 public:
  Notice_configuration_commiter(
      iface::Notice_configuration *notice_configuration)
      : m_notice_configuration(notice_configuration) {}

  bool try_to_mark_notice(const std::string &notice_name) {
    Notice_type out_notice_type;

    if (!m_notice_configuration->get_notice_type_by_name(notice_name,
                                                         &out_notice_type))
      return false;

    m_marked_notices.emplace(out_notice_type);

    return true;
  }

  void commit_marked_notices(const bool should_be_enabled) {
    for (const auto notice_type : m_marked_notices) {
      m_notice_configuration->set_notice(notice_type, should_be_enabled);
    }
  }

 private:
  iface::Notice_configuration *m_notice_configuration;
  std::set<Notice_type> m_marked_notices;
};

}  // namespace details

const Admin_command_handler::Command_handler
    Admin_command_handler::m_command_handler;

Admin_command_handler::Command_handler::Command_handler()
    : std::map<std::string, Method_ptr>{
          {"ping", &Admin_command_handler::ping},
          {"list_clients", &Admin_command_handler::list_clients},
          {"kill_client", &Admin_command_handler::kill_client},
          {"create_collection", &Admin_command_handler::create_collection},
          {"drop_collection", &Admin_command_handler::drop_collection},
          {"ensure_collection", &Admin_command_handler::ensure_collection},
          {"modify_collection_options",
           &Admin_command_handler::modify_collection_options},
          {"get_collection_options",
           &Admin_command_handler::get_collection_options},
          {"create_collection_index",
           &Admin_command_handler::create_collection_index},
          {"drop_collection_index",
           &Admin_command_handler::drop_collection_index},
          {"list_objects", &Admin_command_handler::list_objects},
          {"enable_notices", &Admin_command_handler::enable_notices},
          {"disable_notices", &Admin_command_handler::disable_notices},
          {"list_notices", &Admin_command_handler::list_notices}} {}

ngs::Error_code Admin_command_handler::Command_handler::execute(
    Admin_command_handler *admin, const std::string &command,
    Command_arguments *args) const {
  const_iterator iter = find(command);
  if (iter == end())
    return ngs::Error(ER_X_INVALID_ADMIN_COMMAND, "Invalid %s command %s",
                      k_mysqlx_namespace, command.c_str());

  try {
    auto error = (admin->*(iter->second))(args);
    if (error.error == ER_X_CMD_INVALID_ARGUMENT) {
      return ngs::Error(error.error, "%s for %s command", error.message.c_str(),
                        command.c_str());
    }
    return error;
  } catch (std::exception &e) {
    log_error(ER_XPLUGIN_FAILED_TO_EXECUTE_ADMIN_CMD, command.c_str(),
              e.what());
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }
}

Admin_command_handler::Admin_command_handler(iface::Session *session)
    : m_session(session), m_collection_handler(session, k_mysqlx_namespace) {}

ngs::Error_code Admin_command_handler::execute(const std::string &command,
                                               Command_arguments *args) {
  if (m_session->data_context().password_expired())
    return ngs::Error(ER_MUST_CHANGE_PASSWORD,
                      "You must reset your password using ALTER USER statement "
                      "before executing this statement.");

  if (command.empty()) {
    log_error(ER_XPLUGIN_EMPTY_ADMIN_CMD);
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }

  return m_command_handler.execute(this, to_lower(command), args);
}

/* Stmt: ping
 * No arguments required
 */
ngs::Error_code Admin_command_handler::ping(Command_arguments *args) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_ping);

  ngs::Error_code error = args->end();
  if (error) return error;

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

namespace {

struct Client_data_ {
  iface::Client::Client_id id{0};
  std::string user;
  std::string host;
  uint64_t session{0};
  bool has_session{false};
};

void get_client_data(std::vector<Client_data_> *clients_data,
                     const iface::Session &requesting_session,
                     const iface::Sql_session &da, iface::Client *client) {
  // The client object is handled by different thread,
  // when accessing its session we need to hold it in
  // shared_pointer to be sure that the session is
  // not reseted (by Mysqlx::Session::Reset) in middle
  // of this operations.
  auto session = client->session_shared_ptr();
  Client_data_ c;

  if (session) {
    const std::string user =
        session->state() == iface::Session::State::k_ready
            ? session->data_context().get_authenticated_user_name()
            : "";
    if (requesting_session.can_see_user(user)) {
      c.id = client->client_id_num();
      c.host = client->client_hostname();
      if (!user.empty()) {
        c.user = user;
        c.session = session->data_context().mysql_session_id();
        c.has_session = true;
      }

      clients_data->push_back(c);
    }
  } else if (da.has_authenticated_user_a_super_priv()) {
    c.id = client->client_id_num();
    c.host = client->client_hostname();

    clients_data->push_back(c);
  }
}

}  // namespace

/* Stmt: list_clients
 * No arguments required
 */
ngs::Error_code Admin_command_handler::list_clients(Command_arguments *args) {
  DBUG_TRACE;
  m_session->update_status(&ngs::Common_status_variables::m_stmt_list_clients);

  ngs::Error_code error = args->end();
  if (error) return error;

  std::vector<Client_data_> clients;
  {
    auto &server = m_session->client().server();

    MUTEX_LOCK(lock, server.get_client_exit_mutex());
    std::vector<std::shared_ptr<xpl::iface::Client>> client_list;

    server.get_client_list().get_all_clients(&client_list);

    clients.reserve(client_list.size());

    for (const auto &c : client_list)
      get_client_data(&clients, *m_session, m_session->data_context(), c.get());
  }

  auto &proto = m_session->proto();

  ngs::Column_info_builder column[4]{
      {Mysqlx::Resultset::ColumnMetaData::UINT, "client_id"},
      {Mysqlx::Resultset::ColumnMetaData::BYTES, "user"},
      {Mysqlx::Resultset::ColumnMetaData::BYTES, "host"},
      {Mysqlx::Resultset::ColumnMetaData::UINT, "sql_session"}};

  proto.send_column_metadata(column[0].get());
  proto.send_column_metadata(column[1].get());
  proto.send_column_metadata(column[2].get());
  proto.send_column_metadata(column[3].get());

  for (std::vector<Client_data_>::const_iterator it = clients.begin();
       it != clients.end(); ++it) {
    proto.start_row();
    proto.row_builder()->field_unsigned_longlong(it->id);

    if (it->user.empty())
      proto.row_builder()->field_null();
    else
      proto.row_builder()->field_string(it->user.c_str(), it->user.length());

    if (it->host.empty())
      proto.row_builder()->field_null();
    else
      proto.row_builder()->field_string(it->host.c_str(), it->host.length());

    if (!it->has_session)
      proto.row_builder()->field_null();
    else
      proto.row_builder()->field_unsigned_longlong(it->session);
    proto.send_row();
  }

  proto.send_result_fetch_done();
  proto.send_exec_ok();

  return ngs::Success();
}

/* Stmt: kill_client
 * Required arguments:
 * - id: bigint - the client identification number
 */
ngs::Error_code Admin_command_handler::kill_client(Command_arguments *args) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_kill_client);

  uint64_t cid = 0;

  ngs::Error_code error =
      args->uint_arg({"id"}, &cid, Argument_appearance::k_obligatory).end();
  if (error) return error;

  auto &server = m_session->client().server();
  error = server.kill_client(cid, m_session);

  if (error) return error;

  m_session->proto().send_exec_ok();

  return ngs::Success();
}

/* Stmt: create_collection
 * Required arguments:
 * - name: string - name of created collection
 * - schema: string - name of collection's schema
 * - options: object, optional - additional collection options
 *   - reuse_existing: bool, optional - semantically the same as create table
 *                     if not exists
 *   - validation: object, optional - validation schema options
 *     - schema: object|string, optional - json validation document
 *     - level: string, optional - level of validation {"STRICT"|"OFF"};
 *       default "STRICT"
 */
ngs::Error_code Admin_command_handler::create_collection(
    Command_arguments *args) {
  DBUG_TRACE;
  return m_collection_handler.create_collection(args);
}

/* Stmt: drop_collection
 * Required arguments:
 * - name: string - name of dropped collection
 * - schema: string - name of collection's schema
 */
ngs::Error_code Admin_command_handler::drop_collection(
    Command_arguments *args) {
  DBUG_TRACE;
  return m_collection_handler.drop_collection(args);
}

/* Stmt: create_collection_index
 * Required arguments:
 * - name: string - name of index
 * - collection: string - name of indexed collection
 * - schema: string - name of collection's schema
 * - unique: bool - whether the index should be a unique index
 * - type: string, optional - name of index's type
 *   {"INDEX"|"SPATIAL"|"FULLTEXT"}
 * - fields|constraint: object, list - detailed information for the generated
 *   column
 *   - field|member: string - path to document member for which the index
 *     will be created
 *   - required: bool, optional - whether the generated column will be created
 *     as NOT NULL
 *   - type: string, optional - data type of the indexed values
 *   - options: int, optional - parameter for generation spatial column
 *   - srid: int, optional - parameter for generation spatial column
 *   - array: bool, optional - indexed field is an array of scalars
 *
 * VARCHAR and CHAR are now indexable because:
 * - varchar column needs to be created with a length, which would limit
 *   documents to have that field smaller than that
 * - if we use left() to truncate the value of the column, then the index
 * won't be usable unless queries also specify left(), which is not desired.
 */
ngs::Error_code Admin_command_handler::create_collection_index(
    Command_arguments *args) {
  DBUG_TRACE;
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_create_collection_index);
  return Admin_command_index(m_session).create(args);
}

/* Stmt: drop_collection_index
 * Required arguments:
 * - name: string - name of dropped index
 * - collection: string - name of collection with dropped index
 * - schema: string - name of collection's schema
 */
ngs::Error_code Admin_command_handler::drop_collection_index(
    Command_arguments *args) {
  DBUG_TRACE;
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_drop_collection_index);
  return Admin_command_index(m_session).drop(args);
}

namespace {

static const std::array<const char *const, 4> k_fixed_notice_names = {
    "account_expired",
    "generated_insert_id",
    "rows_affected",
    "produced_message",
};

inline bool is_fixed_notice_name(const std::string &notice) {
  return std::find(k_fixed_notice_names.begin(), k_fixed_notice_names.end(),
                   notice) != k_fixed_notice_names.end();
}

inline void add_notice_row(iface::Protocol_encoder *proto,
                           const std::string &notice, longlong status) {
  proto->start_row();
  proto->row_builder()->field_string(notice.c_str(), notice.length());
  proto->row_builder()->field_signed_longlong(status);
  proto->send_row();
}

}  // namespace

/* Stmt: enable_notices
 * Required arguments:
 * - notice: string, list - name(s) of enabled notice(s)
 */
ngs::Error_code Admin_command_handler::enable_notices(Command_arguments *args) {
  DBUG_TRACE;
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_enable_notices);

  std::vector<std::string> notice_names_to_enable;
  ngs::Error_code error = args->string_list({"notice"}, &notice_names_to_enable,
                                            Argument_appearance::k_obligatory)
                              .end();

  if (error) return error;

  auto notice_configurator = &m_session->get_notice_configuration();
  details::Notice_configuration_commiter new_configuration(notice_configurator);

  for (const auto &name : notice_names_to_enable) {
    if (is_fixed_notice_name(name)) continue;

    if (!new_configuration.try_to_mark_notice(name)) {
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s",
                        name.c_str());
    }
  }

  const bool enable_notices = true;
  new_configuration.commit_marked_notices(enable_notices);

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

/* Stmt: disable_notices
 * Required arguments:
 * - notice: string, list - name (or names) of enabled notice
 */
ngs::Error_code Admin_command_handler::disable_notices(
    Command_arguments *args) {
  DBUG_TRACE;
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_disable_notices);

  std::vector<std::string> notice_names_to_disable;
  ngs::Error_code error =
      args->string_list({"notice"}, &notice_names_to_disable,
                        Argument_appearance::k_obligatory)
          .end();

  if (error) return error;

  auto notice_configurator = &m_session->get_notice_configuration();
  details::Notice_configuration_commiter new_configuration(notice_configurator);

  for (const auto &name : notice_names_to_disable) {
    if (is_fixed_notice_name(name))
      return ngs::Error(ER_X_CANNOT_DISABLE_NOTICE, "Cannot disable notice %s",
                        name.c_str());
    if (!new_configuration.try_to_mark_notice(name)) {
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s",
                        name.c_str());
    }
  }

  const bool disable_notices = false;
  new_configuration.commit_marked_notices(disable_notices);

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

/* Stmt: list_notices
 * No arguments required
 */
ngs::Error_code Admin_command_handler::list_notices(Command_arguments *args) {
  DBUG_TRACE;
  m_session->update_status(&ngs::Common_status_variables::m_stmt_list_notices);
  const auto &notice_config = m_session->get_notice_configuration();

  ngs::Error_code error = args->end();
  if (error) return error;

  // notice | enabled
  // <name> | <1/0>
  auto &proto = m_session->proto();
  ngs::Column_info_builder column[2]{
      {Mysqlx::Resultset::ColumnMetaData::BYTES, "notice"},
      {Mysqlx::Resultset::ColumnMetaData::SINT, "enabled"}};

  proto.send_column_metadata(column[0].get());
  proto.send_column_metadata(column[1].get());

  const auto last_notice_value =
      static_cast<int>(ngs::Notice_type::k_last_element);

  for (int notice_value = 0; notice_value < last_notice_value; ++notice_value) {
    const auto notice_type = static_cast<ngs::Notice_type>(notice_value);
    std::string out_notice_name;

    // Fails in case when notice is not by name.
    if (!notice_config.get_name_by_notice_type(notice_type, &out_notice_name))
      continue;

    add_notice_row(&proto, out_notice_name,
                   notice_config.is_notice_enabled(notice_type) ? 1 : 0);
  }

  for (const auto notice : k_fixed_notice_names) {
    add_notice_row(&proto, notice, 1);
  }

  proto.send_result_fetch_done();
  proto.send_exec_ok();
  return ngs::Success();
}

namespace {
ngs::Error_code is_schema_selected_and_exists(iface::Sql_session *da,
                                              const std::string &schema) {
  Query_string_builder qb;
  qb.put("SHOW TABLES");
  if (!schema.empty()) qb.put(" FROM ").quote_identifier(schema);

  Empty_resultset rset;
  return da->execute_sql(qb.get().data(), qb.get().length(), &rset);
}

}  // namespace

/* Stmt: list_objects
 * Required arguments:
 * - schema: string, optional - name of listed object's schema
 * - pattern: string, optional - a filter to use for matching object names to
 * be returned
 */
ngs::Error_code Admin_command_handler::list_objects(Command_arguments *args) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_list_objects);

  static const bool is_table_names_case_sensitive =
      get_system_variable<int64_t>(&m_session->data_context(),
                                   "lower_case_table_names") == 0l;

  static const char *const BINARY_OPERATOR =
      is_table_names_case_sensitive &&
              get_system_variable<int64_t>(&m_session->data_context(),
                                           "lower_case_file_system") == 0l
          ? "BINARY "
          : "";

  std::string schema, pattern;
  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_optional)
          .string_arg({"pattern"}, &pattern, Argument_appearance::k_optional)
          .end();
  if (error) return error;

  if (!is_table_names_case_sensitive) schema = to_lower(schema);

  error = is_schema_selected_and_exists(&m_session->data_context(), schema);
  if (error) return error;

  Query_string_builder qb;
  qb.put("SELECT ")
      .put(BINARY_OPERATOR)
      .put(
          "T.table_name AS name, "
          "IF(ANY_VALUE(T.table_type) LIKE '%VIEW', "
          "IF(COUNT(*)=1 AND ")
      .put(k_count_doc)
      .put("=1, 'COLLECTION_VIEW', 'VIEW'), IF(")
      .put(k_count_without_schema)
      .put("-2 = ");

  if (m_session->data_context().is_sql_mode_set("NO_BACKSLASH_ESCAPES")) {
    qb.put(k_count_gen_no_backslash_escapes)
        .put(" AND ")
        .put(k_count_doc)
        .put("=1 AND ")
        .put(k_count_id_no_backslash_escapes);
  } else {
    qb.put(k_count_gen)
        .put(" AND ")
        .put(k_count_doc)
        .put("=1 AND ")
        .put(k_count_id);
  }

  qb.put(
        "=1, 'COLLECTION', 'TABLE')) AS type "
        "FROM information_schema.tables AS T "
        "LEFT JOIN information_schema.columns AS C ON (")
      .put(BINARY_OPERATOR)
      .put("T.table_schema = C.table_schema AND ")
      .put(BINARY_OPERATOR)
      .put(
          "T.table_name = C.table_name) "
          "WHERE T.table_schema = ");
  if (schema.empty())
    qb.put("schema()");
  else
    qb.quote_string(schema);
  if (!pattern.empty()) qb.put(" AND T.table_name LIKE ").quote_string(pattern);
  qb.put(" GROUP BY name ORDER BY name");

  log_debug("LIST: %s", qb.get().c_str());
  Streaming_resultset<> resultset(m_session, false);
  error = m_session->data_context().execute_sql(qb.get().data(),
                                                qb.get().length(), &resultset);
  if (error) return error;

  return ngs::Success();
}

/* Stmt: ensure_collection
 * Required arguments:
 * - name: string - name of created collection
 * - schema: string, optional - name of collection's schema
 * - options: object, optional - additional collection options
 */
ngs::Error_code Admin_command_handler::ensure_collection(
    Command_arguments *args) {
  DBUG_TRACE;
  return m_collection_handler.ensure_collection(args);
}

/* Stmt: modify_collection_options
 * Required arguments:
 * - name: string - name of collection
 * - schema: string - name of collection's schema
 * - options: object, optional - additional collection options
 */
ngs::Error_code Admin_command_handler::modify_collection_options(
    Command_arguments *args) {
  DBUG_TRACE;
  return m_collection_handler.modify_collection_options(args);
}

/* Stmt: get_collection_options
 * Required arguments:
 * - name: string - name of collection
 * - schema: string - name of collection's schema
 * - options: string, list - collection options to fetch
 */
ngs::Error_code Admin_command_handler::get_collection_options(
    Command_arguments *args) {
  DBUG_TRACE;
  return m_collection_handler.get_collection_options(args);
}

const char *const Admin_command_handler::Command_arguments::k_placeholder = "?";

}  // namespace xpl
