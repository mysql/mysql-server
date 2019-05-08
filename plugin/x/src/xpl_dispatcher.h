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

#ifndef PLUGIN_X_SRC_XPL_DISPATCHER_H_
#define PLUGIN_X_SRC_XPL_DISPATCHER_H_

#include "plugin/x/ngs/include/ngs/protocol/message.h"
#include "plugin/x/src/admin_cmd_handler.h"
#include "plugin/x/src/crud_cmd_handler.h"
#include "plugin/x/src/expect/expect_stack.h"
#include "plugin/x/src/prepare_command_handler.h"
#include "plugin/x/src/stmt_command_handler.h"

namespace xpl {

class Session;

class Dispatcher {
 public:
  explicit Dispatcher(ngs::Session_interface *session) : m_session{session} {}
  bool execute(const ngs::Message_request &command);
  void reset();

  const Prepare_command_handler::Prepared_stmt_info_list &
  get_prepared_stmt_info() const {
    return m_prepare_handler.get_prepared_stmt_info();
  }

 private:
  ngs::Error_code dispatch(const ngs::Message_request &command);
  ngs::Error_code on_expect_open(const Mysqlx::Expect::Open &msg);
  ngs::Error_code on_expect_close();

  ngs::Session_interface *m_session;
  Crud_command_handler m_crud_handler{m_session};
  Expectation_stack m_expect_stack;
  Stmt_command_handler m_stmt_handler{m_session};
  Prepare_command_handler m_prepare_handler{m_session};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_DISPATCHER_H_
