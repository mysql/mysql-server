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

#include "crud_cmd_handler.h"
#include "admin_cmd_handler.h"

#include "xpl_log.h"
#include "ngs/mysqlx/getter_any.h"
#include "expr_generator.h"
#include "sql_data_context.h"
#include "xpl_server.h"
#include "xpl_session.h"
#include "xpl_dispatcher.h"
#include "xpl_error.h"
#include "mysqlx.pb.h"
#include "notices.h"
#include "expect.h"


using namespace xpl;

using xpl::Server;

class Stmt
{
public:
  ngs::Error_code execute(Sql_data_context &da, ngs::Protocol_encoder &proto, const bool show_warnings,
                          const bool compact_metadata, const std::string &query,
                          const ::google::protobuf::RepeatedPtrField< ::Mysqlx::Datatypes::Any > &args)
  {
    const int args_size = args.size();

    if (0 == args_size)
      return execute(da, proto, show_warnings, compact_metadata, query);

    m_qb.clear();
    m_qb.put(query);

    try
    {
      for (int i = 0; i < args_size; ++i)
      {
        ngs::Getter_any::put_scalar_value_to_functor(args.Get(i), *this);
      }
    }
    catch (const ngs::Error_code &error)
    {
      return error;
    }

    return execute(da, proto, show_warnings, compact_metadata, m_qb.get());
  }

  ngs::Error_code execute(Sql_data_context &da, ngs::Protocol_encoder &proto, const bool show_warnings, const bool compact_metadata, const std::string &query)
  {
    Sql_data_context::Result_info info;
    ngs::Error_code error = da.execute_sql_and_stream_results(query, compact_metadata, info);

    if (!error)
    {
      if (info.num_warnings > 0 && show_warnings)
        notices::send_warnings(da, proto);
      notices::send_rows_affected(proto, info.affected_rows);
      if (info.last_insert_id > 0)
        notices::send_generated_insert_id(proto, info.last_insert_id);
      if (!info.message.empty())
        notices::send_message(proto, info.message);
      proto.send_exec_ok();
    }
    else
    {
      if (show_warnings)
        notices::send_warnings(da, proto, true);
    }

    return error;
  }

  void operator() ()
  {
    const char *value_null = "NULL";

    m_qb.format() % Query_formatter::No_escape<const char*>(value_null);
  }

  template <typename Value_type>
  void operator() (const Value_type &value)
  {
    m_qb.format() % value;
  }

private:
  Query_string_builder m_qb;
};


ngs::Error_code on_stmt_execute(Session &session,
                                Sql_data_context &da, Session_options &options,
                                ngs::Protocol_encoder &proto,
                                const Mysqlx::Sql::StmtExecute &msg)
{
  log_debug("%s: %s", session.client().client_id(), msg.stmt().c_str());
  if (msg.namespace_() == "sql" || !msg.has_namespace_())
  {
    Server::update_status_variable<&Common_status_variables::inc_stmt_execute_sql>(session.get_status_variables());

    Stmt stmt;
    return stmt.execute(da, proto, options.get_send_warnings(), msg.compact_metadata(), msg.stmt(), msg.args());
  }
  else if (msg.namespace_() == "xplugin")
  {
    Server::update_status_variable<&Common_status_variables::inc_stmt_execute_xplugin>(session.get_status_variables());

    return Admin_command_handler::execute(session, da, options, msg.stmt(), msg.args());
  }
  else
    return ngs::Error_code(ER_X_INVALID_NAMESPACE, "Unknown namespace "+msg.namespace_());
}


ngs::Error_code on_expect_open(Session &session, ngs::Protocol_encoder &proto, Expectation_stack &expect, Session_options &options, const Mysqlx::Expect::Open &msg)
{
  Server::update_status_variable<&Common_status_variables::inc_expect_open>(session.get_status_variables());

  ngs::Error_code error = expect.open(msg);
  if (!error)
    proto.send_ok("");
  return error;
}


ngs::Error_code on_expect_close(Session &session, ngs::Protocol_encoder &proto, Expectation_stack &expect, Session_options &options, const Mysqlx::Expect::Close &msg)
{
  Server::update_status_variable<&Common_status_variables::inc_expect_close>(session.get_status_variables());

  ngs::Error_code error = expect.close();
  if (!error)
    proto.send_ok("");
  return error;
}


static ngs::Error_code do_dispatch_command(Session &session,
                                           Sql_data_context &da, ngs::Protocol_encoder &proto,
                                           Crud_command_handler &crudh, Expectation_stack &expect,
                                           Session_options &options,
                                           ngs::Request &command)
{
  switch (command.get_type())
  {
    case Mysqlx::ClientMessages::SQL_STMT_EXECUTE:
      return on_stmt_execute(session, da, options, proto, static_cast<const Mysqlx::Sql::StmtExecute&>(*command.message()));

    case Mysqlx::ClientMessages::CRUD_FIND:
      return crudh.execute_crud_find(proto, static_cast<const Mysqlx::Crud::Find&>(*command.message()));

    case Mysqlx::ClientMessages::CRUD_INSERT:
      return crudh.execute_crud_insert(proto, static_cast<const Mysqlx::Crud::Insert&>(*command.message()));

    case Mysqlx::ClientMessages::CRUD_UPDATE:
      return crudh.execute_crud_update(proto, static_cast<const Mysqlx::Crud::Update&>(*command.message()));

    case Mysqlx::ClientMessages::CRUD_DELETE:
      return crudh.execute_crud_delete(proto, static_cast<const Mysqlx::Crud::Delete&>(*command.message()));

    case Mysqlx::ClientMessages::EXPECT_OPEN:
      return on_expect_open(session, proto, expect, options, static_cast<const Mysqlx::Expect::Open&>(*command.message()));

    case Mysqlx::ClientMessages::EXPECT_CLOSE:
      return on_expect_close(session, proto, expect, options, static_cast<const Mysqlx::Expect::Close&>(*command.message()));
  }
  return ngs::Error(ER_UNKNOWN_COM_ERROR, "Unexpected message received");
}


bool dispatcher::dispatch_command(Session &session,
                                  Sql_data_context &da, ngs::Protocol_encoder &proto,
                                  Crud_command_handler &crudh, Expectation_stack &expect,
                                  Session_options &options,
                                  ngs::Request &command)
{
  ngs::Error_code error = expect.pre_client_stmt(command.get_type());
  if (!error)
  {
    error = do_dispatch_command(session, da, proto, crudh, expect, options, command);
    if (error)
      proto.send_result(error);
    expect.post_client_stmt(command.get_type(), error);
  }
  else
    proto.send_result(error);
  return error.error != ER_UNKNOWN_COM_ERROR;
}
