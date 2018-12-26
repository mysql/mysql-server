/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "admin_cmd_handler.h"
#include "xpl_error.h"
#include "sql_data_context.h"
#include "query_string_builder.h"
#include "mysql/service_my_snprintf.h"
#include "ngs/protocol/row_builder.h"
#include "sql_data_result.h"
#include "ngs/mysqlx/getter_any.h"
#include "sha1.h"
#include "password.h"

#include "mysqlx_resultset.pb.h"
#include "mysqlx_datatypes.pb.h"
#include "mysqlx_sql.pb.h"

#include "xpl_regex.h"
#include "xpl_session.h"
#include "xpl_log.h"
#include "xpl_server.h"
#include <algorithm>


namespace
{

struct Index_field_traits
{
  bool is_binary;
  bool unsigned_allowed;
  bool unquote;
  bool prefix_len_allowed;
  std::string v_col_prefix;

  Index_field_traits(bool b, bool ua, bool u, bool pa, const std::string &pref)
  : is_binary(b), unsigned_allowed(ua), unquote(u),
    prefix_len_allowed(pa), v_col_prefix(pref)
  {}

  Index_field_traits()
  : is_binary(false), unsigned_allowed(false), unquote(false),
    prefix_len_allowed(false), v_col_prefix("")
  {}
};


inline std::string to_lower(std::string src)
{
  std::transform(src.begin(), src.end(), src.begin(), ::tolower);
  return src;
}

} // namespace


const xpl::Admin_command_handler::Command_handler xpl::Admin_command_handler::m_command_handler;


xpl::Admin_command_handler::Command_handler::Command_handler()
{
  (*this)["ping"] = &Admin_command_handler::ping;

  (*this)["list_clients"] = &Admin_command_handler::list_clients;
  (*this)["kill_client"] = &Admin_command_handler::kill_client;

  (*this)["create_collection"] = &Admin_command_handler::create_collection;
  (*this)["drop_collection"] = &Admin_command_handler::drop_collection;
  (*this)["ensure_collection"] = &Admin_command_handler::ensure_collection;

  (*this)["create_collection_index"] = &Admin_command_handler::create_collection_index;
  (*this)["drop_collection_index"] = &Admin_command_handler::drop_collection_index;

  (*this)["list_objects"] = &Admin_command_handler::list_objects;

  (*this)["enable_notices"] = &Admin_command_handler::enable_notices;
  (*this)["disable_notices"] = &Admin_command_handler::disable_notices;
  (*this)["list_notices"] = &Admin_command_handler::list_notices;
}


ngs::Error_code xpl::Admin_command_handler::Command_handler::execute(Admin_command_handler *admin,
                                                                     const std::string &namespace_,
                                                                     const std::string &command,
                                                                     Command_arguments &args) const
{
  const_iterator iter = find(command);
  if (iter == end())
    return ngs::Error(ER_X_INVALID_ADMIN_COMMAND, "Invalid %s command %s", namespace_.c_str(), command.c_str());

  try
  {
    return (admin->*(iter->second))(args);
  }
  catch (std::exception &e)
  {
    log_error("Error executing admin command %s: %s", command.c_str(), e.what());
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }
}


xpl::Admin_command_handler::Admin_command_handler(Session &session)
: m_session(session), m_da(session.data_context()), m_options(session.options())
{}


ngs::Error_code xpl::Admin_command_handler::execute(const std::string &namespace_, const std::string &command,
                                                    Command_arguments &args)
{
  if (m_da.password_expired())
    return ngs::Error(ER_MUST_CHANGE_PASSWORD,
                      "You must reset your password using ALTER USER statement before executing this statement.");

  if (command.empty())
  {
    log_error("Error executing empty admin command");
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }

  return m_command_handler.execute(this, namespace_, to_lower(command), args);
}



/* Stmt: ping
 * No arguments required
 */
ngs::Error_code xpl::Admin_command_handler::ping(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_ping>();

  ngs::Error_code error = args.end();
  if (error)
    return error;

  m_da.proto().send_exec_ok();
  return ngs::Success();
}


namespace
{

struct Client_data_
{
  uint64_t id;
  std::string user;
  std::string host;
  uint64_t session;
  bool has_session;

  Client_data_() : id(0), session(0), has_session(false) {}
};


void get_client_data(std::vector<Client_data_> &clients_data, xpl::Session &requesting_session,
                     xpl::Sql_data_context &da, ngs::Client_ptr &client)
{
  ngs::shared_ptr<xpl::Session> session(ngs::static_pointer_cast<xpl::Session>(client->session()));
  Client_data_ c;

  if (session)
  {
    const std::string user = session->is_ready() ? session->data_context().get_authenticated_user_name() : "";
    if (requesting_session.can_see_user(user))
    {
      c.id = static_cast<long>(client->client_id_num());
      c.host = client->client_hostname();
      if (!user.empty())
      {
        c.user = user;
        c.session = session->data_context().mysql_session_id();
        c.has_session = true;
      }

      clients_data.push_back(c);
    }
  }
  else if (da.has_authenticated_user_a_super_priv())
  {
    c.id = static_cast<long>(client->client_id_num());
    c.host = client->client_hostname();

    clients_data.push_back(c);
  }
}

} // namespace


/* Stmt: list_clients
 * No arguments required
 */
