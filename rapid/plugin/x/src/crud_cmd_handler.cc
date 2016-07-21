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

#include "ngs_common/protocol_protobuf.h"

#include "xpl_log.h"
#include "sql_data_context.h"
#include "expr_generator.h"
#include "xpl_error.h"
#include "update_statement_builder.h"
#include "find_statement_builder.h"
#include "delete_statement_builder.h"
#include "insert_statement_builder.h"
#include "notices.h"
#include "xpl_server.h"
#include "xpl_session.h"

namespace
{

template<typename T>
inline bool is_table_data_model(const T& msg)
{
  return msg.data_model() == Mysqlx::Crud::TABLE;
}

} // namespace


ngs::Error_code xpl::Crud_command_handler::execute_crud_insert(Session &session, const Mysqlx::Crud::Insert &msg)
{
  session.update_status<&Common_status_variables::inc_crud_insert>();

  m_qb.clear();
  ngs::Error_code error = Insert_statement_builder(msg, m_qb).build();
  if (error)
    return error;

  Sql_data_context::Result_info info;
  error = session.data_context().execute_sql_no_result(m_qb.get(), info);
  if (error)
    return error_handling_insert(error, msg);

  if (info.num_warnings > 0 && session.options().get_send_warnings())
    notices::send_warnings(session.data_context(), session.proto());
  notices::send_rows_affected(session.proto(), info.affected_rows);
  if (is_table_data_model(msg))
    notices::send_generated_insert_id(session.proto(), info.last_insert_id);
  if (!info.message.empty())
    notices::send_message(session.proto(), info.message);
  session.proto().send_exec_ok();
  return ngs::Success();
}


ngs::Error_code xpl::Crud_command_handler::error_handling_insert(const ngs::Error_code &error,
                                                                 const Mysqlx::Crud::Insert &msg) const
{
  if (is_table_data_model(msg))
    return error;

  //XXX do some work to find the column that's violating constraints and fix the error message (or just append the original error)
  switch (error.error)
  {
  case ER_BAD_NULL_ERROR:
    return ngs::Error(ER_X_DOC_ID_MISSING,
                      "Document is missing a required field");

  case ER_BAD_FIELD_ERROR:
    return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                      "Table '%s' is not a document collection", msg.collection().name().c_str());

  case ER_DUP_ENTRY:
    return ngs::Error(ER_X_DOC_ID_DUPLICATE,
                      "Document contains a field value that is not unique but required to be");
  }
  return error;
}


ngs::Error_code xpl::Crud_command_handler::execute_crud_update(Session &session, const Mysqlx::Crud::Update &msg)
{
  session.update_status<&Common_status_variables::inc_crud_update>();

  m_qb.clear();
  ngs::Error_code error = Update_statement_builder(msg, m_qb).build();
  if (error)
    return error;

  Sql_data_context::Result_info info;
  error = session.data_context().execute_sql_no_result(m_qb.get(), info);
  if (error)
    return error_handling_update(error, msg);

  if (info.num_warnings > 0 && session.options().get_send_warnings())
    notices::send_warnings(session.data_context(), session.proto());
  notices::send_rows_affected(session.proto(), info.affected_rows);
  if (!info.message.empty())
    notices::send_message(session.proto(), info.message);
  session.proto().send_exec_ok();
  return ngs::Success();
}


ngs::Error_code xpl::Crud_command_handler::error_handling_update(const ngs::Error_code &error,
                                                                 const Mysqlx::Crud::Update &msg) const
{
  if (is_table_data_model(msg))
    return error;

  switch (error.error)
  {
  case ER_INVALID_JSON_TEXT_IN_PARAM:
    return ngs::Error(ER_X_BAD_UPDATE_DATA,
                      "Invalid data for update operation on document collection table");
  }
  return error;
}


ngs::Error_code xpl::Crud_command_handler::execute_crud_delete(Session &session, const Mysqlx::Crud::Delete &msg)
{
  session.update_status<&Common_status_variables::inc_crud_delete>();

  m_qb.clear();
  ngs::Error_code error = Delete_statement_builder(msg, m_qb).build();
  if (error)
    return error;

  Sql_data_context::Result_info info;
  error = session.data_context().execute_sql_no_result(m_qb.get(), info);
  if (error)
    return error;

  if (info.num_warnings > 0 && session.options().get_send_warnings())
    notices::send_warnings(session.data_context(), session.proto());
  notices::send_rows_affected(session.proto(), info.affected_rows);
  if (!info.message.empty())
    notices::send_message(session.proto(), info.message);
  session.proto().send_exec_ok();
  return ngs::Success();
}


ngs::Error_code xpl::Crud_command_handler::execute_crud_find(Session &session, const Mysqlx::Crud::Find &msg)
{
  session.update_status<&Common_status_variables::inc_crud_find>();

  m_qb.clear();
  ngs::Error_code error = Find_statement_builder(msg, m_qb).build();
  if (error)
    return error;

  Sql_data_context::Result_info info;
  error = session.data_context().execute_sql_and_stream_results(m_qb.get(), false, info);
  if (error)
    return error_handling_find(error, msg);

  if (info.num_warnings > 0 && session.options().get_send_warnings())
    notices::send_warnings(session.data_context(), session.proto());
  if (!info.message.empty())
    notices::send_message(session.proto(), info.message);
  session.proto().send_exec_ok();
  return ngs::Success();
}


namespace
{

inline bool check_message(const std::string &msg, const char *pattern, std::string::size_type &pos)
{
  return (pos = msg.find(pattern)) != std::string::npos;
}

} // namespace


ngs::Error_code xpl::Crud_command_handler::error_handling_find(const ngs::Error_code &error,
                                                               const Mysqlx::Crud::Find &msg) const
{
  if (is_table_data_model(msg))
    return error;

  switch (error.error)
  {
  case ER_BAD_FIELD_ERROR:
    std::string::size_type pos = std::string::npos;
    if (check_message(error.message, "having clause", pos))
      return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                        "%sgrouping criteria", error.message.substr(0, pos-1).c_str());
    if (check_message(error.message, "where clause", pos))
      return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                        "%sselection criteria", error.message.substr(0, pos-1).c_str());
    if (check_message(error.message, "field list", pos))
      return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                        "%scollection", error.message.substr(0, pos-1).c_str());
  }
  return error;
}
