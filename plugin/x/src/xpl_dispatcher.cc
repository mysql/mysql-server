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

#include "plugin/x/src/xpl_dispatcher.h"
#include "plugin/x/ngs/include/ngs/interface/notice_output_queue_interface.h"
#include "plugin/x/ngs/include/ngs/ngs_error.h"
#include "plugin/x/src/xpl_session.h"

namespace xpl {
bool Dispatcher::execute(const ngs::Message_request &command) {
  ngs::Error_code error =
      m_expect_stack.pre_client_stmt(command.get_message_type());
  if (!error) {
    error = dispatch(command);
    if (error) m_session->proto().send_result(error);
    m_expect_stack.post_client_stmt(command.get_message_type(), error);
  } else {
    m_session->proto().send_result(error);
  }
  return error.error != ER_UNKNOWN_COM_ERROR;
}

ngs::Error_code Dispatcher::dispatch(const ngs::Message_request &command) {
  switch (command.get_message_type()) {
    case Mysqlx::ClientMessages::SQL_STMT_EXECUTE:
      return m_stmt_handler.execute(
          static_cast<const Mysqlx::Sql::StmtExecute &>(
              *command.get_message()));

    case Mysqlx::ClientMessages::CRUD_FIND:
      return m_crud_handler.execute_crud_find(
          static_cast<const Mysqlx::Crud::Find &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_INSERT:
      return m_crud_handler.execute_crud_insert(
          static_cast<const Mysqlx::Crud::Insert &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_UPDATE:
      return m_crud_handler.execute_crud_update(
          static_cast<const Mysqlx::Crud::Update &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_DELETE:
      return m_crud_handler.execute_crud_delete(
          static_cast<const Mysqlx::Crud::Delete &>(*command.get_message()));

    case Mysqlx::ClientMessages::CRUD_CREATE_VIEW:
      return m_crud_handler.execute_create_view(
          static_cast<const Mysqlx::Crud::CreateView &>(
              *command.get_message()));

    case Mysqlx::ClientMessages::CRUD_MODIFY_VIEW:
      return m_crud_handler.execute_modify_view(
          static_cast<const Mysqlx::Crud::ModifyView &>(
              *command.get_message()));

    case Mysqlx::ClientMessages::CRUD_DROP_VIEW:
      return m_crud_handler.execute_drop_view(
          static_cast<const Mysqlx::Crud::DropView &>(*command.get_message()));

    case Mysqlx::ClientMessages::EXPECT_OPEN:
      return on_expect_open(
          static_cast<const Mysqlx::Expect::Open &>(*command.get_message()));

    case Mysqlx::ClientMessages::EXPECT_CLOSE:
      return on_expect_close();

    case Mysqlx::ClientMessages::PREPARE_PREPARE:
      return m_prepare_handler.execute_prepare(
          static_cast<const Mysqlx::Prepare::Prepare &>(
              *command.get_message()));

    case Mysqlx::ClientMessages::PREPARE_EXECUTE:
      return m_prepare_handler.execute_execute(
          static_cast<const Mysqlx::Prepare::Execute &>(
              *command.get_message()));

    case Mysqlx::ClientMessages::PREPARE_DEALLOCATE:
      return m_prepare_handler.execute_deallocate(
          static_cast<const Mysqlx::Prepare::Deallocate &>(
              *command.get_message()));

    case Mysqlx::ClientMessages::CURSOR_OPEN:
      return m_prepare_handler.execute_cursor_open(
          static_cast<const Mysqlx::Cursor::Open &>(*command.get_message()));

    case Mysqlx::ClientMessages::CURSOR_FETCH:
      return m_prepare_handler.execute_cursor_fetch(
          static_cast<const Mysqlx::Cursor::Fetch &>(*command.get_message()));

    case Mysqlx::ClientMessages::CURSOR_CLOSE:
      return m_prepare_handler.execute_cursor_close(
          static_cast<const Mysqlx::Cursor::Close &>(*command.get_message()));
  }

  m_session->proto().get_protocol_monitor().on_error_unknown_msg_type();
  return ngs::Error(ER_UNKNOWN_COM_ERROR, "Unexpected message received");
}

ngs::Error_code Dispatcher::on_expect_open(const Mysqlx::Expect::Open &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_expect_open);
  ngs::Error_code error = m_expect_stack.open(msg);
  if (!error) m_session->proto().send_ok();
  return error;
}

ngs::Error_code Dispatcher::on_expect_close() {
  m_session->update_status(&ngs::Common_status_variables::m_expect_close);
  ngs::Error_code error = m_expect_stack.close();
  if (!error) m_session->proto().send_ok();
  return error;
}

void Dispatcher::reset() {
  m_prepare_handler = Prepare_command_handler{m_session};
}
}  // namespace xpl
