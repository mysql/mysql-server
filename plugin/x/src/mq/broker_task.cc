/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/x/src/mq/broker_task.h"

#include <assert.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>

#include "plugin/x/src/helper/multithread/xsync_point.h"
#include "plugin/x/src/ngs/protocol/message.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"

namespace xpl {

using Notice = ::Mysqlx::Notice::Frame;

Broker_task::Broker_task(std::shared_ptr<Broker_context> context)
    : m_broker_context(context) {
  assert(nullptr != m_broker_context.get());
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

    XSYNC_POINT_CHECK(XSYNC_WAIT("gr_notice_bug_broker_dispatch"));
    distribute(notice_description);
    XSYNC_POINT_CHECK(XSYNC_WAIT_NONE,
                      XSYNC_WAKE("gr_notice_bug_client_accept"));

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
      [&binary_notice](std::shared_ptr<iface::Client> &client) -> bool {
        auto session = client->session();
        if (!session) return false;

        auto &session_out_queue = session->get_notice_output_queue();

        session_out_queue.emplace(binary_notice);
        return false;
      });
}

std::shared_ptr<Broker_task::Notice_descriptor>
Broker_task::create_notice_message(
    const Notice_descriptor &notice_description) {
  using Protocol_type = Mysqlx::Notice::GroupReplicationStateChanged_Type;
  using Notice_type_map = std::map<Notice_type, Protocol_type>;

  static const Notice_type_map k_map_types{
      {Notice_type::k_group_replication_quorum_loss,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBERSHIP_QUORUM_LOSS},
      {Notice_type::k_group_replication_view_changed,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE},
      {Notice_type::k_group_replication_member_role_changed,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBER_ROLE_CHANGE},
      {Notice_type::k_group_replication_member_state_changed,
       Protocol_type::GroupReplicationStateChanged_Type_MEMBER_STATE_CHANGE}};

  assert(k_map_types.count(notice_description.m_notice_type) == 1 &&
         Notice_descriptor::is_dispatchable(notice_description.m_notice_type));

  auto binary_notice =
      std::make_shared<Notice_descriptor>(notice_description.m_notice_type);

  ::Mysqlx::Notice::GroupReplicationStateChanged group_replication_state_change;
  group_replication_state_change.set_type(
      k_map_types.at(notice_description.m_notice_type));

  if (!notice_description.m_payload.empty())
    group_replication_state_change.set_view_id(notice_description.m_payload);

  group_replication_state_change.SerializeToString(&binary_notice->m_payload);

  return binary_notice;
}

}  // namespace xpl
