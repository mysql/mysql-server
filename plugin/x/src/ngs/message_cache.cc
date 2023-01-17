// Copyright (c) 2019, 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include "plugin/x/src/ngs/message_cache.h"

namespace ngs {

bool Message_cache::alloc_message(const uint8_t message_type,
                                  Message_request *out_message) {
  Message *message = nullptr;
  bool message_was_allocated = false;

  switch (static_cast<Mysqlx::ClientMessages::Type>(message_type)) {
    case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
      message = allocate_object<Mysqlx::Connection::CapabilitiesGet>();
      message_was_allocated = true;
      break;
    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
      message = allocate_object<Mysqlx::Connection::CapabilitiesSet>();
      message_was_allocated = true;
      break;
    case Mysqlx::ClientMessages::CON_CLOSE:
      message = allocate_object<Mysqlx::Connection::Close>();
      message_was_allocated = true;
      break;
    case Mysqlx::ClientMessages::SESS_CLOSE:
      message = allocate_object<Mysqlx::Session::Close>();
      message_was_allocated = true;
      break;
    case Mysqlx::ClientMessages::SESS_RESET:
      message = allocate_object<Mysqlx::Session::Reset>();
      message_was_allocated = true;
      break;
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
      message = allocate_object<Mysqlx::Session::AuthenticateStart>();
      message_was_allocated = true;
      break;
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE:
      message = allocate_object<Mysqlx::Session::AuthenticateContinue>();
      message_was_allocated = true;
      break;
    case Mysqlx::ClientMessages::SQL_STMT_EXECUTE:
      message = &m_stmt_execute;
      break;
    case Mysqlx::ClientMessages::CRUD_FIND:
      message = &m_crud_find;
      break;
    case Mysqlx::ClientMessages::CRUD_INSERT:
      message = &m_crud_insert;
      break;
    case Mysqlx::ClientMessages::CRUD_UPDATE:
      message = &m_crud_update;
      break;
    case Mysqlx::ClientMessages::CRUD_DELETE:
      message = &m_crud_delete;
      break;
    case Mysqlx::ClientMessages::EXPECT_OPEN:
      message = &m_expect_open;
      break;
    case Mysqlx::ClientMessages::EXPECT_CLOSE:
      message = &m_expect_close;
      break;
    case Mysqlx::ClientMessages::CRUD_CREATE_VIEW:
      message = &m_crud_create_view;
      break;
    case Mysqlx::ClientMessages::CRUD_MODIFY_VIEW:
      message = &m_crud_modify_view;
      break;
    case Mysqlx::ClientMessages::CRUD_DROP_VIEW:
      message = &m_crud_drop_view;
      break;
    case Mysqlx::ClientMessages::CURSOR_OPEN:
      message = &m_cursor_open;
      break;
    case Mysqlx::ClientMessages::CURSOR_CLOSE:
      message = &m_cursor_close;
      break;
    case Mysqlx::ClientMessages::CURSOR_FETCH:
      message = &m_cursor_fetch;
      break;
    case Mysqlx::ClientMessages::PREPARE_PREPARE:
      message = &m_prepare_prepare;
      break;
    case Mysqlx::ClientMessages::PREPARE_EXECUTE:
      message = &m_prepare_execute;
      break;
    case Mysqlx::ClientMessages::PREPARE_DEALLOCATE:
      message = &m_prepare_deallocate;
      break;

    default:
      break;
  }

  out_message->reset(message_type, message, message_was_allocated);

  return message;
}

}  // namespace ngs
