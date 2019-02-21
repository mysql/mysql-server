/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/interface/notice_configuration_interface.h"
#include "plugin/x/ngs/include/ngs/notice_descriptor.h"
#include "plugin/x/ngs/include/ngs/protocol/column_info_builder.h"
#include "plugin/x/src/admin_cmd_index.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_server.h"

namespace xpl {

const char *const Admin_command_handler::k_mysqlx_namespace = "mysqlx";

namespace details {

class Notice_configuration_commiter {
 public:
  using Notice_type = ngs::Notice_type;
  using Notice_configuration_interface = ngs::Notice_configuration_interface;

 public:
  Notice_configuration_commiter(
      Notice_configuration_interface *notice_configuration)
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
  Notice_configuration_interface *m_notice_configuration;
  std::set<Notice_type> m_marked_notices;
};

inline std::string to_lower(std::string src) {
  std::transform(src.begin(), src.end(), src.begin(), ::tolower);
  return src;
}

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
          {"create_collection_index",
           &Admin_command_handler::create_collection_index},
          {"drop_collection_index",
           &Admin_command_handler::drop_collection_index},
          {"list_objects", &Admin_command_handler::list_objects},
          {"enable_notices", &Admin_command_handler::enable_notices},
          {"disable_notices", &Admin_command_handler::disable_notices},
          {"list_notices", &Admin_command_handler::list_notices}} {}

ngs::Error_code Admin_command_handler::Command_handler::execute(
    Admin_command_handler *admin, const std::string &name_space,
    const std::string &command, Command_arguments *args) const {
  const_iterator iter = find(command);
  if (iter == end())
    return ngs::Error(ER_X_INVALID_ADMIN_COMMAND, "Invalid %s command %s",
                      name_space.c_str(), command.c_str());

  try {
    return (admin->*(iter->second))(details::to_lower(name_space), args);
  } catch (std::exception &e) {
    log_error(ER_XPLUGIN_FAILED_TO_EXECUTE_ADMIN_CMD, command.c_str(),
              e.what());
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }
}

Admin_command_handler::Admin_command_handler(ngs::Session_interface *session)
    : m_session(session) {}

ngs::Error_code Admin_command_handler::execute(const std::string &name_space,
                                               const std::string &command,
                                               Command_arguments *args) {
  if (m_session->data_context().password_expired())
    return ngs::Error(ER_MUST_CHANGE_PASSWORD,
                      "You must reset your password using ALTER USER statement "
                      "before executing this statement.");

  if (command.empty()) {
    log_error(ER_XPLUGIN_EMPTY_ADMIN_CMD);
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }

  return m_command_handler.execute(this, name_space, details::to_lower(command),
                                   args);
}

/* Stmt: ping
 * No arguments required
 */
ngs::Error_code Admin_command_handler::ping(const std::string & /*name_space*/,
                                            Command_arguments *args) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_ping);

  ngs::Error_code error = args->end();
  if (error) return error;

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

