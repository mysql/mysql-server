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

#ifndef PLUGIN_X_SRC_CRUD_CMD_HANDLER_H_
#define PLUGIN_X_SRC_CRUD_CMD_HANDLER_H_

#include "plugin/x/src/interface/resultset.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol_fwd.h"
#include "plugin/x/src/ngs/session_status_variables.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_context.h"

namespace xpl {

class Crud_command_handler {
 public:
  explicit Crud_command_handler(iface::Session *session)
      : m_session{session}, m_qb{1024} {}

  ngs::Error_code execute_crud_insert(const Mysqlx::Crud::Insert &msg);
  ngs::Error_code execute_crud_update(const Mysqlx::Crud::Update &msg);
  ngs::Error_code execute_crud_find(const Mysqlx::Crud::Find &msg);
  ngs::Error_code execute_crud_delete(const Mysqlx::Crud::Delete &msg);

  ngs::Error_code execute_create_view(const Mysqlx::Crud::CreateView &msg);
  ngs::Error_code execute_modify_view(const Mysqlx::Crud::ModifyView &msg);
  ngs::Error_code execute_drop_view(const Mysqlx::Crud::DropView &msg);

 private:
  using Status_variable =
      ngs::Common_status_variables::Variable ngs::Common_status_variables::*;

  template <typename B, typename M>
  ngs::Error_code execute(const B &builder, const M &msg,
                          iface::Resultset &resultset, Status_variable variable,
                          bool (iface::Protocol_encoder::*send_ok)());

  template <typename M>
  ngs::Error_code error_handling(const ngs::Error_code &error,
                                 const M & /*msg*/) const {
    return error;
  }

  template <typename B, typename M>
  void notice_handling(const iface::Resultset::Info &info, const B &builder,
                       const M &msg) const;

  void notice_handling_common(const iface::Resultset::Info &info) const;

  iface::Session *m_session;
  Query_string_builder m_qb;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CRUD_CMD_HANDLER_H_