ngs::Error_code xpl::Admin_command_handler::list_clients(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_list_clients>();

  ngs::Error_code error = args.end();
  if (error)
    return error;

  std::vector<Client_data_> clients;
  {
    Server::Server_ref server(Server::get_instance());
    if (server)
    {
      Mutex_lock lock((*server)->server().get_client_exit_mutex());
      std::vector<ngs::Client_ptr> client_list;

      (*server)->server().get_client_list().get_all_clients(client_list);

      clients.reserve(client_list.size());

      std::for_each(client_list.begin(), client_list.end(),
                    ngs::bind(get_client_data, ngs::ref(clients), ngs::ref(m_session), ngs::ref(m_da), ngs::placeholders::_1));
    }
  }

  ngs::Protocol_encoder &proto(m_da.proto());

  proto.send_column_metadata("", "", "", "", "client_id", "", 0, Mysqlx::Resultset::ColumnMetaData::UINT, 0, 0, 0);
  proto.send_column_metadata("", "", "", "", "user", "", 0, Mysqlx::Resultset::ColumnMetaData::BYTES, 0, 0, 0);
  proto.send_column_metadata("", "", "", "", "host", "", 0, Mysqlx::Resultset::ColumnMetaData::BYTES, 0, 0, 0);
  proto.send_column_metadata("", "", "", "", "sql_session", "", 0, Mysqlx::Resultset::ColumnMetaData::UINT, 0, 0, 0);

  for (std::vector<Client_data_>::const_iterator it = clients.begin(); it != clients.end(); ++it)
  {
    proto.start_row();
    proto.row_builder().add_longlong_field(it->id, true);

    if (it->user.empty())
      proto.row_builder().add_null_field();
    else
      proto.row_builder().add_string_field(it->user.c_str(), it->user.length(), NULL);

    if (it->host.empty())
      proto.row_builder().add_null_field();
    else
      proto.row_builder().add_string_field(it->host.c_str(), it->host.length(), NULL);

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
ngs::Error_code xpl::Admin_command_handler::kill_client(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_kill_client>();

  uint64_t cid = 0;

  ngs::Error_code error = args.uint_arg("id", cid).end();
  if (error)
    return error;

  {
    xpl::Server::Server_ref server(Server::get_instance());
    if (server)
      error = (*server)->kill_client(cid, m_session);
  }
  if (error)
    return error;

  m_da.proto().send_exec_ok();

  return ngs::Success();
}

namespace
{

ngs::Error_code create_collection_impl(xpl::Sql_data_context &da, const std::string &schema, const std::string &name)
{
  xpl::Query_string_builder qb;
  qb.put("CREATE TABLE ");
  if (!schema.empty())
    qb.quote_identifier(schema).dot();
  qb.quote_identifier(name)
    .put(" (doc JSON,"
         "_id VARCHAR(32) GENERATED ALWAYS AS (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY"
         ") CHARSET utf8mb4 ENGINE=InnoDB;");

  xpl::Sql_data_context::Result_info info;
  const ngs::PFS_string &tmp(qb.get());
  log_debug("CreateCollection: %s", tmp.c_str());
  return da.execute_sql_no_result(tmp.c_str(), tmp.length(), info);
}

} // namespace


/* Stmt: create_collection
 * Required arguments:
 * - name: string - name of created collection
 * - schema: string - name of collection's schema
 */
ngs::Error_code xpl::Admin_command_handler::create_collection(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_create_collection>();

  std::string schema;
  std::string collection;

  ngs::Error_code error = args.string_arg("schema", schema).string_arg("name", collection).end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  error = create_collection_impl(m_da, schema, collection);
  if (error)
    return error;
  m_da.proto().send_exec_ok();
  return ngs::Success();
}


namespace
{
/*
 * valid input examples:
 * DECIMAL
 * DECIMAL UNSIGNED
 * DECIMAL(10)
 * DECIMAL(10) UNSIGNED
 * DECIMAL(10,5)
 * DECIMAL(10,5) UNSIGNED
 */
bool parse_type(const std::string &s, std::string &r_type, int &r_arg, int &r_arg2, bool &r_uns)
{
  std::string::const_iterator c = s.begin();
  for (; c != s.end() && isalpha(*c); ++c)
    r_type.push_back(toupper(*c));
  if (c != s.end())
  {
    int consumed;
    if (sscanf(s.c_str() + (c - s.begin()), "(%i,%i)%n", &r_arg, &r_arg2, &consumed) == 2)
      c += consumed;
    else if (sscanf(s.c_str() + (c - s.begin()), "(%i)%n", &r_arg, &consumed) == 1)
      c += consumed;
    // skip potential spaces
    while (c != s.end() && isspace(*c))
      c++;

    std::string ident;
    for (; c != s.end() && isalpha(*c); ++c)
      ident.push_back(toupper(*c));

    r_uns = false;
    if (ident == "UNSIGNED")
      r_uns = true;
    else
    {
      if (!ident.empty())
        return false;
    }

    if (c != s.end())
      return false;
  }
  return true;
}


std::string get_type_prefix(const std::string &prefix, int type_arg, int type_arg2, bool is_unsigned, bool required)
{
  std::stringstream result;
  std::string traits;

  // type
  result << "ix_" << prefix;
  if (type_arg > 0)
    result << type_arg;
  if (type_arg2 > 0)
    result << "_" << type_arg2;

  // additional traits (unsigned, required, ...)
  if (is_unsigned)
    traits += "u";
  if (required)
    traits += "r";
  if (!traits.empty())
    result << "_" << traits;

  result << "_";

  return result.str();
}


typedef std::list<std::vector<std::string> > String_fields_values;


ngs::Error_code query_string_columns(xpl::Sql_data_context &da, const ngs::PFS_string &sql,
                                     std::vector<unsigned> &field_idxs, String_fields_values &ret_values)
{
  xpl::Buffering_command_delegate::Resultset r_rows;
  std::vector<xpl::Command_delegate::Field_type> r_types;
  xpl::Sql_data_context::Result_info r_info;

  ngs::Error_code err = da.execute_sql_and_collect_results(sql.data(), sql.length(), r_types, r_rows, r_info);
  if (err)
    return err;

  ret_values.clear();
  size_t fields_number = field_idxs.size();
  xpl::Buffering_command_delegate::Resultset::iterator it = r_rows.begin();
  for (; it != r_rows.end(); ++it)
  {
    ret_values.push_back(std::vector<std::string>(fields_number));
    for (size_t i = 0; i < field_idxs.size(); ++i)
    {
      unsigned field_idx = field_idxs[i];

      xpl::Buffering_command_delegate::Row_data *row_data = &(*it);
      if ((!row_data) || (row_data->fields.size() <= field_idx))
      {
        log_error("query_string_columns failed: invalid row data");
        return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
      }

      xpl::Buffering_command_delegate::Field_value *field = row_data->fields[field_idx];
      if (!field)
      {
        log_error("query_string_columns failed: missing row data");
        return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
      }

      if (MYSQL_TYPE_VARCHAR != r_types[field_idx].type &&
          MYSQL_TYPE_STRING != r_types[field_idx].type &&
          MYSQL_TYPE_TINY_BLOB != r_types[field_idx].type &&
          MYSQL_TYPE_MEDIUM_BLOB != r_types[field_idx].type &&
          MYSQL_TYPE_LONG_BLOB != r_types[field_idx].type &&
          MYSQL_TYPE_BLOB != r_types[field_idx].type &&
          MYSQL_TYPE_VAR_STRING != r_types[field_idx].type)
      {
        log_error("query_string_columns failed: invalid field type");
        return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
      }

      ret_values.back()[i] = *field->value.v_string;
    }
  }

  return ngs::Success();
}


bool name_is(const std::vector<std::string> &field, const std::string &name)
{
  return field[0] == name;
}


ngs::Error_code remove_nonvirtual_column_names(const std::string &schema_name, const std::string &table_name,
                                               String_fields_values &ret_column_names, xpl::Sql_data_context &da)
{
  xpl::Query_string_builder qb;
  const unsigned FIELD_COLMN_IDX = 0;
  const unsigned EXTRA_COLMN_IDX = 5;

  if (ret_column_names.size() == 0)
    return ngs::Success();

  qb.put("SHOW COLUMNS FROM ")
      .quote_identifier(schema_name).dot().quote_identifier(table_name)
      .put(" WHERE Field IN (");
  String_fields_values::const_iterator it_columns = ret_column_names.begin();
  for (;;)
  {
    qb.quote_string((*it_columns)[0]);
    if (++it_columns != ret_column_names.end())
      qb.put(",");
    else
      break;
  }
  qb.put(")");

  std::vector<unsigned> fields_ids(2);
  fields_ids[0] = FIELD_COLMN_IDX;
  fields_ids[1] = EXTRA_COLMN_IDX;
  String_fields_values column_descs;

  ngs::Error_code error = query_string_columns(da, qb.get(), fields_ids, column_descs);
  if (error)
    return error;

  String_fields_values::const_iterator it_field = column_descs.begin();
  for (; it_field != column_descs.end(); ++it_field)
  {
    std::string column_name = (*it_field)[0];
    std::string column_desc = (*it_field)[1];
    if (!(column_desc.find("VIRTUAL GENERATED") != std::string::npos))
      ret_column_names.remove_if(ngs::bind(name_is, ngs::placeholders::_1, column_name));
  }

  return ngs::Success();
}


ngs::Error_code index_on_virtual_column_supported(const std::string &schema_name, const std::string &table_name,
                                                  xpl::Sql_data_context &da, bool &r_supports)
{
  const unsigned CREATE_COLMN_IDX = 1;
  xpl::Query_string_builder qb;
  std::vector<unsigned> fields_ids(1);
  fields_ids[0] = CREATE_COLMN_IDX;
  String_fields_values create_stmts;

  qb.put("SHOW CREATE TABLE ").quote_identifier(schema_name).dot().quote_identifier(table_name);
  ngs::Error_code error = query_string_columns(da, qb.get(), fields_ids, create_stmts);
  if (error)
    return error;

  // if query didn't fail it should return 1 row
  if (create_stmts.size() != 1)
  {
    const unsigned int num_of_rows = static_cast<unsigned int>(create_stmts.size());
    log_error("index_on_virtual_column_supported() failed: wrong number of rows: %u", num_of_rows);
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }

  String_fields_values::const_iterator it_create_stmt = create_stmts.begin();
  std::string create_stmt = (*it_create_stmt)[0];
  size_t pos = create_stmt.find("ENGINE=");
  if (pos == std::string::npos)
  {
    log_error("index_on_virtual_column_supported() failed: no engine info: %s", create_stmt.c_str());
    return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
  }
  std::string engine;
  std::string::const_iterator ci = create_stmt.begin() + pos + strlen("ENGINE=");
  for (; ci != create_stmt.end() && isalpha(*ci); ++ci)
    engine.push_back(*ci);

  // currently only InnoDB supports VIRTUAL GENERATED columns
  r_supports = (engine == "InnoDB");

  return ngs::Success();
}


bool table_column_exists(const std::string &schema_name, const std::string &table_name,
                         const std::string &column_name, xpl::Sql_data_context &da, bool &r_exists)
{
  xpl::Query_string_builder qb;
  xpl::Buffering_command_delegate::Resultset r_rows;
  std::vector<xpl::Command_delegate::Field_type> r_types;
  xpl::Sql_data_context::Result_info r_info;

  qb.put("SHOW COLUMNS FROM ")
      .quote_identifier(schema_name).dot().quote_identifier(table_name)
      .put(" WHERE Field = ").quote_string(column_name);

  ngs::Error_code err = da.execute_sql_and_collect_results(qb.get().data(), qb.get().length(), r_types, r_rows, r_info);
  if (err)
    return false;

  r_exists = r_rows.size() > 0;
  return true;
}


std::string hash_column_name(const std::string &name)
{
  std::string hash;
  hash.resize(2*SHA1_HASH_SIZE + 2);
  // just an arbitrary hash
  ::make_scrambled_password(&hash[0], name.c_str());
  hash.resize(2*SHA1_HASH_SIZE + 1); // strip the \0
  return hash.substr(1); // skip the 1st char
}

} // namespace


/* Stmt: create_collection_index
 * Required arguments:
 * - name: string - name of index
 * - collection: string - name of indexed collection
 * - schema: string - name of collection's schema
 * - unique: bool - whether the index should be a unique index
 * - constraint: object, list - detailed information for the generated column
 *   - member: string - path to document member for which the index will be created
 *   - required: bool - whether the generated column will be created as NOT NULL
 *   - type: string - data type of the created index
 *
 * VARCHAR and CHAR are now indexable because:
 * - varchar column needs to be created with a length, which would limit documents to have
 *  that field smaller than that
 * - if we use left() to truncate the value of the column, then the index won't be usable unless
 *  queries also specify left(), which is not desired.
 */
ngs::Error_code xpl::Admin_command_handler::create_collection_index(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_create_collection_index>();

  Query_string_builder qb;
  bool required = false;
  typedef Index_field_traits _T;
  static std::map<std::string, _T> valid_types;

  static struct Valid_type_init
  {
    Valid_type_init()
    {
      //                             binary   unsigned  unqote prefix_len column_prefix
      valid_types["TINYINT"] =    _T(false,   true,     false, false,     "it");
      valid_types["SMALLINT"] =   _T(false,   true,     false, false,     "is");
      valid_types["MEDIUMINT"] =  _T(false,   true,     false, false,     "im");
      valid_types["INT"] =        _T(false,   true,     false, false,     "i");
      valid_types["INTEGER"] =    _T(false,   true,     false, false,     "i");
      valid_types["BIGINT"] =     _T(false,   true,     false, false,     "ib");
      valid_types["REAL"] =       _T(false,   true,     false, false,     "fr");
      valid_types["FLOAT"] =      _T(false,   true,     false, false,     "f");
      valid_types["DOUBLE"] =     _T(false,   true,     false, false,     "fd");
      valid_types["DECIMAL"] =    _T(false,   true,     false, false,     "xd");
      valid_types["NUMERIC"] =    _T(false,   true,     false, false,     "xn");
      valid_types["DATE"] =       _T(false,   false,    true,  false,     "d");
      valid_types["TIME"] =       _T(false,   false,    true,  false,     "dt");
      valid_types["TIMESTAMP"] =  _T(false,   false,    true,  false,     "ds");
      valid_types["DATETIME"] =   _T(false,   false,    true,  false,     "dd");
      valid_types["YEAR"] =       _T(false,   false,    true,  false,     "dy");
      valid_types["BIT"] =        _T(false,   false,    true,  true,      "t");
      valid_types["BLOB"] =       _T(true,    false,    true,  true,      "bt");
      valid_types["TEXT"] =       _T(true,    false,    true,  true,      "t");
    }
  } _type_init;

  std::string schema;
  std::string collection;
  std::string index_name;
  bool unique = false;
  std::vector<Command_arguments*> constraints;

  ngs::Error_code error = args.string_arg("schema", schema)
          .string_arg("collection", collection)
          .string_arg("name", index_name)
          .bool_arg("unique", unique)
          .object_list("constraint", constraints)
          .error();
  if (error)
    return error;

  std::vector<std::string> col_field_path;
  std::vector<std::string> col_raw_type;
  std::vector<bool> col_required;
  bool column_exists = false;
  typedef std::vector<Command_arguments*>::iterator It;
  for (It i = constraints.begin(); i != constraints.end(); ++i)
  {
    std::string f, t;
    bool r = false;
    error = (*i)->docpath_arg("member", f)
            .string_arg("type", t)
            .bool_arg("required", r)
            .error();
    if (error)
      return error;
    if (f.empty())
      return ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Argument value '%s' for document member is invalid", f.c_str());
    col_field_path.push_back(f);
    col_raw_type.push_back(t);
    col_required.push_back(r);
    required = required || r;
  }
  error = args.end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error(ER_X_BAD_SCHEMA, "Invalid schema '%s'", schema.c_str());
  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name '%s'", collection.c_str());
  if (index_name.empty())
    return ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Argument value '%s' for index name is invalid", index_name.c_str());

  // check if the table's engine supports index on the virtual column
  bool virtual_supported = false;
  error = index_on_virtual_column_supported(schema, collection, m_da, virtual_supported);
  if (error)
  {
    if (error.error == ER_INTERNAL_ERROR)
      return error;
    else
      // if it is not internal then the reason is bad schema or table name
      return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name: %s.%s", schema.c_str(), collection.c_str());
  }
  std::string column_type = virtual_supported ? "VIRTUAL" : "STORED";

  std::vector<std::pair<std::string, std::string> > columns;

  // NOTE: This could be done more efficiently with ALGORIHM=INPLACE but:
  // - currently server does not support adding virtual columns to the table inplace combined with
  //   other ALTER TABLE statement (adding index in this case)
  // - attempt to split adding the index and adding the virtual columns into 2 separate statements
  //   leads to the server crash ("Bug 21640846 ASSERTION FAILURE WHEN CREATING VIRTUAL COLUMN.")

  // generate DDL
  qb.put("ALTER TABLE ").quote_identifier(schema).dot().quote_identifier(collection);
  for (size_t c = col_field_path.size(), i = 0; i < c; i++)
  {
    std::string column_name;
    std::string type_name;
    int type_arg = -1, type_arg2 = -1;
    bool is_unsigned = false;
    // validate the type
    if (!col_raw_type[i].empty())
    {
      if (!parse_type(col_raw_type[i], type_name, type_arg, type_arg2, is_unsigned)
          || (valid_types.find(type_name) == valid_types.end())
          || (is_unsigned && !valid_types[type_name].unsigned_allowed))
        return ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Invalid or unsupported type specification '%s'", col_raw_type[i].c_str());
    }
    else
    {
      type_name = "TEXT";
      type_arg = 64;
    }

    std::string required = col_required[i] ? "NOT NULL" : "";

    column_name = '$' + get_type_prefix(valid_types[type_name].v_col_prefix, type_arg, type_arg2, is_unsigned, !required.empty())
                      + hash_column_name(col_field_path[i].substr(2));

    // if column with given name already exists just skip adding it and use it for the index
    if (!table_column_exists(schema, collection, column_name, m_da, column_exists))
      return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name: %s.%s", schema.c_str(), collection.c_str());

    std::stringstream column_index_size;
    if (type_arg > 0)
    {
      column_index_size << "(" << type_arg;
      if (type_arg2 > 0)
        column_index_size << ", " << type_arg2;
      column_index_size << ")";
    }

    if (!column_exists)
    {
      std::string extract_begin, extract_end;
      if (valid_types[type_name].unquote)
      {
        extract_begin = "JSON_UNQUOTE(";
        extract_end = ")";
      }

      qb.put(" ADD COLUMN ").quote_identifier(column_name).put(" ").put(type_name);

      if (type_name != "TEXT")
        qb.put(column_index_size.str());

      if (is_unsigned)
        qb.put(" UNSIGNED");

      qb.put(" GENERATED ALWAYS AS (").put(extract_begin).put("JSON_EXTRACT(doc, ")
                        .quote_string(col_field_path[i]).put(")").put(extract_end).put(") ")
                        .put(column_type).put(" ").put(required).put(",");
    }
    columns.push_back(std::make_pair(column_name,
                                     valid_types[type_name].is_binary ? column_index_size.str() : ""));
  }

  qb.put(unique ? " ADD UNIQUE INDEX " : " ADD INDEX ")
    .quote_identifier(index_name).put(" (");

  std::vector<std::pair<std::string, std::string> >::const_iterator it = columns.begin();
  for (; it != columns.end(); ++it)
  {
    if (it != columns.begin())
      qb.put(",");
    qb.quote_identifier(it->first).put(it->second);
  }
  qb.put(")");

  Sql_data_context::Result_info info;
  const ngs::PFS_string &tmp(qb.get());
  log_debug("CreateCollectionIndex: %s", tmp.c_str());
  error = m_da.execute_sql_no_result(tmp.data(), tmp.length(), info);
  if (error)
  {
    // if we're creating a NOT NULL generated index/column and get a NULL error, it's
    // because one of the existing documents had a NULL / unset value
    if (error.error == ER_BAD_NULL_ERROR && required)
      return ngs::Error_code(ER_X_DOC_REQUIRED_FIELD_MISSING, "Collection contains document missing required field");
    return error;
  }
  m_da.proto().send_exec_ok();
  return ngs::Success();
}


/* Stmt: drop_collection
 * Required arguments:
 * - name: string - name of dropped collection
 * - schema: string - name of collection's schema
 */
ngs::Error_code xpl::Admin_command_handler::drop_collection(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_drop_collection>();

  Query_string_builder qb;
  std::string schema;
  std::string collection;

  ngs::Error_code error = args.string_arg("schema", schema).string_arg("name", collection).end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  qb.put("DROP TABLE ").quote_identifier(schema).dot().quote_identifier(collection);

  const ngs::PFS_string &tmp(qb.get());
  log_debug("DropCollection: %s", tmp.c_str());
  Sql_data_context::Result_info info;
  error = m_da.execute_sql_no_result(tmp.data(), tmp.length(), info);
  if (error)
    return error;
  m_da.proto().send_exec_ok();

  return ngs::Success();
}


namespace
{

ngs::Error_code get_index_virtual_column_names(const std::string &schema_name, const std::string &table_name, const std::string &index_name,
                                               xpl::Sql_data_context &da, String_fields_values &ret_column_names)
{
  const unsigned INDEX_NAME_COLUMN_IDX = 4;
  xpl::Query_string_builder qb;

  /* get list of all index column names */
  qb.put("SHOW INDEX FROM ")
      .quote_identifier(schema_name).dot().quote_identifier(table_name)
      .put(" WHERE Key_name = ").quote_string(index_name);

  std::vector<unsigned> fields_ids(1);
  fields_ids[0] = INDEX_NAME_COLUMN_IDX;
  ngs::Error_code error = query_string_columns(da, qb.get(), fields_ids, ret_column_names);
  if (error)
    return error;

  /* remove from the list columns that shouldn't be dropped */

  /* don't drop non-virtual columns */
  error = remove_nonvirtual_column_names(schema_name, table_name, ret_column_names, da);
  if (error)
    return error;

  xpl::Buffering_command_delegate::Resultset r_rows;
  std::vector<xpl::Command_delegate::Field_type> r_types;
  xpl::Sql_data_context::Result_info r_info;
  String_fields_values::iterator it = ret_column_names.begin();
  while (it != ret_column_names.end())
  {
    /* don't drop '_id' column */
    if ((*it)[0] == "_id")
    {
      ret_column_names.erase(it++);
      continue;
    }

    /* don't drop columns used by other index(es) */
    qb.clear();
    qb.put("SHOW INDEX FROM ")
        .quote_identifier(schema_name).dot().quote_identifier(table_name)
        .put(" WHERE Key_name <> ").quote_string(index_name)
        .put(" AND Column_name = ").quote_string((*it)[0]);
    da.execute_sql_and_collect_results(qb.get().data(), qb.get().length(), r_types, r_rows, r_info);
    if (r_rows.size() > 0)
    {
      ret_column_names.erase(it++);
      continue;
    }
    ++it;
  }

  return ngs::Success();
}

} // namespace


/* Stmt: drop_collection_index
 * Required arguments:
 * - name: string - name of dropped index
 * - collection: string - name of collection with dropped index
 * - schema: string - name of collection's schema
 */
ngs::Error_code xpl::Admin_command_handler::drop_collection_index(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_drop_collection_index>();

  Query_string_builder qb;
  std::string schema;
  std::string collection;
  std::string name;

  ngs::Error_code error = args
      .string_arg("schema", schema)
      .string_arg("collection", collection)
      .string_arg("name", name).end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");
  if (name.empty())
    return ngs::Error_code(ER_X_MISSING_ARGUMENT, "Invalid index name");

  String_fields_values column_names;

  // collect the index columns (if any) to be dropped
  error = get_index_virtual_column_names(schema, collection, name, m_da, column_names);
  if (error)
  {
    if (error.error == ER_INTERNAL_ERROR)
      return error;
    else
      // if it is not internal then the reason is bad schema or table name
      return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name: %s.%s", schema.c_str(), collection.c_str());
  }

  // drop the index
  qb.put("ALTER TABLE ").quote_identifier(schema).dot().quote_identifier(collection)
                            .put(" DROP INDEX ").quote_identifier(name);

  // drop the index's virtual columns
  String_fields_values::const_iterator it = column_names.begin();
  for (; it != column_names.end(); ++it)
  {
    qb.put(", DROP COLUMN ").quote_identifier((*it)[0]);
  }

  const ngs::PFS_string &tmp(qb.get());
  log_debug("DropCollectionIndex: %s", tmp.c_str());
  Sql_data_context::Result_info info;
  error = m_da.execute_sql_no_result(tmp.data(), tmp.length(), info);
  if (error)
    return error;

  m_da.proto().send_exec_ok();
  return ngs::Success();
}


namespace
{

static const char* const fixed_notice_names[] = {
    "account_expired",
    "generated_insert_id",
    "rows_affected",
    "produced_message"
};
static const char* const *fixed_notice_names_end = &fixed_notice_names[0] + sizeof(fixed_notice_names) / sizeof(fixed_notice_names[0]);


inline bool is_fixed_notice_name(const std::string &notice)
{
  return std::find(fixed_notice_names, fixed_notice_names_end, notice) != fixed_notice_names_end;
}


inline void add_notice_row(xpl::Sql_data_context &da, const std::string &notice, longlong status)
{
  da.proto().start_row();
  da.proto().row_builder().add_string_field(notice.c_str(), notice.length(), NULL);
  da.proto().row_builder().add_longlong_field(status, 0);
  da.proto().send_row();
}

} // namespace


/* Stmt: enable_notices
 * Required arguments:
 * - notice: string, list - name (or names) of enabled notice
 */
ngs::Error_code xpl::Admin_command_handler::enable_notices(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_enable_notices>();

  std::vector<std::string> notices;
  ngs::Error_code error = args.string_list("notice", notices).end();
  if (error)
    return error;

  bool enable_warnings = false;
  for (std::vector<std::string>::const_iterator i = notices.begin(); i != notices.end(); ++i)
  {
    if (*i == "warnings")
      enable_warnings = true;
    else if (!is_fixed_notice_name(*i))
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s", i->c_str());
  }

  if (enable_warnings)
    m_options.set_send_warnings(true);

  m_da.proto().send_exec_ok();
  return ngs::Success();
}


/* Stmt: disable_notices
 * Required arguments:
 * - notice: string, list - name (or names) of enabled notice
 */
ngs::Error_code xpl::Admin_command_handler::disable_notices(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_disable_notices>();

  std::vector<std::string> notices;
  ngs::Error_code error = args.string_list("notice", notices).end();
  if (error)
    return error;

  bool disable_warnings = false;
  for (std::vector<std::string>::const_iterator i = notices.begin(); i != notices.end(); ++i)
  {
    if (*i == "warnings")
      disable_warnings = true;
    else if (is_fixed_notice_name(*i))
      return ngs::Error(ER_X_CANNOT_DISABLE_NOTICE, "Cannot disable notice %s", i->c_str());
    else
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s", i->c_str());
  }

  if (disable_warnings)
    m_options.set_send_warnings(false);

  m_da.proto().send_exec_ok();
  return ngs::Success();
}


/* Stmt: list_notices
 * No arguments required
 */
ngs::Error_code xpl::Admin_command_handler::list_notices(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_list_notices>();

  ngs::Error_code error = args.end();
  if (error)
    return error;

  // notice | enabled
  // <name> | <1/0>

  m_da.proto().send_column_metadata("", "", "", "", "notice", "", 0, Mysqlx::Resultset::ColumnMetaData::BYTES, 0, 0, 0);
  m_da.proto().send_column_metadata("", "", "", "", "enabled", "", 0, Mysqlx::Resultset::ColumnMetaData::SINT, 0, 0, 0);

  add_notice_row(m_da, "warnings", m_options.get_send_warnings() ? 1 : 0);
  for (const char* const *notice = fixed_notice_names; notice < fixed_notice_names_end; ++notice)
    add_notice_row(m_da, *notice, 1);

  m_da.proto().send_result_fetch_done();
  m_da.proto().send_exec_ok();
  return ngs::Success();
}


namespace
{
ngs::Error_code is_schema_selected_and_exists(xpl::Sql_data_context &da, const std::string &schema)
{
  xpl::Query_string_builder qb;
  qb.put("SHOW TABLES");
  if (!schema.empty())
    qb.put(" FROM ").quote_identifier(schema);

  xpl::Sql_data_context::Result_info info;
  return da.execute_sql_no_result(qb.get().data(), qb.get().length(), info);
}

template<typename T>
T get_system_variable(xpl::Sql_data_context &da, const std::string &variable)
{
  xpl::Sql_data_result result(da);
  try
  {
    result.query(("SELECT @@" + variable).c_str());
    if (result.size() != 1)
    {
      log_error("Unable to retrieve system variable '%s'", variable.c_str());
      return T();
    }
    T value = T();
    result.get(value);
    return value;
  }
  catch (const ngs::Error_code &)
  {
    log_error("Unable to retrieve system variable '%s'", variable.c_str());
    return T();
  }
}

const char *const COUNT_DOC =
    "COUNT(CASE WHEN (column_name = 'doc' "
    "AND data_type = 'json') THEN 1 ELSE NULL END)";
const char *const COUNT_ID =
    "COUNT(CASE WHEN (column_name = '_id' "
    "AND generation_expression = "
    "'json_unquote(json_extract(`doc`,''$._id''))') THEN 1 ELSE NULL END)";
const char *const COUNT_GEN =
    "COUNT(CASE WHEN (column_name != '_id' "
    "AND generation_expression RLIKE "
    "'^(json_unquote[[.(.]])?json_extract[[.(.]]`doc`,"
    "''[[.$.]]([[...]][^[:space:][...]]+)+''[[.).]]{1,2}$') THEN 1 ELSE NULL "
    "END)";
}  // namespace

/* Stmt: list_objects
 * Required arguments:
 * - schema: string, optional - name of listed object's schema
 * - pattern: string, optional - a filter to use for matching object names to be returned
 */
ngs::Error_code xpl::Admin_command_handler::list_objects(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_list_objects>();

  static const bool is_table_names_case_sensitive =
      get_system_variable<long>(m_da, "lower_case_table_names") == 0l;

  static const char *const BINARY_OPERATOR =
      is_table_names_case_sensitive &&
              get_system_variable<long>(m_da, "lower_case_file_system") == 0l
          ? "BINARY "
          : "";

  std::string schema, pattern;
  ngs::Error_code error = args
      .string_arg("schema", schema, true)
      .string_arg("pattern", pattern, true).end();
  if (error)
    return error;

  if (!is_table_names_case_sensitive) schema = to_lower(schema);

  error = is_schema_selected_and_exists(m_da, schema);
  if (error)
    return error;

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
  if (!pattern.empty())
    qb.put(" AND T.table_name LIKE ").quote_string(pattern);
  qb.put(" GROUP BY name ORDER BY name");

  Sql_data_context::Result_info info;
  error = m_da.execute_sql_and_stream_results(qb.get().data(),
                                              qb.get().length(), false, info);
  if (error)
    return error;

  m_da.proto().send_exec_ok();
  return ngs::Success();
}


namespace
{
bool is_collection(xpl::Sql_data_context &da, const std::string &schema, const std::string &name)
{
  xpl::Query_string_builder qb;
  qb.put("SELECT COUNT(*) AS cnt,")
    .put(COUNT_DOC).put(" AS doc,").put(COUNT_ID).put(" AS id,").put(COUNT_GEN).put(" AS gen "
      "FROM information_schema.columns "
      "WHERE table_name = ").quote_string(name).put(" AND table_schema = ");
  if (schema.empty())
    qb.put("schema()");
  else
    qb.quote_string(schema);

  xpl::Sql_data_result result(da);
  try
  {
    result.query(qb.get());
    if (result.size() != 1)
    {
      log_debug("Unable to recognize '%s' as a collection; query result size: %lu",
                std::string(schema.empty() ? name : schema + "." + name).c_str(),
                static_cast<unsigned long>(result.size()));
      return false;
    }
    long int cnt = 0, doc = 0, id = 0, gen = 0;
    result.get(cnt).get(doc).get(id).get(gen);
    return doc == 1 && id == 1 && (cnt == gen + doc + id);
  }
#if defined(XPLUGIN_LOG_DEBUG) && !defined(XPLUGIN_DISABLE_LOG)
  catch (const ngs::Error_code &e)
#else
  catch (const ngs::Error_code &)
#endif
  {
    log_debug("Unable to recognize '%s' as a collection; exception message: '%s'",
              std::string(schema.empty() ? name : schema + "." + name).c_str(), e.message.c_str());
    return false;
  }
}

} // namespace


/* Stmt: ensure_collection
 * Required arguments:
 * - name: string - name of created collection
 * - schema: string, optional - name of collection's schema
 */
ngs::Error_code xpl::Admin_command_handler::ensure_collection(Command_arguments &args)
{
  m_session.update_status<&Common_status_variables::m_stmt_ensure_collection>();
  std::string schema;
  std::string collection;

  ngs::Error_code error = args.string_arg("schema", schema, true).string_arg("name", collection).end();
  if (error)
    return error;

  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  error = create_collection_impl(m_da, schema, collection);
  if (error)
  {
    if (error.error != ER_TABLE_EXISTS_ERROR)
      return error;
    if (!is_collection(m_da, schema, collection))
      return ngs::Error(ER_X_INVALID_COLLECTION,
                        "Table '%s' exists but is not a collection",
                        (schema.empty() ? collection : schema + '.' + collection).c_str());
  }
  m_da.proto().send_exec_ok();
  return ngs::Success();
}


const char* const xpl::Admin_command_handler::Command_arguments::PLACEHOLDER = "?";


xpl::Admin_command_arguments_list::Admin_command_arguments_list(const List &args)
: m_args(args), m_current(m_args.begin()), m_args_consumed(0)
{}


xpl::Admin_command_arguments_list &xpl::Admin_command_arguments_list::string_arg(const char *name, std::string &ret_value, bool optional)
{
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_STRING, "string", optional))
  {
    const std::string &value = m_current->scalar().v_string().value();
    if (memchr(value.data(), 0, value.length()))
    {
      m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Invalid value for argument '%s'", name);
      return *this;
    }
    ret_value = value;
    ++m_current;
  }
  return *this;
}


