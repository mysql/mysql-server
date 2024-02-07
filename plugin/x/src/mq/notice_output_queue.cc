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

#include "plugin/x/src/mq/notice_output_queue.h"

#include <string>

#include "mutex_lock.h"  // NOLINT(build/include_subdir)

namespace xpl {

using Notice_description = ngs::Notice_descriptor;

class Notice_output_queue::Idle_reporting : public xpl::iface::Waiting_for_io {
 public:
  explicit Idle_reporting(Notice_output_queue *session_queue)
      : m_session_queue(session_queue) {}

  bool has_to_report_idle_waiting() override {
    if (m_session_queue->m_notice_configuration
            ->is_any_dispatchable_notice_enabled())
      return true;

    return !m_session_queue->m_queue.empty();
  }

  bool on_idle_or_before_read() override {
    DBUG_TRACE;
    const bool force_flush_at_last_notice = true;

    m_session_queue->encode_queued_items(force_flush_at_last_notice);

    return true;
  }

 private:
  Notice_output_queue *m_session_queue;
};

Notice_output_queue::Notice_output_queue(
    iface::Protocol_encoder *encoder,
    iface::Notice_configuration *notice_configuration)
    : m_encoder(encoder),
      m_notice_configuration(notice_configuration),
      m_decoder_io_callbacks(std::make_unique<Idle_reporting>(this)) {}

void Notice_output_queue::emplace(const Buffer_shared &notice) {
  if (!m_notice_configuration->is_notice_enabled(notice->m_notice_type)) return;

  MUTEX_LOCK(locker, m_queue_mutex);
  m_queue.emplace(notice);
}

namespace {
using Notice_type = ngs::Notice_type;
inline iface::Frame_type get_notice_frame_type(const Notice_type notice_type) {
  switch (notice_type) {
    case Notice_type::k_warning:
      return iface::Frame_type::k_warning;
    case Notice_type::k_group_replication_quorum_loss:
    case Notice_type::k_group_replication_view_changed:
    case Notice_type::k_group_replication_member_role_changed:
    case Notice_type::k_group_replication_member_state_changed:
      return iface::Frame_type::k_group_replication_state_changed;
    default: {
      assert(false && "unsupported ngs::Notice_type");
    }
  }
  return iface::Frame_type::k_group_replication_state_changed;
}
}  // namespace

void Notice_output_queue::encode_queued_items(const bool last_force_flush) {
  if (m_queue.empty()) return;

  MUTEX_LOCK(locker, m_queue_mutex);

  while (!m_queue.empty()) {
    const auto &item = m_queue.front();
    const bool flush = last_force_flush && (1 == m_queue.size());

    if (!m_encoder->send_notice(get_notice_frame_type(item->m_notice_type),
                                iface::Frame_scope::k_global, item->m_payload,
                                flush))
      break;

    m_queue.pop();
  }
}

void Notice_output_queue::set_encoder(iface::Protocol_encoder *encoder) {
  m_encoder = encoder;
}

xpl::iface::Waiting_for_io *
Notice_output_queue::get_callbacks_waiting_for_io() {
  return m_decoder_io_callbacks.get();
}

}  // namespace xpl
