/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_CRUD_CMD_HANDLER_H_
#define PLUGIN_X_SRC_CRUD_CMD_HANDLER_H_

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/resultset_interface.h"
#include "plugin/x/ngs/include/ngs/protocol_fwd.h"
#include "plugin/x/ngs/include/ngs/session_status_variables.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_context.h"

namespace xpl {
class Session;

class Crud_command_handler {
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
  using Status_variable =
      ngs::Common_status_variables::Variable ngs::Common_status_variables::*;

  template <typename B, typename M>
  ngs::Error_code execute(Session &session, const B &builder, const M &msg,
                          ngs::Resultset_interface &resultset,
                          Status_variable variable,
                          bool (ngs::Protocol_encoder_interface::*send_ok)());

  template <typename M>
  ngs::Error_code error_handling(const ngs::Error_code &error,
                                 const M & /*msg*/) const {
    return error;
  }

  template <typename B, typename M>
  void notice_handling(Session &session,
                       const ngs::Resultset_interface::Info &info,
                       const B &builder, const M &msg) const;

  void notice_handling_common(Session &session,
                              const ngs::Resultset_interface::Info &info) const;

  Query_string_builder m_qb;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CRUD_CMD_HANDLER_H_