xpl::Admin_command_arguments_list &xpl::Admin_command_arguments_list::string_list(const char *name, std::vector<std::string> &ret_value, bool optional)
{
  std::string value;
  do
  {
    string_arg(name, value, optional);
    ret_value.push_back(value);
    value.clear();
  }
  while (!is_end());
  return *this;
}


xpl::Admin_command_arguments_list &xpl::Admin_command_arguments_list::sint_arg(const char *name, int64_t &ret_value, bool optional)
{
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_SINT, "signed int", optional))
  {
    if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_UINT)
      ret_value = (int64_t)m_current->scalar().v_unsigned_int();
    else if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_SINT)
      ret_value = m_current->scalar().v_signed_int();
    ++m_current;
  }
  return *this;
}


xpl::Admin_command_arguments_list &xpl::Admin_command_arguments_list::uint_arg(const char *name, uint64_t &ret_value, bool optional)
{
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_UINT, "unsigned int", optional))
  {
    if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_UINT)
      ret_value = m_current->scalar().v_unsigned_int();
    else if (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_SINT)
      ret_value = (uint64_t)m_current->scalar().v_signed_int();
    ++m_current;
  }
  return *this;
}


xpl::Admin_command_arguments_list &xpl::Admin_command_arguments_list::bool_arg(const char *name, bool &ret_value, bool optional)
{
  if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_BOOL, "bool", optional))
  {
    ret_value = m_current->scalar().v_bool();
    ++m_current;
  }
  return *this;
}


