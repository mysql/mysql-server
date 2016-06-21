/*
* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "ngs_common/protocol_protobuf.h"

#include "expr_generator.h"
#include "json_utils.h"

#include "xpl_session.h"
#include "xpl_log.h"
#include "xpl_server.h"
#include <ctype.h>
#include <sstream>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <set>

using namespace xpl;


static ngs::Error_code arg_type_mismatch(const char *argname, int argpos, const char *type)
{
  return ngs::Error(ER_X_CMD_ARGUMENT_TYPE, "Invalid type for argument '%s' at #%i (should be %s)", argname, argpos, type);
}


class Argument_extractor
{
public:
  Argument_extractor(const Admin_command_handler::Argument_list &args)
    : m_args(args), m_current(m_args.begin()), m_args_consumed(0)
  {
  }

  Argument_extractor &string_arg(const char *name, std::string &ret_value, bool optional = false)
  {
    if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_STRING, "string", optional))
    {
      ret_value = m_current->scalar().v_string().value();
      ++m_current;
    }
    return *this;
  }

  Argument_extractor &sint_arg(const char *name, int64_t &ret_value, bool optional = false)
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

  Argument_extractor &uint_arg(const char *name, uint64_t &ret_value, bool optional = false)
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

  Argument_extractor &bool_arg(const char *name, bool &ret_value, bool optional = false)
  {
    if (check_scalar_arg(name, Mysqlx::Datatypes::Scalar::V_BOOL, "bool", optional))
    {
      ret_value = m_current->scalar().v_bool();
      ++m_current;
    }
    return *this;
  }

  Argument_extractor &docpath_arg_non_validated(const char *name, std::string &ret_value, bool optional = false)
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
          try
          {
            ret_value = m_current->scalar().v_string().value();
            // We could perform some extra validation on the document path here, but
            // since the path will be quoted and escaped when used, it would be redundant.
            // Plus, the best way to have the exact same syntax as the server
            // is to let the server do it.
            if (ret_value.empty() || ret_value.size() < 2)
              m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Invalid document path value for argument %s", name);
          }
          catch (Expression_generator::Error&)
          {
            m_error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Invalid document path value for argument %s", name);
          }
        }
        else
          m_error = arg_type_mismatch(name, m_args_consumed, "document path string");
      }
      ++m_current;
    }
    return *this;
  }

  bool is_end() const
  {
    if (m_error.error == 0 && m_args.size() > m_args_consumed)
      return false;
    return true;
  }

  ngs::Error_code error()
  {
    return m_error;
  }

  ngs::Error_code end()
  {
    if (m_error.error == ER_X_CMD_NUM_ARGUMENTS || (m_error.error == 0 && m_args.size() > m_args_consumed))
    {
      m_error = ngs::Error(ER_X_CMD_NUM_ARGUMENTS, "Invalid number of arguments, expected %i but got %i", m_args_consumed, m_args.size());
    }
    return m_error;
  }

private:
  bool check_scalar_arg(const char *argname, Mysqlx::Datatypes::Scalar::Type type, const char *type_name, bool optional)
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
            m_error = arg_type_mismatch(argname, m_args_consumed, type_name);
        }
        else
          m_error = arg_type_mismatch(argname, m_args_consumed, type_name);
        ++m_current;
      }
    }
    return false;
  }

  const Admin_command_handler::Argument_list &m_args;
  Admin_command_handler::Argument_list::const_iterator m_current;
  ngs::Error_code m_error;
  int m_args_consumed;
};

struct Index_field_traits {
  bool is_binary;
  bool unsigned_allowed;
  bool unquote;
  bool prefix_len_allowed;
  std::string v_col_prefix;

  Index_field_traits(bool b, bool ua, bool u, bool pa, const std::string& pref) :
    is_binary(b), unsigned_allowed(ua), unquote(u), prefix_len_allowed(pa),
    v_col_prefix(pref)
  {}

  Index_field_traits() : is_binary(false), unsigned_allowed(false), unquote(false),
    v_col_prefix("")
  {}
};


static const std::string fixed_notice_names[] = {
  "account_expired",
  "generated_insert_id",
  "rows_affected",
  "produced_message"
};

Admin_command_handler::Command_handler_map Admin_command_handler::m_command_handlers;
Admin_command_handler::Command_handler_map_init Admin_command_handler::m_command_handler_init;

Admin_command_handler::Command_handler_map_init::Command_handler_map_init()
{
  m_command_handlers["ping"] = &Admin_command_handler::ping;

  m_command_handlers["list_clients"] = &Admin_command_handler::list_clients;
  m_command_handlers["kill_client"] = &Admin_command_handler::kill_client;

  m_command_handlers["create_collection"] = &Admin_command_handler::create_collection;
  m_command_handlers["create_collection_index"] = &Admin_command_handler::create_collection_index;
  m_command_handlers["drop_collection"] = &Admin_command_handler::drop_collection_or_table;
  m_command_handlers["drop_collection_index"] = &Admin_command_handler::drop_collection_index;

  m_command_handlers["list_objects"] = &Admin_command_handler::list_objects;

  m_command_handlers["enable_notices"] = &Admin_command_handler::enable_notices;
  m_command_handlers["disable_notices"] = &Admin_command_handler::disable_notices;
  m_command_handlers["list_notices"] = &Admin_command_handler::list_notices;
}


ngs::Error_code Admin_command_handler::execute(Session &session,
  Sql_data_context &da, Session_options &options,
  const std::string &command, const Argument_list &args)
{
  ngs::Error_code error;
  std::string command_lower = command;

  if (da.password_expired())
    return ngs::Error(ER_MUST_CHANGE_PASSWORD, "You must reset your password using ALTER USER statement before executing this statement.");

  std::transform(command_lower.begin(), command_lower.end(), command_lower.begin(), ::tolower);

  Command_handler_map::const_iterator iter = m_command_handlers.find(command_lower);
  if (iter != m_command_handlers.end())
  {
    try
    {
      error = (*iter->second)(session, da, options, args);
    }
    catch (std::exception &exc)
    {
      log_error("Error executing admin command %s: %s", command.c_str(), exc.what());
      error = ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
    }
  }
  else
    error = ngs::Error(ER_X_INVALID_ADMIN_COMMAND, "Invalid xplugin command %s", command.c_str());
  return error;
}


ngs::Error_code Admin_command_handler::ping(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_ping>(session.get_status_variables());

  da.proto().send_exec_ok();
  return ngs::Success();
}

struct Client_data_ {
  uint64_t id;
  std::string user;
  std::string host;
  uint64_t session;
  bool has_session;
  // init with "NULLs"
  Client_data_() : id(0), session(0), has_session(false) {}
};


void get_client_data(std::vector<Client_data_> &clients_data, Session &requesting_session, Sql_data_context &da, ngs::Client_ptr &client)
{
  boost::shared_ptr<xpl::Session> session(boost::static_pointer_cast<xpl::Session>(client->session()));
  Client_data_ c;

  if (session)
  {
    const char *user = session->is_ready() ? session->data_context().authenticated_user() : NULL;
    if (requesting_session.can_see_user(user))
    {
      c.id = static_cast<long>(client->client_id_num());
      c.host = client->client_hostname();
      if (user)
      {
        c.user = std::string(user);
        c.session = session->data_context().mysql_session_id();
        c.has_session = true;
      }

      clients_data.push_back(c);
    }
  }
  else if (da.authenticated_user_is_super())
  {
    c.id = static_cast<long>(client->client_id_num());
    c.host = client->client_hostname();

    clients_data.push_back(c);
  }
}

ngs::Error_code Admin_command_handler::list_clients(Session &requesting_session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_list_clients>(requesting_session.get_status_variables());

  ngs::Error_code error = Argument_extractor(args).end();
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
                     boost::bind(get_client_data,
                                 boost::ref(clients),
                                 boost::ref(requesting_session),
                                 boost::ref(da),
                                 _1));
    }
  }

  ngs::Protocol_encoder &proto(da.proto());

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


ngs::Error_code Admin_command_handler::kill_client(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_kill_client>(session.get_status_variables());

  uint64_t cid = 0;

  ngs::Error_code error = Argument_extractor(args)
    .uint_arg("client_id", cid).end();
  if (error)
    return error;

  {
    xpl::Server::Server_ref server(Server::get_instance());
    if (server)
      error = (*server)->kill_client(cid, session);
  }
  if (error)
    return error;

  da.proto().send_exec_ok();

  return ngs::Success();
}

/* CreateCollection

Required arguments:
- schema
- name
*/
ngs::Error_code Admin_command_handler::create_collection(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_create_collection>(session.get_status_variables());

  std::string schema;
  std::string name;

  ngs::Error_code error = Argument_extractor(args)
    .string_arg("schema", schema)
    .string_arg("name", name).end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (name.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  // ensure there are no invalid characters in the name
  if (memchr(name.data(), 0, name.length()))
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  Query_string_builder qb;

  qb.put("CREATE TABLE ").quote_identifier(schema).dot().quote_identifier(name).put(" (");
  qb.put("doc JSON,");
  // XXX once merged, change it so column is created as VIRTUAL, so that InnoDB won't automatically turn this into a PK
  qb.put("_id VARCHAR(32) GENERATED ALWAYS AS (JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED NOT NULL UNIQUE");
  qb.put(") CHARSET utf8mb4 ENGINE=InnoDB;");

  Sql_data_context::Result_info info;
  const std::string &tmp(qb.get());
  log_debug("CreateCollection: %s", tmp.c_str());
  error = da.execute_sql_no_result(tmp, info);
  if (error)
    return error;
  da.proto().send_exec_ok();
  return ngs::Success();
}

/*
* valid input examples:
* DECIMAL
* DECIMAL UNSIGNED
* DECIMAL(10)
* DECIMAL(10) UNSIGNED
* DECIMAL(10,5)
* DECIMAL(10,5) UNSIGNED
*/
static bool parse_type(const std::string &s, std::string &r_type, int &r_arg, int &r_arg2, bool &r_uns)
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

static std::string get_type_prefix(const std::string& prefix, int type_arg, int type_arg2, bool is_unsigned, bool required)
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

static ngs::Error_code query_string_columns(
  Sql_data_context &da,
  const std::string &sql,
  std::vector<unsigned> &field_idxs,
  String_fields_values &ret_values)
{
  Buffering_command_delegate::Resultset r_rows;
  std::vector<Command_delegate::Field_type> r_types;
  Sql_data_context::Result_info r_info;

  ngs::Error_code err = da.execute_sql_and_collect_results(sql, r_types, r_rows, r_info);
  if (err)
    return err;

  ret_values.clear();
  size_t fields_number = field_idxs.size();
  Buffering_command_delegate::Resultset::iterator it = r_rows.begin();
  for (; it != r_rows.end(); ++it)
  {
    ret_values.push_back(std::vector<std::string>(fields_number));
    for (size_t i = 0; i < field_idxs.size(); ++i)
    {
      unsigned field_idx = field_idxs[i];

      Buffering_command_delegate::Row_data* row_data = &(*it);
      if ((!row_data) || (row_data->fields.size() <= field_idx))
      {
        log_error("query_string_columns failed: invalid row data");
        return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
      }

      Buffering_command_delegate::Field_value *field = row_data->fields[field_idx];
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


static bool name_is(const std::vector<std::string>& field,
  const std::string& name)
{
  return field[0] == name;
}

static ngs::Error_code remove_nonvirtual_column_names(
  const std::string &schema_name,
  const std::string &table_name,
  String_fields_values &ret_column_names,
  Sql_data_context &da
  )
{
  Query_string_builder qb;
  const unsigned FIELD_COLMN_IDX = 0;
  const unsigned EXTRA_COLMN_IDX = 5;

  if (ret_column_names.size() == 0)
    return ngs::Success();

  qb.put("SHOW COLUMNS FROM ").quote_identifier(schema_name).dot().quote_identifier(table_name)
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
      ret_column_names.remove_if(boost::bind(name_is, _1, column_name));
  }

  return ngs::Success();
}

static ngs::Error_code index_on_virtual_column_supported(
  const std::string &schema_name,
  const std::string &table_name,
  Sql_data_context &da,
  bool &r_supports
  )
{
  const unsigned CREATE_COLMN_IDX = 1;
  Query_string_builder qb;
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

static bool table_column_exists(
  const std::string &schema_name,
  const std::string &table_name,
  const std::string &column_name,
  Sql_data_context &da,
  bool &r_exists)
{
  Query_string_builder qb;
  Buffering_command_delegate::Resultset r_rows;
  std::vector<Command_delegate::Field_type> r_types;
  Sql_data_context::Result_info r_info;

  qb.put("SHOW COLUMNS FROM ").quote_identifier(schema_name).dot().quote_identifier(table_name)
    .put(" WHERE Field = ").quote_string(column_name);

  ngs::Error_code err =
    da.execute_sql_and_collect_results(qb.get(), r_types, r_rows, r_info);
  if (err)
    return false;

  r_exists = r_rows.size() > 0;
  return true;
}


#include "sha1.h"
#include "password.h"

static std::string hash_column_name(const std::string& name)
{
  std::string hash;
  hash.resize(2*SHA1_HASH_SIZE + 2);
  // just an arbitrary hash
  ::make_scrambled_password(&hash[0], name.c_str());
  hash.resize(2*SHA1_HASH_SIZE + 1); // strip the \0
  return hash.substr(1); // skip the 1st char
}

/* CreateCollectionIndex

Required arguments:
- schema
- collection
- index_name
- unique: bool
Repeated:
- required: bool
- document_path
- type: (text(length), int, float, datetime, time, date)

VARCHAR and CHAR are now indexable because:
- varchar column needs to be created with a length, which would limit documents to have
 that field smaller than that
- if we use left() to truncate the value of the column, then the index won't be usable unless
 queries also specify left(), which is not desired.
*/
ngs::Error_code Admin_command_handler::create_collection_index(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_create_collection_index>(session.get_status_variables());

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
  Query_string_builder qb;
  ngs::Error_code error;
  std::vector<std::string> col_field_path;
  std::vector<std::string> col_raw_type;
  std::vector<bool> col_required;
  bool required = false;
  bool column_exists = false;

  Argument_extractor argx(args);
  argx.string_arg("schema", schema)
    .string_arg("collection", collection)
    .string_arg("index_name", index_name)
    .bool_arg("unique", unique);
  error = argx.error();
  if (error)
    return error;

  do
  {
    std::string f, t;
    bool r = false;
    argx.docpath_arg_non_validated("document_path", f)
      .string_arg("type", t)
      .bool_arg("required", r);
    error = argx.error();
    if (error)
      return error;
    if (f.empty())
      return ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Argument value '%s' for document_path is invalid", f.c_str());
    col_field_path.push_back(f);
    col_raw_type.push_back(t);
    col_required.push_back(r);
    required = required || r;
  } while (!argx.is_end());

  error = argx.end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error(ER_X_BAD_SCHEMA, "Invalid schema '%s'", schema.c_str());
  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name '%s'", collection.c_str());
  if (index_name.empty())
    return ngs::Error(ER_X_CMD_ARGUMENT_VALUE, "Argument value '%s' for index_name is invalid", index_name.c_str());

  // check if the table's engine supports index on the virtual column
  bool virtual_supported = false;
  error = index_on_virtual_column_supported(schema, collection, da, virtual_supported);
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
    if (!table_column_exists(schema, collection, column_name, da, column_exists))
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

  qb.put(unique ? " ADD UNIQUE INDEX " : " ADD INDEX ").quote_identifier(index_name).
    put(" (");

  std::vector<std::pair<std::string, std::string> >::const_iterator it = columns.begin();
  for (; it != columns.end(); ++it)
  {
    if (it != columns.begin())
      qb.put(",");
    qb.quote_identifier(it->first).put(it->second);
  }
  qb.put(")");

  Sql_data_context::Result_info info;
  const std::string &tmp(qb.get());
  log_debug("CreateCollectionIndex: %s", tmp.c_str());
  error = da.execute_sql_no_result(tmp, info);
  if (error)
  {
    // if we're creating a NOT NULL generated index/column and get a NULL error, it's
    // because one of the existing documents had a NULL / unset value
    if (error.error == ER_BAD_NULL_ERROR && required)
      return ngs::Error_code(ER_X_DOC_REQUIRED_FIELD_MISSING, "Collection contains document missing required field");
    return error;
  }
  da.proto().send_exec_ok();
  return ngs::Success();
}


/** DropCollection

Required arguments:
- schema
- table_or_collection
*/
ngs::Error_code Admin_command_handler::drop_collection_or_table(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_drop_collection>(session.get_status_variables());

  std::string schema;
  std::string collection;

  ngs::Error_code error = Argument_extractor(args)
    .string_arg("schema", schema)
    .string_arg("table_or_collection", collection)
    .end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");

  Query_string_builder qb;

  qb.put("DROP TABLE ").quote_identifier(schema).dot().quote_identifier(collection);

  const std::string &tmp(qb.get());
  log_debug("DropCollection: %s", tmp.c_str());
  Sql_data_context::Result_info info;
  error = da.execute_sql_no_result(tmp, info);
  if (error)
    return error;
  da.proto().send_exec_ok();
  return ngs::Success();
}

static ngs::Error_code get_index_virtual_column_names(
  const std::string &schema_name,
  const std::string &table_name,
  const std::string &index_name,
  Sql_data_context &da,
  String_fields_values &ret_column_names)
{
  const unsigned INDEX_NAME_COLUMN_IDX = 4;
  Query_string_builder qb;

  /* get list of all index column names */
  qb.put("SHOW INDEX FROM ").quote_identifier(schema_name).dot().quote_identifier(table_name)
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

  Buffering_command_delegate::Resultset r_rows;
  std::vector<Command_delegate::Field_type> r_types;
  Sql_data_context::Result_info r_info;
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
    qb.put("SHOW INDEX FROM ").quote_identifier(schema_name).dot().quote_identifier(table_name)
      .put(" WHERE Key_name <> ").quote_string(index_name)
      .put(" AND Column_name = ").quote_string((*it)[0]);
    da.execute_sql_and_collect_results(qb.get(), r_types, r_rows, r_info);
    if (r_rows.size() > 0)
    {
      ret_column_names.erase(it++);
      continue;
    }
    ++it;
  }

  return ngs::Success();
}

/** DropCollectionIndex

Required arguments:
- schema
- table_or_collection
- index_name

*/
ngs::Error_code Admin_command_handler::drop_collection_index(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_drop_collection_index>
    (session.get_status_variables());

  std::string schema;
  std::string table;
  std::string name;

  ngs::Error_code error = Argument_extractor(args)
    .string_arg("schema", schema)
    .string_arg("table_or_collection", table)
    .string_arg("index_name", name)
    .end();
  if (error)
    return error;

  if (schema.empty())
    return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (table.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");
  if (name.empty())
    return ngs::Error_code(ER_X_MISSING_ARGUMENT, "Invalid index name");

  Query_string_builder qb;
  String_fields_values column_names;
  Sql_data_context::Result_info info;

  // collect the index columns (if any) to be dropped
  error = get_index_virtual_column_names(schema, table, name, da, column_names);
  if (error)
  {
    if (error.error == ER_INTERNAL_ERROR)
      return error;
    else
      // if it is not internal then the reason is bad schema or table name
      return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name: %s.%s", schema.c_str(), table.c_str());
  }

  // drop the index
  qb.put("ALTER TABLE ").quote_identifier(schema).dot().quote_identifier(table)
    .put(" DROP INDEX ").quote_identifier(name);

  // drop the index's virtual columns
  String_fields_values::const_iterator it = column_names.begin();
  for (; it != column_names.end(); ++it)
  {
    qb.put(", DROP COLUMN ").quote_identifier((*it)[0]);
  }

  const std::string &tmp(qb.get());
  log_debug("DropCollectionIndex: %s", tmp.c_str());
  error = da.execute_sql_no_result(tmp, info);
  if (error)
    return error;

  da.proto().send_exec_ok();
  return ngs::Success();
}


ngs::Error_code Admin_command_handler::enable_notices(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_enable_notices>(session.get_status_variables());

  Argument_extractor argx(args);
  const std::string *end = &fixed_notice_names[0] + sizeof(fixed_notice_names) / sizeof(fixed_notice_names[0]);

  bool enable_warnings = false;
  do
  {
    std::string notice;
    argx.string_arg("notice", notice);

    if (notice == "warnings")
      enable_warnings = true;
    else if (std::find(fixed_notice_names, end, notice) == end)
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s", notice.c_str());
  } while (!argx.is_end());
  ngs::Error_code error = argx.end();
  if (error)
    return error;

  if (enable_warnings)
    options.set_send_warnings(true);

  da.proto().send_exec_ok();
  return ngs::Success();
}


ngs::Error_code Admin_command_handler::disable_notices(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_disable_notices>(session.get_status_variables());

  Argument_extractor argx(args);
  const std::string *end = &fixed_notice_names[0] + sizeof(fixed_notice_names) / sizeof(fixed_notice_names[0]);
  bool disable_warnings = false;
  do
  {
    std::string notice;
    argx.string_arg("notice", notice);

    if (notice == "warnings")
      disable_warnings = true;
    else if (std::find(fixed_notice_names, end, notice) != end)
      return ngs::Error(ER_X_CANNOT_DISABLE_NOTICE, "Cannot disable notice %s", notice.c_str());
    else
      return ngs::Error(ER_X_BAD_NOTICE, "Invalid notice name %s", notice.c_str());
  } while (!argx.is_end());
  ngs::Error_code error = argx.end();
  if (error)
    return error;

  if (disable_warnings)
    options.set_send_warnings(false);

  da.proto().send_exec_ok();
  return ngs::Success();
}


ngs::Error_code Admin_command_handler::list_notices(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_list_notices>(session.get_status_variables());

  ngs::Error_code error = Argument_extractor(args).end();
  if (error)
    return error;

  // notice | enabled
  // <name> | <1/0>

  da.proto().send_column_metadata("", "", "", "", "notice", "", 0, Mysqlx::Resultset::ColumnMetaData::BYTES, 0, 0, 0);
  da.proto().send_column_metadata("", "", "", "", "enabled", "", 0, Mysqlx::Resultset::ColumnMetaData::SINT, 0, 0, 0);

  {
    da.proto().start_row();
    da.proto().row_builder().add_string_field("warnings", strlen("warnings"), NULL);
    da.proto().row_builder().add_longlong_field(options.get_send_warnings() ? 1 : 0, 0);
    da.proto().send_row();
  }
  {
    const size_t fixed_notices_qty = sizeof(fixed_notice_names) / sizeof(fixed_notice_names[0]);
    for (size_t i = 0; i < fixed_notices_qty; ++i)
    {
      da.proto().start_row();
      da.proto().row_builder().add_string_field(fixed_notice_names[i].c_str(), fixed_notice_names[i].length(), NULL);
      da.proto().row_builder().add_longlong_field(1, 0);
      da.proto().send_row();
    }
  }
  da.proto().send_result_fetch_done();
  da.proto().send_exec_ok();
  return ngs::Success();
}



static Callback_command_delegate::Row_data *begin_list_objects_row(Callback_command_delegate::Row_data *row,
  ngs::Protocol_encoder &proto, bool *header_sent)
{
  row->clear();

  if (!*header_sent)
  {
    // name | type
    proto.send_column_metadata("", "", "", "", "name", "", 0, Mysqlx::Resultset::ColumnMetaData::BYTES, 0, 0, 0);
    proto.send_column_metadata("", "", "", "", "type", "", 0, Mysqlx::Resultset::ColumnMetaData::BYTES, 0, 0, 0);

    *header_sent = true;
  }

  return row;
}

static bool end_list_collections_row(Callback_command_delegate::Row_data *row,
  std::set<std::string> *collection_names)
{
  Callback_command_delegate::Field_value *field(row->fields.at(0));

  if (field)
    collection_names->insert(*field->value.v_string);

  return true;
}


static bool end_list_tables_row(Callback_command_delegate::Row_data *row, ngs::Protocol_encoder &proto,
  std::set<std::string> *collection_names)
{
  Callback_command_delegate::Field_value *name_field(row->fields.at(0));
  Callback_command_delegate::Field_value *type_field(row->fields.at(1));

  if (name_field && type_field)
  {
    std::string name(*name_field->value.v_string);
    std::string type(*type_field->value.v_string);
    bool is_collection = false;
    std::set<std::string>::iterator iter;
    if ((iter = collection_names->find(name)) != collection_names->end())
    {
      if (type == "VIEW")
        collection_names->erase(iter);
      else
        is_collection = true;
    }

    if (!is_collection)
    {
      proto.start_row();

      std::string type_str(type == "BASE TABLE" ? "TABLE" : "VIEW");

      proto.row_builder().add_string_field(name.c_str(), name.length(), NULL);
      proto.row_builder().add_string_field(type_str.c_str(), type_str.length(), NULL);

      proto.send_row();
    }
  }

  return true;
}


ngs::Error_code Admin_command_handler::list_objects(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args)
{
  xpl::Server::update_status_variable<&Common_status_variables::inc_stmt_list_objects>(session.get_status_variables());

  std::string schema;
  std::string pattern;
  ngs::Error_code error = Argument_extractor(args)
      .string_arg("schema", schema, true)
      .string_arg("pattern", pattern, true)
      .end();
  if (error)
    return error;

  Sql_data_context::Result_info info;
  Callback_command_delegate::Row_data row;
  bool header_sent = false;

  Query_string_builder qb;
  qb.put("SELECT table_name, COUNT(table_name) c FROM information_schema.columns WHERE"
    " ((column_name = 'doc' and data_type = 'json') OR"
    " (column_name = '_id' and generation_expression = 'json_unquote(json_extract(`doc`,''$._id''))')) AND table_schema = ")
    .quote_string(schema.empty() ? "schema()" : schema);
  if (!pattern.empty())
    qb.put("AND table_name LIKE ").quote_string(pattern);
  qb.put(" GROUP BY table_name HAVING c = 2;");

  std::set<std::string> collection_names;
  error = da.execute_sql_and_process_results(qb.get(), boost::bind(begin_list_objects_row, &row, boost::ref(da.proto()), &header_sent),
    boost::bind(end_list_collections_row, _1, &collection_names),
    info);
  if (error)
    return error;

  qb.clear();
  if (schema.empty())
    qb.put("SHOW FULL TABLES");
  else
    qb.put("SHOW FULL TABLES FROM ").quote_identifier(schema);
  if (!pattern.empty())
    qb.put(" LIKE ").quote_string(pattern);

  error = da.execute_sql_and_process_results(qb.get(),
    boost::bind(begin_list_objects_row, &row, boost::ref(da.proto()), &header_sent),
    boost::bind(end_list_tables_row, _1, boost::ref(da.proto()), &collection_names),
    info);
  if (error)
    return error;

  for (std::set<std::string>::const_iterator col = collection_names.begin(); col != collection_names.end(); ++col)
  {
    da.proto().start_row();

    da.proto().row_builder().add_string_field(col->c_str(), col->length(), NULL);
    da.proto().row_builder().add_string_field("COLLECTION", strlen("COLLECTION"), NULL);

    da.proto().send_row();
  }

  // send metadata for the case of empty resultset
  if (!header_sent)
    begin_list_objects_row(&row, da.proto(), &header_sent);

  da.proto().send_result_fetch_done();
  da.proto().send_exec_ok();
  return ngs::Success();
}