namespace {

struct Client_data_ {
  ngs::Client_interface::Client_id id{0};
  std::string user;
  std::string host;
  uint64_t session{0};
  bool has_session{false};
};

void get_client_data(std::vector<Client_data_> *clients_data,
                     const ngs::Session_interface &requesting_session,
                     const ngs::Sql_session_interface &da,
                     ngs::Client_interface *client) {
  // The client object is handled by different thread,
  // when accessing its session we need to hold it in
  // shared_pointer to be sure that the session is
  // not reseted (by Mysqlx::Session::Reset) in middle
  // of this operations.
  auto session = client->session_smart_ptr();
  Client_data_ c;

  if (session) {
    const std::string user =
        session->state() == ngs::Session_interface::k_ready
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
ngs::Error_code Admin_command_handler::list_clients(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_list_clients);

  ngs::Error_code error = args->end();
  if (error) return error;

  std::vector<Client_data_> clients;
  {
    Server::Server_ptr server(Server::get_instance());
    if (server) {
      MUTEX_LOCK(lock, (*server)->server().get_client_exit_mutex());
      std::vector<ngs::Client_ptr> client_list;

      (*server)->server().get_client_list().get_all_clients(client_list);

      clients.reserve(client_list.size());

      for (const auto &c : client_list)
        get_client_data(&clients, *m_session, m_session->data_context(),
                        c.get());
    }
  }

  auto &proto = m_session->proto();

  ngs::Column_info_builder column[4]{
      {Mysqlx::Resultset::ColumnMetaData::UINT, "client_id"},
      {Mysqlx::Resultset::ColumnMetaData::BYTES, "user"},
      {Mysqlx::Resultset::ColumnMetaData::BYTES, "host"},
      {Mysqlx::Resultset::ColumnMetaData::UINT, "sql_session"}};

  proto.send_column_metadata(&column[0].get());
  proto.send_column_metadata(&column[1].get());
  proto.send_column_metadata(&column[2].get());
  proto.send_column_metadata(&column[3].get());

  for (std::vector<Client_data_>::const_iterator it = clients.begin();
       it != clients.end(); ++it) {
    proto.start_row();
    proto.row_builder().add_longlong_field(it->id, true);

    if (it->user.empty())
      proto.row_builder().add_null_field();
    else
      proto.row_builder().add_string_field(it->user.c_str(), it->user.length());

    if (it->host.empty())
      proto.row_builder().add_null_field();
    else
      proto.row_builder().add_string_field(it->host.c_str(), it->host.length());

    if (!it->has_session)
      proto.row_builder().add_null_field();
    else
      proto.row_builder().add_longlong_field(it->session, true);
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
ngs::Error_code Admin_command_handler::kill_client(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_kill_client);

  uint64_t cid = 0;

  ngs::Error_code error = args->uint_arg({"id"}, &cid).end();
  if (error) return error;

  {
    auto server(Server::get_instance());
    if (server) error = (*server)->kill_client(cid, *m_session);
  }
  if (error) return error;

  m_session->proto().send_exec_ok();

  return ngs::Success();
}

namespace {

ngs::Error_code create_collection_impl(ngs::Sql_session_interface *da,
                                       const std::string &schema,
                                       const std::string &name) {
  Query_string_builder qb;
  qb.put("CREATE TABLE ");
  if (!schema.empty()) qb.quote_identifier(schema).dot();
  qb.quote_identifier(name).put(
      " (doc JSON,"
      "_id VARBINARY(32) GENERATED ALWAYS AS "
      "(JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY"
      ") CHARSET utf8mb4 ENGINE=InnoDB;");

  const ngs::PFS_string &tmp(qb.get());
  log_debug("CreateCollection: %s", tmp.c_str());
  Empty_resultset rset;
  return da->execute(tmp.c_str(), tmp.length(), &rset);
}

}  // namespace

/* Stmt: create_collection
 * Required arguments:
 * - name: string - name of created collection
 * - schema: string - name of collection's schema
 */
ngs::Error_code Admin_command_handler::create_collection(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_create_collection);

  std::string schema;
  std::string collection;

  ngs::Error_code error = args->string_arg({"schema"}, &schema)
                              .string_arg({"name"}, &collection)
                              .end();
  if (error) return error;

  if (schema.empty()) return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  error =
      create_collection_impl(&m_session->data_context(), schema, collection);
  if (error) return error;
  m_session->proto().send_exec_ok();
  return ngs::Success();
}

/* Stmt: drop_collection
 * Required arguments:
 * - name: string - name of dropped collection
 * - schema: string - name of collection's schema
 */
ngs::Error_code Admin_command_handler::drop_collection(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_drop_collection);

  Query_string_builder qb;
  std::string schema;
  std::string collection;

  ngs::Error_code error = args->string_arg({"schema"}, &schema)
                              .string_arg({"name"}, &collection)
                              .end();
  if (error) return error;

  if (schema.empty()) return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  qb.put("DROP TABLE ")
      .quote_identifier(schema)
      .dot()
      .quote_identifier(collection);

  const ngs::PFS_string &tmp(qb.get());
  log_debug("DropCollection: %s", tmp.c_str());
  Empty_resultset rset;
  error = m_session->data_context().execute(tmp.data(), tmp.length(), &rset);
  if (error) return error;
  m_session->proto().send_exec_ok();

  return ngs::Success();
}

/* Stmt: create_collection_index
 * Required arguments:
 * - name: string - name of index
 * - collection: string - name of indexed collection
 * - schema: string - name of collection's schema
 * - unique: bool - whether the index should be a unique index
 * - type: string, optional - name of index's type {"INDEX"|"SPATIAL"}
 * - fields|constraint: object, list - detailed information for the generated
 *   column
 *   - field|member: string - path to document member for which the index
 *     will be created
 *   - required: bool - whether the generated column will be created as NOT NULL
 *   - type: string - data type of the generated column
 *   - options: int, optional - parameter for generation spatial column
 *   - srid: int, optional - parameter for generation spatial column
 *
 * VARCHAR and CHAR are now indexable because:
 * - varchar column needs to be created with a length, which would limit
 *   documents to have that field smaller than that
 * - if we use left() to truncate the value of the column, then the index won't
 *   be usable unless queries also specify left(), which is not desired.
 */
ngs::Error_code Admin_command_handler::create_collection_index(
    const std::string &name_space, Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_create_collection_index);
  return Admin_command_index(m_session).create(name_space, args);
}