xpl::Admin_command_arguments_list &xpl::Admin_command_arguments_list::docpath_arg(const char *name, std::string &ret_value, bool optional)
{
  m_args_consumed++;
  if (!m_error)
  {
    if (m_current == m_args.end())
      m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS, "Too few arguments");
    else
    {
      if (m_current->type() == Mysqlx::Datatypes::Any::SCALAR && m_current->has_scalar() &&
          (m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_STRING && m_current->scalar().has_v_string()))
      {
        ret_value = m_current->scalar().v_string().value();
        // We could perform some extra validation on the document path here, but
        // since the path will be quoted and escaped when used, it would be redundant.
        // Plus, the best way to have the exact same syntax as the server
        // is to let the server do it.
        if (ret_value.empty() || ret_value.size() < 2)
          m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Invalid document path value for argument %s", name);
      }
      else
        arg_type_mismatch(name, m_args_consumed, "document path string");
    }
    ++m_current;
  }
  return *this;
}


xpl::Admin_command_arguments_list &xpl::Admin_command_arguments_list::object_list(const char *name, std::vector<Command_arguments*> &ret_value,
                                                                                  bool optional, unsigned expected_members_count)
{
  List::difference_type left = m_args.end() - m_current;
  if (left % expected_members_count > 0)
  {
    m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS, "Too few values for argument '%s'", name);
    return *this;
  }
  for (unsigned i = 0; i < left / expected_members_count; ++i)
    ret_value.push_back(this);
  return *this;
}


