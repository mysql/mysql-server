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

#ifndef _XPL_CRUD_CMD_HANDLER_H_
#define _XPL_CRUD_CMD_HANDLER_H_

#include "ngs/error_code.h"
#include "ngs/protocol_fwd.h"
#include "query_string_builder.h"
#include "sql_data_context.h"
#include "xpl_session_status_variables.h"


namespace xpl
{
class Session;

class Crud_command_handler
{
public:
  Crud_command_handler() : m_qb(1024) {}

  ngs::Error_code execute_crud_insert(Session &session,
                                      const Mysqlx::Crud::Insert &msg);
  ngs::Error_code execute_crud_update(Session &session,
                                      const Mysqlx::Crud::Update &msg);
  ngs::Error_code execute_crud_find(Session &session,
                                    const Mysqlx::Crud::Find &msg);
  ngs::Error_code execute_crud_delete(Session &session,
                                      const Mysqlx::Crud::Delete &msg);

  ngs::Error_code execute_create_view(Session &session,
                                      const Mysqlx::Crud::CreateView &msg);
  ngs::Error_code execute_modify_view(Session &session,
                                      const Mysqlx::Crud::ModifyView &msg);
  ngs::Error_code execute_drop_view(Session &session,
                                    const Mysqlx::Crud::DropView &msg);

private:
 typedef Common_status_variables::Variable
     Common_status_variables::*Status_variable;

  template <typename B, typename M>
  ngs::Error_code execute(Session &session, const B &builder, const M &msg,
                          Status_variable variable,
                          bool (ngs::Protocol_encoder::*send_ok)());

  template <typename M>
  ngs::Error_code error_handling(const ngs::Error_code &error,
                                 const M & /*msg*/) const
  {
    return error;
  }

  template <typename M>
  void notice_handling(Session &session,
                       const Sql_data_context::Result_info &info,
                       const M &msg) const;

  void notice_handling_common(Session &session,
                              const Sql_data_context::Result_info &info) const;

  template <typename M>
  ngs::Error_code sql_execute(Session &session,
                              Sql_data_context::Result_info &info) const;

  Query_string_builder m_qb;
};

} // namespace xpl

#endif // _XPL_CRUD_CMD_HANDLER_H_