/* Stmt: drop_collection_index
 * Required arguments:
 * - name: string - name of dropped index
 * - collection: string - name of collection with dropped index
 * - schema: string - name of collection's schema
 */
ngs::Error_code Admin_command_handler::drop_collection_index(
    const std::string &name_space, Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_drop_collection_index);
  return Admin_command_index(m_session).drop(name_space, args);
}

namespace {

static const char *const fixed_notice_names[] = {
    "account_expired", "generated_insert_id", "rows_affected",
    "produced_message"};
static const char *const *fixed_notice_names_end =
    &fixed_notice_names[0] +
    sizeof(fixed_notice_names) / sizeof(fixed_notice_names[0]);

inline bool is_fixed_notice_name(const std::string &notice) {
  return std::find(fixed_notice_names, fixed_notice_names_end, notice) !=
         fixed_notice_names_end;
}

inline void add_notice_row(ngs::Protocol_encoder_interface *proto,
                           const std::string &notice, longlong status) {
  proto->start_row();
  proto->row_builder().add_string_field(notice.c_str(), notice.length());
  proto->row_builder().add_longlong_field(status, 0);
  proto->send_row();
}

}  // namespace

/* Stmt: enable_notices
 * Required arguments:
 * - notice: string, list - name(s) of enabled notice(s)
 */
ngs::Error_code Admin_command_handler::enable_notices(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_enable_notices);

  std::vector<std::string> notice_names_to_enable;
  ngs::Error_code error =
      args->string_list({"notice"}, &notice_names_to_enable).end();

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
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_disable_notices);

  std::vector<std::string> notice_names_to_disable;
  ngs::Error_code error =
      args->string_list({"notice"}, &notice_names_to_disable).end();

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
ngs::Error_code Admin_command_handler::list_notices(
    const std::string & /*name_space*/, Command_arguments *args) {
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

  proto.send_column_metadata(&column[0].get());
  proto.send_column_metadata(&column[1].get());

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

  for (const auto notice : fixed_notice_names) {
    add_notice_row(&proto, notice, 1);
  }

  proto.send_result_fetch_done();
  proto.send_exec_ok();
  return ngs::Success();
}

namespace {
ngs::Error_code is_schema_selected_and_exists(ngs::Sql_session_interface *da,
                                              const std::string &schema) {
  Query_string_builder qb;
  qb.put("SHOW TABLES");
  if (!schema.empty()) qb.put(" FROM ").quote_identifier(schema);

  Empty_resultset rset;
  return da->execute(qb.get().data(), qb.get().length(), &rset);
}

template <typename T>
T get_system_variable(ngs::Sql_session_interface *da,
                      const std::string &variable) {
  xpl::Sql_data_result result(*da);
  try {
    result.query(("SELECT @@" + variable).c_str());
    if (result.size() != 1) {
      log_error(ER_XPLUGIN_FAILED_TO_GET_SYS_VAR, variable.c_str());
      return T();
    }
    T value = T();
    result.get(value);
    return value;
  } catch (const ngs::Error_code &) {
    log_error(ER_XPLUGIN_FAILED_TO_GET_SYS_VAR, variable.c_str());
    return T();
  }
}

#define DOC_ID_REGEX R"(\\$\\._id)"

#define JSON_EXTRACT_REGEX(member) \
  R"(json_extract\\(`doc`,(_[[:alnum:]]+)?\\\\'')" member R"(\\\\''\\))"

#define COUNT_WHEN(expresion) \
  "COUNT(CASE WHEN (" expresion ") THEN 1 ELSE NULL END)"

const char *const COUNT_DOC =
    COUNT_WHEN("column_name = 'doc' AND data_type = 'json'");
const char *const COUNT_ID = COUNT_WHEN(
    R"(column_name = '_id' AND generation_expression RLIKE '^json_unquote\\()" JSON_EXTRACT_REGEX(
        DOC_ID_REGEX) R"(\\)$')");
const char *const COUNT_GEN = COUNT_WHEN(
    "column_name != '_id' AND column_name != 'doc' AND "
    "generation_expression RLIKE '" JSON_EXTRACT_REGEX(DOC_MEMBER_REGEX) "'");
}  // namespace