bool xpl::Admin_command_arguments_list::is_end() const
{
  return !(m_error.error == 0 && m_args.size() > m_args_consumed);
}


const ngs::Error_code &xpl::Admin_command_arguments_list::end()
{
  if (m_error.error == ER_X_CMD_NUM_ARGUMENTS || (m_error.error == 0 && m_args.size() > m_args_consumed))
  {
    m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                         "Invalid number of arguments, expected %i but got %i", m_args_consumed, m_args.size());
  }
  return m_error;
}


void xpl::Admin_command_arguments_list::arg_type_mismatch(const char *argname, int argpos, const char *type)
{
  m_error = ngs::Error(ER_X_CMD_ARGUMENT_TYPE, "Invalid type for argument '%s' at #%i (should be %s)", argname, argpos, type);
}


bool xpl::Admin_command_arguments_list::check_scalar_arg(const char *argname, Mysqlx::Datatypes::Scalar::Type type, const char *type_name, bool optional)
{
  m_args_consumed++;
  if (!m_error)
  {
    if (m_current == m_args.end())
    {
      if (!optional)
        m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS, "Insufficient number of arguments");
    }
    else
    {
      if (m_current->type() == Mysqlx::Datatypes::Any::SCALAR && m_current->has_scalar())
      {
        if (m_current->scalar().type() == type)
        {
          //TODO: add charset check for strings?
          // return true only if value to be consumed is available
          return true;
        }
        else if (type == Mysqlx::Datatypes::Scalar::V_SINT &&
            m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_UINT &&
            m_current->scalar().v_unsigned_int() < (uint64_t)std::numeric_limits<int64_t>::max())
        {
          return true;
        }
        else if (type == Mysqlx::Datatypes::Scalar::V_UINT &&
            m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_SINT
            && m_current->scalar().v_signed_int() >= 0)
        {
          return true;
        }
        else if (optional && m_current->scalar().type() == Mysqlx::Datatypes::Scalar::V_NULL)
          ;
        else
          arg_type_mismatch(argname, m_args_consumed, type_name);
      }
      else
        arg_type_mismatch(argname, m_args_consumed, type_name);
      ++m_current;
    }
  }
  return false;
}



