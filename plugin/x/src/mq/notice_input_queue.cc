/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/x/src/mq/notice_input_queue.h"

#include "plugin/x/ngs/include/ngs/interface/server_task_interface.h"
#include "plugin/x/src/mq/broker_task.h"

namespace xpl {

Notice_input_queue::Notice_input_queue() : m_context(new Broker_context()) {}

bool Notice_input_queue::emplace(const Notice_type notice_id,
                                 const std::string &notice_payload) {
  auto sync = m_context->m_synchronize.block();

  if (State::k_eol == m_context->m_state ||
      State::k_closing == m_context->m_state)
    return false;

  m_context->m_queue.emplace(notice_id, notice_payload);
  sync.notify();

  return true;
}

std::unique_ptr<ngs::Server_task_interface>
Notice_input_queue::create_broker_task() {
  std::unique_ptr<ngs::Server_task_interface> task(new Broker_task(m_context));
  return task;
}

}  // namespace xpl
