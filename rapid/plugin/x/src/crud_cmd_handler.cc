/*
 * Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.
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

#include "ngs_common/protocol_protobuf.h"

#include "xpl_log.h"
#include "expr_generator.h"
#include "xpl_error.h"
#include "update_statement_builder.h"
#include "find_statement_builder.h"
#include "delete_statement_builder.h"
#include "insert_statement_builder.h"
#include "view_statement_builder.h"
#include "notices.h"
#include "xpl_session.h"


namespace xpl
{

template <typename B, typename M>
ngs::Error_code Crud_command_handler::execute(
    Session &session, const B &builder, const M &msg, Status_variable variable,
    bool (ngs::Protocol_encoder::*send_ok)())
{
  session.update_status(variable);
  m_qb.clear();
  try
  {
    builder.build(msg);
  }
  catch (const Expression_generator::Error &exc)
  {
    return ngs::Error(exc.error(), "%s", exc.what());
  }
  catch (const ngs::Error_code &error)
  {
    return error;
  }
  log_debug("CRUD query: %s", m_qb.get().c_str());
  Sql_data_context::Result_info info;
  ngs::Error_code error = sql_execute<M>(session, info);
  if (error)
    return error_handling(error, msg);

  notice_handling(session, info, msg);
  (session.proto().*send_ok)();
  return ngs::Success();
}


template <typename M>
void Crud_command_handler::notice_handling(
    Session &session, const Sql_data_context::Result_info &info,
    const M & /*msg*/) const
{
  notice_handling_common(session, info);
}


void Crud_command_handler::notice_handling_common(
    Session &session, const Sql_data_context::Result_info &info) const
{
  if (info.num_warnings > 0 && session.options().get_send_warnings())
    notices::send_warnings(session.data_context(), session.proto());

  if (!info.message.empty())
    notices::send_message(session.proto(), info.message);
}


template <typename M>
ngs::Error_code Crud_command_handler::sql_execute(
    Session &session, Sql_data_context::Result_info &info) const
{
  return session.data_context().execute_sql_no_result(
      m_qb.get().data(), m_qb.get().length(), info);
}


// -- Insert
ngs::Error_code Crud_command_handler::execute_crud_insert(
    Session &session, const Mysqlx::Crud::Insert &msg)
{
  Expression_generator gen(m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  return execute(session, Insert_statement_builder(gen), msg,
                 &Common_status_variables::m_crud_insert,
                 &ngs::Protocol_encoder::send_exec_ok);
}


template <>
ngs::Error_code Crud_command_handler::error_handling(
    const ngs::Error_code &error, const Mysqlx::Crud::Insert &msg) const
{
  if (is_table_data_model(msg))
    return error;

  switch (error.error)
  {
  case ER_BAD_NULL_ERROR:
    return ngs::Error(ER_X_DOC_ID_MISSING,
                      "Document is missing a required field");

  case ER_BAD_FIELD_ERROR:
    return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                      "Table '%s' is not a document collection",
                      msg.collection().name().c_str());

  case ER_DUP_ENTRY:
    return ngs::Error(ER_X_DOC_ID_DUPLICATE,
                      "Document contains a field value that is not unique but "
                      "required to be");
  }
  return error;
}


template<>
void Crud_command_handler::notice_handling(
    Session &session, const Sql_data_context::Result_info &info,
    const Mysqlx::Crud::Insert &msg) const
{
  notice_handling_common(session, info);
  notices::send_rows_affected(session.proto(), info.affected_rows);
  if (is_table_data_model(msg))
    notices::send_generated_insert_id(session.proto(), info.last_insert_id);
}