namespace
{
typedef ::Mysqlx::Datatypes::Object_ObjectField Object_field;


struct Object_field_key_is_equal
{
  Object_field_key_is_equal(const char *p) : m_pattern(p) {}
  bool operator() (const Object_field &fld) const
  {
    return fld.has_key() && fld.key() == m_pattern;
  }
private:
  const char *m_pattern;
};


template<typename T>
class General_argument_validator
{
public:
  General_argument_validator(const char*, ngs::Error_code&) {}
  void operator() (const T &input, T &output) { output = input; }
};


template<typename T, typename V = General_argument_validator<T> >
class Argument_type_handler
{
public:
  Argument_type_handler(const char *name, T &value, ngs::Error_code &error)
  : m_validator(name, error), m_value(&value), m_error(error), m_name(name)
  {}

  Argument_type_handler(const char *name, ngs::Error_code &error)
  : m_validator(name, error), m_value(NULL), m_error(error), m_name(name)
  {}

  void assign(T &value) { m_value = &value; }
  void operator() (const T &value) { m_validator(value, *m_value); }
  void operator() () { m_error = ngs::Error(ER_X_CMD_ARGUMENT_TYPE,
                                            "Invalid type of value for argument '%s'", m_name); }
  template<typename O> void operator()(const O &value) { this->operator()(); }

private:
  V m_validator;
  T *m_value;
  ngs::Error_code &m_error;
  const char *m_name;
};


class String_argument_validator
{
public:
  String_argument_validator(const char *name, ngs::Error_code &error)
  : m_name(name), m_error(error)
  {}