/* Stmt: list_objects
 * Required arguments:
 * - schema: string, optional - name of listed object's schema
 * - pattern: string, optional - a filter to use for matching object names to be
 * returned
 */
ngs::Error_code Admin_command_handler::list_objects(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_list_objects);

  static const bool is_table_names_case_sensitive =
      get_system_variable<long>(&m_session->data_context(),
                                "lower_case_table_names") == 0l;

  static const char *const BINARY_OPERATOR =
      is_table_names_case_sensitive &&
              get_system_variable<long>(&m_session->data_context(),
                                        "lower_case_file_system") == 0l
          ? "BINARY "
          : "";

  std::string schema, pattern;
  ngs::Error_code error = args->string_arg({"schema"}, &schema, true)
                              .string_arg({"pattern"}, &pattern, true)
                              .end();
  if (error) return error;

  if (!is_table_names_case_sensitive) schema = details::to_lower(schema);

  error = is_schema_selected_and_exists(&m_session->data_context(), schema);
  if (error) return error;

  Query_string_builder qb;
  qb.put("SELECT ")
      .put(BINARY_OPERATOR)
      .put(
          "T.table_name AS name, "
          "IF(ANY_VALUE(T.table_type) LIKE '%VIEW', "
          "IF(COUNT(*)=1 AND ")
      .put(COUNT_DOC)
      .put("=1, 'COLLECTION_VIEW', 'VIEW'), IF(COUNT(*)-2 = ")
      .put(COUNT_GEN)
      .put(" AND ")
      .put(COUNT_DOC)
      .put("=1 AND ")
      .put(COUNT_ID)
      .put(
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
  error = m_session->data_context().execute(qb.get().data(), qb.get().length(),
                                            &resultset);
  if (error) return error;

  return ngs::Success();
}

namespace {
bool is_collection(ngs::Sql_session_interface *da, const std::string &schema,
                   const std::string &name) {
  Query_string_builder qb;
  qb.put("SELECT COUNT(*) AS cnt,")
      .put(COUNT_DOC)
      .put(" AS doc,")
      .put(COUNT_ID)
      .put(" AS id,")
      .put(COUNT_GEN)
      .put(
          " AS gen "
          "FROM information_schema.columns "
          "WHERE table_name = ")
      .quote_string(name)
      .put(" AND table_schema = ");
  if (schema.empty())
    qb.put("schema()");
  else
    qb.quote_string(schema);

  Sql_data_result result(*da);
  try {
    result.query(qb.get());
    if (result.size() != 1) {
      log_debug(
          "Unable to recognize '%s' as a collection; query result size: %llu",
          std::string(schema.empty() ? name : schema + "." + name).c_str(),
          static_cast<unsigned long long>(result.size()));
      return false;
    }
    long cnt = 0, doc = 0, id = 0, gen = 0;
    result.get(cnt).get(doc).get(id).get(gen);
    return doc == 1 && id == 1 && (cnt == gen + doc + id);
  }
#if defined(XPLUGIN_LOG_DEBUG) && !defined(XPLUGIN_DISABLE_LOG)
  catch (const ngs::Error_code &e) {
#else
  catch (const ngs::Error_code &) {
#endif
    log_debug(
        "Unable to recognize '%s' as a collection; exception message: '%s'",
        std::string(schema.empty() ? name : schema + "." + name).c_str(),
        e.message.c_str());
    return false;
  }
}

}  // namespace

/* Stmt: ensure_collection
 * Required arguments:
 * - name: string - name of created collection
 * - schema: string, optional - name of collection's schema
 */
ngs::Error_code Admin_command_handler::ensure_collection(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_ensure_collection);
  std::string schema;
  std::string collection;

  ngs::Error_code error = args->string_arg({"schema"}, &schema, true)
                              .string_arg({"name"}, &collection)
                              .end();
  if (error) return error;

  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  error =
      create_collection_impl(&m_session->data_context(), schema, collection);
  if (error) {
    if (error.error != ER_TABLE_EXISTS_ERROR) return error;
    if (!is_collection(&m_session->data_context(), schema, collection))
      return ngs::Error(
          ER_X_INVALID_COLLECTION, "Table '%s' exists but is not a collection",
          (schema.empty() ? collection : schema + '.' + collection).c_str());
  }
  m_session->proto().send_exec_ok();
  return ngs::Success();
}

const char *const Admin_command_handler::Command_arguments::k_placeholder = "?";

}  // namespace xpl
