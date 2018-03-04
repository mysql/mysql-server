/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/protocol/column_info_builder.h"
#include "plugin/x/src/admin_cmd_index.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_server.h"

namespace xpl {

const char *const Admin_command_handler::MYSQLX_NAMESPACE = "mysqlx";

namespace {

inline std::string to_lower(std::string src) {
  std::transform(src.begin(), src.end(), src.begin(), ::tolower);
  return src;
}

}  // namespace

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
    return (admin->*(iter->second))(to_lower(name_space), args);
  }
  catch (std::exception &e) {
    log_error("Error executing admin command %s: %s", command.c_str(),
              e.what());
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }
}

Admin_command_handler::Admin_command_handler(Session *session)
    : m_session(session) {}

ngs::Error_code Admin_command_handler::execute(const std::string &name_space,
                                               const std::string &command,
                                               Command_arguments *args) {
  if (m_session->data_context().password_expired())
    return ngs::Error(ER_MUST_CHANGE_PASSWORD,
                      "You must reset your password using ALTER USER statement "
                      "before executing this statement.");

  if (command.empty()) {
    log_error("Error executing empty admin command");
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }

  return m_command_handler.execute(this, name_space, to_lower(command), args);
}

/* Stmt: ping
 * No arguments required
 */
ngs::Error_code Admin_command_handler::ping(const std::string & /*name_space*/,
                                            Command_arguments *args) {
  m_session->update_status<&Common_status_variables::m_stmt_ping>();

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
                     const Session &requesting_session,
                     const ngs::Sql_session_interface &da,
                     ngs::Client_interface *client) {
  std::shared_ptr<Session> session(
      ngs::static_pointer_cast<Session>(client->session()));
  Client_data_ c;

  if (session) {
    const std::string user =
        session->is_ready()
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
  m_session->update_status<&Common_status_variables::m_stmt_list_clients>();

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
    {Mysqlx::Resultset::ColumnMetaData::UINT, "sql_session"}
  };

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
      proto.row_builder().add_string_field(it->user.c_str(), it->user.length(),
                                           nullptr);

    if (it->host.empty())
      proto.row_builder().add_null_field();
    else
      proto.row_builder().add_string_field(it->host.c_str(), it->host.length(),
                                           nullptr);

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
  m_session->update_status<&Common_status_variables::m_stmt_kill_client>();

  uint64_t cid = 0;

  ngs::Error_code error = args->uint_arg("id", &cid).end();
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
  qb.quote_identifier(name)
      .put(" (doc JSON,"
           "_id VARCHAR(32) GENERATED ALWAYS AS "
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
  m_session
      ->update_status<&Common_status_variables::m_stmt_create_collection>();

  std::string schema;
  std::string collection;

  ngs::Error_code error =
      args->string_arg("schema", &schema).string_arg("name", &collection).end();
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
  m_session->update_status<&Common_status_variables::m_stmt_drop_collection>();

  Query_string_builder qb;
  std::string schema;
  std::string collection;

  ngs::Error_code error =
      args->string_arg("schema", &schema).string_arg("name", &collection).end();
  if (error) return error;

  if (schema.empty()) return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  qb.put("DROP TABLE ").quote_identifier(schema).dot().quote_identifier(
      collection);

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
 * - constraint: object, list - detailed information for the generated column
 *   - member: string - path to document member for which the index will be
 *     created
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
  m_session->update_status<
      &Common_status_variables::m_stmt_create_collection_index>();
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
  m_session
      ->update_status<&Common_status_variables::m_stmt_drop_collection_index>();
  return Admin_command_index(m_session).drop(name_space, args);
}

namespace {

static const char *const fixed_notice_names[] = {
    "account_expired", "generated_insert_id",
    "rows_affected",   "produced_message"};
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
  proto->row_builder().add_string_field(notice.c_str(), notice.length(),
                                        nullptr);
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
  m_session->update_status<&Common_status_variables::m_stmt_enable_notices>();

  std::vector<std::string> notices;
  ngs::Error_code error = args->string_list("notice", &notices).end();
  if (error) return error;

  bool enable_warnings = false;
  for (const std::string &n : notices) {
    if (n == "warnings")
      enable_warnings = true;
    else if (!is_fixed_notice_name(n))
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s", n.c_str());
  }
  // so far only warnings notices are switchable
  if (enable_warnings) m_session->options().set_send_warnings(true);

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

/* Stmt: disable_notices
 * Required arguments:
 * - notice: string, list - name (or names) of enabled notice
 */
ngs::Error_code Admin_command_handler::disable_notices(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status<&Common_status_variables::m_stmt_disable_notices>();

  std::vector<std::string> notices;
  ngs::Error_code error = args->string_list("notice", &notices).end();
  if (error) return error;

  bool disable_warnings = false;
  for (std::vector<std::string>::const_iterator i = notices.begin();
       i != notices.end(); ++i) {
    if (*i == "warnings")
      disable_warnings = true;
    else if (is_fixed_notice_name(*i))
      return ngs::Error(ER_X_CANNOT_DISABLE_NOTICE, "Cannot disable notice %s",
                        i->c_str());
    else
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s", i->c_str());
  }

  if (disable_warnings) m_session->options().set_send_warnings(false);

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

/* Stmt: list_notices
 * No arguments required
 */
ngs::Error_code Admin_command_handler::list_notices(
    const std::string & /*name_space*/, Command_arguments *args) {
  m_session->update_status<&Common_status_variables::m_stmt_list_notices>();

  ngs::Error_code error = args->end();
  if (error) return error;

  // notice | enabled
  // <name> | <1/0>
  auto &proto = m_session->proto();
  ngs::Column_info_builder column[2] {
    { Mysqlx::Resultset::ColumnMetaData::BYTES, "notice" },
    { Mysqlx::Resultset::ColumnMetaData::SINT, "enabled" }
  };

  proto.send_column_metadata(&column[0].get());
  proto.send_column_metadata(&column[1].get());

  add_notice_row(&proto, "warnings",
                 m_session->options().get_send_warnings() ? 1 : 0);
  for (const char *const *notice = fixed_notice_names;
       notice < fixed_notice_names_end; ++notice)
    add_notice_row(&proto, *notice, 1);

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
      log_error("Unable to retrieve system variable '%s'", variable.c_str());
      return T();
    }
    T value = T();
    result.get(value);
    return value;
  }
  catch (const ngs::Error_code &) {
    log_error("Unable to retrieve system variable '%s'", variable.c_str());
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
    R"(column_name = '_id' AND generation_expression RLIKE '^json_unquote\\()"
    JSON_EXTRACT_REGEX(DOC_ID_REGEX) R"(\\)$')");
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
  m_session->update_status<&Common_status_variables::m_stmt_list_objects>();

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
  ngs::Error_code error = args->string_arg("schema", &schema, true)
                              .string_arg("pattern", &pattern, true)
                              .end();
  if (error) return error;

  if (!is_table_names_case_sensitive) schema = to_lower(schema);

  error = is_schema_selected_and_exists(&m_session->data_context(), schema);
  if (error) return error;

  Query_string_builder qb;
  qb.put("SELECT ")
      .put(BINARY_OPERATOR)
      .put("T.table_name AS name, "
           "IF(ANY_VALUE(T.table_type) LIKE '%VIEW', "
           "IF(COUNT(*)=1 AND ")
      .put(COUNT_DOC)
      .put("=1, 'COLLECTION_VIEW', 'VIEW'), IF(COUNT(*)-2 = ")
      .put(COUNT_GEN)
      .put(" AND ")
      .put(COUNT_DOC)
      .put("=1 AND ")
      .put(COUNT_ID)
      .put("=1, 'COLLECTION', 'TABLE')) AS type "
           "FROM information_schema.tables AS T "
           "LEFT JOIN information_schema.columns AS C ON (")
      .put(BINARY_OPERATOR)
      .put("T.table_schema = C.table_schema AND ")
      .put(BINARY_OPERATOR)
      .put("T.table_name = C.table_name) "
           "WHERE T.table_schema = ");
  if (schema.empty())
    qb.put("schema()");
  else
    qb.quote_string(schema);
  if (!pattern.empty()) qb.put(" AND T.table_name LIKE ").quote_string(pattern);
  qb.put(" GROUP BY name ORDER BY name");

  log_debug("LIST: %s", qb.get().c_str());
  Streaming_resultset resultset(&m_session->proto(), false);
  error = m_session->data_context().execute(qb.get().data(), qb.get().length(),
                                            &resultset);
  if (error) return error;

  m_session->proto().send_exec_ok();
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
      .put(" AS gen "
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
  m_session
      ->update_status<&Common_status_variables::m_stmt_ensure_collection>();
  std::string schema;
  std::string collection;

  ngs::Error_code error = args->string_arg("schema", &schema, true)
                              .string_arg("name", &collection)
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

const char *const Admin_command_handler::Command_arguments::PLACEHOLDER = "?";

}  // namespace xpl