  void operator() (const std::string &input, std::string &output)
  {
    if (memchr(input.data(), 0, input.length()))
    {
      m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Invalid value for argument '%s'", m_name);
      return;
    }
    output = input;
  }

protected:
  const char *m_name;
  ngs::Error_code &m_error;
};


class Docpath_argument_validator : String_argument_validator
{
public:
  Docpath_argument_validator(const char *name, ngs::Error_code &error)
  : String_argument_validator(name, error)
  {}

  void operator() (const std::string &input, std::string &output)
  {
    static const xpl::Regex re("^[[.dollar-sign.]]([[.period.]][^[:space:][.period.]]+)+$");
    std::string value;
    String_argument_validator::operator ()(input, value);
    if (m_error)
      return;
    if (re.match(value.c_str()))
      output = value;
    else
      m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                           "Invalid value for argument '%s', expected path to document member", m_name);
  }
};

} // namespace


xpl::Admin_command_arguments_object::Admin_command_arguments_object(const List &args)
: m_args_empty(args.size() == 0),
  m_is_object(args.size() == 1 && args.Get(0).has_obj()),
  m_object(m_is_object ? args.Get(0).obj() : Object::default_instance()),
  m_args_consumed(0)
{}


xpl::Admin_command_arguments_object::Admin_command_arguments_object(const Object &obj)
: m_args_empty(true),
  m_is_object(true),
  m_object(obj),
  m_args_consumed(0)
{}