// -- Update
ngs::Error_code Crud_command_handler::execute_crud_update(
    Session &session, const Mysqlx::Crud::Update &msg)
{
  Expression_generator gen(m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  return execute(session, Update_statement_builder(gen), msg,
                 &Common_status_variables::m_crud_update,
                 &ngs::Protocol_encoder::send_exec_ok);
}


template<>
ngs::Error_code Crud_command_handler::error_handling(
    const ngs::Error_code &error, const Mysqlx::Crud::Update &msg) const
{
  if (is_table_data_model(msg))
    return error;

  switch (error.error)
  {
  case ER_INVALID_JSON_TEXT_IN_PARAM:
    return ngs::Error(ER_X_BAD_UPDATE_DATA,
                      "Invalid data for update operation on "
                      "document collection table");
  }
  return error;
}


template<>
void Crud_command_handler::notice_handling(
    Session &session, const Sql_data_context::Result_info &info,
    const Mysqlx::Crud::Update &msg) const
{
  notice_handling_common(session, info);
  notices::send_rows_affected(session.proto(), info.affected_rows);
}


// -- Delete
ngs::Error_code Crud_command_handler::execute_crud_delete(
    Session &session, const Mysqlx::Crud::Delete &msg)
{
  Expression_generator gen(m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  return execute(session, Delete_statement_builder(gen), msg,
                 &Common_status_variables::m_crud_delete,
                 &ngs::Protocol_encoder::send_exec_ok);
}


template<>
void Crud_command_handler::notice_handling(
    Session &session, const Sql_data_context::Result_info &info,
    const Mysqlx::Crud::Delete &msg) const
{
  notice_handling_common(session, info);
  notices::send_rows_affected(session.proto(), info.affected_rows);
}


// -- Find
ngs::Error_code Crud_command_handler::execute_crud_find(
    Session &session, const Mysqlx::Crud::Find &msg)
{
  Expression_generator gen(m_qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  return execute(session, Find_statement_builder(gen), msg,
                 &Common_status_variables::m_crud_find,
                 &ngs::Protocol_encoder::send_exec_ok);
}

namespace
{
inline bool check_message(const std::string &msg, const char *pattern,
                          std::string::size_type &pos)
{
  return (pos = msg.find(pattern)) != std::string::npos;
}
} // namespace


template<>
ngs::Error_code Crud_command_handler::error_handling(
    const ngs::Error_code &error, const Mysqlx::Crud::Find &msg) const
{
  if (is_table_data_model(msg))
    return error;

  switch (error.error)
  {
  case ER_BAD_FIELD_ERROR:
    std::string::size_type pos = std::string::npos;
    if (check_message(error.message, "having clause", pos))
      return ngs::Error(ER_X_EXPR_BAD_VALUE,
                        "Invalid expression in grouping criteria");

    if (check_message(error.message, "where clause", pos))
      return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING, "%sselection criteria",
                        error.message.substr(0, pos - 1).c_str());

    if (check_message(error.message, "field list", pos))
      return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING, "%scollection",
                        error.message.substr(0, pos - 1).c_str());
  }
  return error;
}


template <>
ngs::Error_code Crud_command_handler::sql_execute<Mysqlx::Crud::Find>(
    Session &session, Sql_data_context::Result_info &info) const
{
  return session.data_context().execute_sql_and_stream_results(
      m_qb.get().data(), m_qb.get().length(), false, info);
}


// -- View
ngs::Error_code Crud_command_handler::execute_create_view(
    Session &session, const Mysqlx::Crud::CreateView &msg)
{
  Expression_generator gen(m_qb, Expression_generator::Args(),
                           msg.collection().schema(), true);
  return execute(session, View_statement_builder(gen), msg,
                 &Common_status_variables::m_crud_create_view,
                 &ngs::Protocol_encoder::send_ok);
}


ngs::Error_code Crud_command_handler::execute_modify_view(
    Session &session, const Mysqlx::Crud::ModifyView &msg)
{
  Expression_generator gen(m_qb, Expression_generator::Args(),
                           msg.collection().schema(), true);
  return execute(session, View_statement_builder(gen), msg,
                 &Common_status_variables::m_crud_modify_view,
                 &ngs::Protocol_encoder::send_ok);
}


ngs::Error_code Crud_command_handler::execute_drop_view(
    Session &session, const Mysqlx::Crud::DropView &msg)
{
  Expression_generator gen(m_qb, Expression_generator::Args(),
                           msg.collection().schema(), true);
  return execute(session, View_statement_builder(gen), msg,
                 &Common_status_variables::m_crud_drop_view,
                 &ngs::Protocol_encoder::send_ok);
}

}  // namespace xpl
