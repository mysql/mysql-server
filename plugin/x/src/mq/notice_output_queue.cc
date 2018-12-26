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

#include "plugin/x/src/mq/notice_output_queue.h"

#include "mutex_lock.h"

namespace xpl {

using Notice_description = ngs::Notice_descriptor;

class Notice_output_queue::Idle_reporting
    : public ngs::Protocol_decoder::Waiting_for_io_interface {
 public:
  Idle_reporting(Notice_output_queue *session_queue)
      : m_session_queue(session_queue) {}

  bool has_to_report_idle_waiting() override {
    if (m_session_queue->m_notice_configuration
            ->is_any_dispatchable_notice_enabled())
      return true;

    return !m_session_queue->m_queue.empty();
  }

  void on_idle_or_before_read() override {
    const bool force_flush_at_last_notice = true;

    m_session_queue->encode_queued_items(force_flush_at_last_notice);
  }

 private:
  Notice_output_queue *m_session_queue;
};

void Notice_output_queue::emplace(const ngs::Notice_type type,
                                  const Buffer_shared &binary_notice) {
  if (!m_notice_configuration->is_notice_enabled(type)) return;

  MUTEX_LOCK(locker, m_queue_mutex);
  m_queue.emplace(binary_notice);
}

void Notice_output_queue::encode_queued_items(const bool last_force_flush) {
  if (m_queue.empty()) return;

  MUTEX_LOCK(locker, m_queue_mutex);

  while (!m_queue.empty()) {
    auto &item = m_queue.front();
    const bool flush = last_force_flush && (1 == m_queue.size());

    if (!m_encoder->send_notice(
            ::ngs::Frame_type::k_group_replication_state_changed,
            ::ngs::Frame_scope::k_global, *item, flush))
      break;

    m_queue.pop();
  }
}

ngs::Protocol_decoder::Waiting_for_io_interface &
Notice_output_queue::get_callbacks_waiting_for_io() {
  if (!m_decoder_io_callbacks)
    m_decoder_io_callbacks.reset(new Idle_reporting(this));

  return *m_decoder_io_callbacks;
}

}  // namespace xpl