void xpl::Admin_command_arguments_object::expected_value_error(const char *name)
{
  m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                       "Invalid number of arguments, expected value for '%s'", name);
}


template<typename H>
void xpl::Admin_command_arguments_object::get_scalar_arg(const char *name, bool optional, H &handler)
{
  const Object::ObjectField *field = get_object_field(name, optional);
  if (!field)
    return;

  get_scalar_value(field->value(), handler);
}


const xpl::Admin_command_arguments_object::Object::ObjectField *xpl::Admin_command_arguments_object::get_object_field(const char *name, bool optional)
{
  if (m_error)
    return NULL;

  ++m_args_consumed;

  if (!m_is_object)
  {
    if (!optional)
      expected_value_error(name);
    return NULL;
  }

  const Object_field_list &fld = m_object.fld();
  Object_field_list::const_iterator i = std::find_if(fld.begin(), fld.end(), Object_field_key_is_equal(name));
  if (i == fld.end())
  {
    if (!optional)
      expected_value_error(name);
    return NULL;
  }

  return &(*i);
}


template<typename H>
void xpl::Admin_command_arguments_object::get_scalar_value(const Any &value, H &handler)
{
  try
  {
    ngs::Getter_any::put_scalar_value_to_functor(value, handler);
  }
  catch (const ngs::Error_code &e)
  {
    m_error = e;
  }
}


xpl::Admin_command_arguments_object &xpl::Admin_command_arguments_object::string_arg(const char *name, std::string &ret_value, bool optional)
{
  Argument_type_handler<std::string, String_argument_validator> handler(name, ret_value, m_error);
  get_scalar_arg(name, optional, handler);
  return *this;
}


xpl::Admin_command_arguments_object &xpl::Admin_command_arguments_object::string_list(const char *name, std::vector<std::string> &ret_value, bool optional)
{
  const Object::ObjectField *field = get_object_field(name, optional);
  if (!field)
    return *this;

  if (!field->value().has_type())
  {
    expected_value_error(name);
    return *this;
  }

  std::vector<std::string> values;
  Argument_type_handler<std::string, String_argument_validator> handler(name, m_error);

  switch (field->value().type())
  {
  case ::Mysqlx::Datatypes::Any_Type_ARRAY:
    for (int i = 0; i < field->value().array().value_size(); ++i)
    {
      handler.assign(*values.insert(values.end(), ""));
      get_scalar_value(field->value().array().value(i), handler);
    }
    break;

  case ::Mysqlx::Datatypes::Any_Type_SCALAR:
    handler.assign(*values.insert(values.end(), ""));
    get_scalar_value(field->value(), handler);
    break;

  default:
    m_error = ngs::Error(ER_X_CMD_ARGUMENT_TYPE,
                         "Invalid type of argument '%s', expected list of arguments", name);
  }

  if (!m_error)
    ret_value = values;

  return *this;
}


xpl::Admin_command_arguments_object &xpl::Admin_command_arguments_object::sint_arg(const char *name, int64_t &ret_value, bool optional)
{
  Argument_type_handler<google::protobuf::int64> handler(name, ret_value, m_error);
  get_scalar_arg(name, optional, handler);
  return *this;
}


xpl::Admin_command_arguments_object &xpl::Admin_command_arguments_object::uint_arg(const char *name, uint64_t &ret_value, bool optional)
{
  Argument_type_handler<google::protobuf::uint64> handler(name, ret_value, m_error);
  get_scalar_arg(name, optional, handler);
  return *this;
}


xpl::Admin_command_arguments_object &xpl::Admin_command_arguments_object::bool_arg(const char *name, bool &ret_value, bool optional)
{
  Argument_type_handler<bool> handler(name, ret_value, m_error);
  get_scalar_arg(name, optional, handler);
  return *this;
}


xpl::Admin_command_arguments_object &xpl::Admin_command_arguments_object::docpath_arg(const char *name, std::string &ret_value, bool optional)
{
  Argument_type_handler<std::string, Docpath_argument_validator> handler(name, ret_value, m_error);
  get_scalar_arg(name, optional, handler);
  return *this;
}


xpl::Admin_command_arguments_object &xpl::Admin_command_arguments_object::object_list(const char *name, std::vector<Command_arguments*> &ret_value,
                                                                                      bool optional, unsigned)
{
  const Object::ObjectField *field = get_object_field(name, optional);
  if (!field)
    return *this;

  if (!field->value().has_type())
  {
    expected_value_error(name);
    return *this;
  }

  std::vector<Command_arguments*> values;
  switch (field->value().type())
  {
  case ::Mysqlx::Datatypes::Any_Type_ARRAY:
    for (int i = 0; i < field->value().array().value_size(); ++i)
    {
      const Any &any = field->value().array().value(i);
      if (!any.has_type() || any.type() != ::Mysqlx::Datatypes::Any_Type_OBJECT)
      {
        m_error = ngs::Error(ER_X_CMD_ARGUMENT_TYPE,
                             "Invalid type of argument '%s', expected list of objects", name);
        break;
      }
      values.push_back(add_sub_object(any.obj()));
    }
    break;

  case ::Mysqlx::Datatypes::Any_Type_OBJECT:
    values.push_back(add_sub_object(field->value().obj()));
    break;

  default:
    m_error = ngs::Error(ER_X_CMD_ARGUMENT_TYPE,
                         "Invalid type of argument '%s', expected list of objects", name);
  }

  if (!m_error)
    ret_value = values;

  return *this;
}


xpl::Admin_command_arguments_object *xpl::Admin_command_arguments_object::add_sub_object(const Object &object)
{
  Admin_command_arguments_object *obj = new Admin_command_arguments_object(object);
  m_sub_objects.push_back(ngs::shared_ptr<Admin_command_arguments_object>(obj));
  return obj;
}




const ngs::Error_code &xpl::Admin_command_arguments_object::end()
{
  if (m_error)
    return m_error;

  if (m_is_object)
  {
    if (m_object.fld().size() > m_args_consumed)
      m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS,
                           "Invalid number of arguments, expected %i but got %i",
                           m_args_consumed, m_object.fld().size());
  }
  else
  {
    if (!m_args_empty)
      m_error = ngs::Error(ER_X_CMD_ARGUMENT_TYPE,
                           "Invalid type of arguments, expected object of arguments");
  }
  return m_error;
}


bool xpl::Admin_command_arguments_object::is_end() const
{
  return !(m_error.error == 0 && m_is_object && m_object.fld().size() > m_args_consumed);
}

