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

#include "plugin/x/src/mq/broker_task.h"

#include <algorithm>
#include <map>

#include "my_dbug.h"

#include "plugin/x/ngs/include/ngs/protocol/message.h"
#include "plugin/x/src/xpl_global_status_variables.h"

namespace xpl {

using Notice = ::Mysqlx::Notice::Frame;

Broker_task::Broker_task(std::shared_ptr<Broker_context> context)
    : m_broker_context(context) {
  DBUG_ASSERT(nullptr != m_broker_context.get());
}

bool Broker_task::prepare(Task_context *context) {
  m_task_context = *context;
  return true;
}

void Broker_task::stop(const Stop_cause) {
  auto sync = m_broker_context->m_synchronize.block();

  if (State::k_eol == m_broker_context->m_state ||
      State::k_closing == m_broker_context->m_state)
    return;

  m_broker_context->m_state = State::k_initializing == m_broker_context->m_state
                                  ? State::k_eol
                                  : State::k_closing;
  sync.notify();

  while (State::k_eol != m_broker_context->m_state) {
    /* Lets wait until the state of the task changes to k_eol.
     *
     * 'wait' call stops current thread from executing until another thread
     * is going wake-it up by 'sync.notify'. Waiting doesn't consume
     * CPU cycles.
     */
    sync.wait();
  }
}

void Broker_task::pre_loop() {
  auto sync = m_broker_context->m_synchronize.block();

  if (State::k_initializing != m_broker_context->m_state) return;

  m_broker_context->m_state = State::k_running;
  sync.notify();
}

void Broker_task::post_loop() {
  auto sync = m_broker_context->m_synchronize.block();

  m_broker_context->m_state = State::k_eol;
  sync.notify();
}

void Broker_task::loop() {
  Publish_queue workers_queue;

  /* Moves the ownership of queue to looper thread (workers_queue). */
  wait_for_data_and_swap_queues(&workers_queue);

  /* Global status variables, add operator is already thread safe.
   * No additional synchronization needed. */
  auto &globals = xpl::Global_status_variables::instance();
  globals.m_notified_by_group_replication += workers_queue.size();

  while (!workers_queue.empty()) {
    const auto &notice_description = workers_queue.front();

    distribute(notice_description);

    workers_queue.pop();
  }
}

void Broker_task::wait_for_data_and_swap_queues(
    Publish_queue *out_workers_queue) {
  auto sync = m_broker_context->m_synchronize.block();

  if (State::k_closing == m_broker_context->m_state) return;

  if (m_broker_context->m_queue.empty()) {
    sync.wait();

    if (State::k_closing == m_broker_context->m_state) return;
  }

  out_workers_queue->swap(m_broker_context->m_queue);
}

void Broker_task::distribute(const Notice_descriptor &notice_descriptor) {
  if (nullptr == m_task_context.m_client_list) return;

  auto binary_notice = create_notice_message(notice_descriptor);

  m_task_context.m_client_list->enumerate(
      [&notice_descriptor,
       &binary_notice](std::shared_ptr<ngs::Client_interface> &client) -> bool {
        auto &session_out_queue = client->session()->get_notice_output_queue();

        session_out_queue.emplace(notice_descriptor.m_notice_type,
                                  binary_notice);
        return false;
      });
}

std::shared_ptr<std::string> Broker_task::create_notice_message(
    const Notice_descriptor &notice_description) {
  using Protocol_type = Mysqlx::Notice::GroupReplicationStateChanged_Type;
  using Notice_type_map = std::map<Notice_type, Protocol_type>;

  const static Notice_type_map map_types{
      {Notice_type::k_group_replication_quorum_loss,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBERSHIP_QUORUM_LOSS},
      {Notice_type::k_group_replication_view_changed,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE},
      {Notice_type::k_group_replication_member_role_changed,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBER_ROLE_CHANGE},
      {Notice_type::k_group_replication_member_state_changed,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBER_STATE_CHANGE}};

  DBUG_ASSERT(map_types.count(notice_description.m_notice_type) == 1 &&
              std::any_of(Notice_descriptor::dispatchables.begin(),
                          Notice_descriptor::dispatchables.end(),
                          [&notice_description](Notice_type nt) {
                            return nt == notice_description.m_notice_type;
                          }));

  ::Mysqlx::Notice::GroupReplicationStateChanged group_replication_state_change;
  std::shared_ptr<std::string> binary_notice{new std::string};

  group_replication_state_change.set_type(
      map_types.at(notice_description.m_notice_type));

  if (!notice_description.m_payload.empty())
    group_replication_state_change.set_view_id(notice_description.m_payload);

  group_replication_state_change.SerializeToString(binary_notice.get());

  return binary_notice;
}  // namespace xpl

}  // namespace xpl
