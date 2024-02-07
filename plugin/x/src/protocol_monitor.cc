/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/src/protocol_monitor.h"

#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/ngs/session_status_variables.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"

namespace xpl {

void Protocol_monitor::init(iface::Client *client) { m_client = client; }

namespace {

template <ngs::Common_status_variables::Variable ngs::Common_status_variables::*
              variable>
inline void update_status(iface::Session *session) {
  if (session) ++(session->get_status_variables().*variable);
  ++(Global_status_variables::instance().*variable);
}

template <ngs::Session_status_variables::Variable
              ngs::Session_status_variables::*variable>
inline void update_session_status(iface::Session *session) {
  if (session) ++(session->get_status_variables().*variable);
}

template <ngs::Common_status_variables::Variable ngs::Common_status_variables::*
              variable>
inline void update_status(iface::Session *session, const uint32_t value) {
  if (session) (session->get_status_variables().*variable) += value;
  (Global_status_variables::instance().*variable) += value;
}

}  // namespace

void Protocol_monitor::on_notice_warning_send() {
  update_status<&ngs::Common_status_variables::m_notice_warning_sent>(
      m_client->session());
}

void Protocol_monitor::on_notice_other_send() {
  update_status<&ngs::Common_status_variables::m_notice_other_sent>(
      m_client->session());
}

void Protocol_monitor::on_notice_global_send() {
  update_status<&ngs::Common_status_variables::m_notice_global_sent>(
      m_client->session());
}

void Protocol_monitor::on_error_send() {
  update_status<&ngs::Common_status_variables::m_errors_sent>(
      m_client->session());
}

void Protocol_monitor::on_fatal_error_send() {
  update_session_status<&ngs::Session_status_variables::m_fatal_errors_sent>(
      m_client->session());
  ++Global_status_variables::instance().m_sessions_fatal_errors_count;
}

void Protocol_monitor::on_init_error_send() {
  ++Global_status_variables::instance().m_init_errors_count;
}

void Protocol_monitor::on_row_send() {
  update_status<&ngs::Common_status_variables::m_rows_sent>(
      m_client->session());
}

void Protocol_monitor::on_send(const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_sent>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_send_compressed(const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_sent_compressed_payload>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_send_before_compression(
    const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_sent_uncompressed_frame>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_receive(const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_received>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_receive_compressed(const uint32_t bytes_transferred) {
  update_status<
      &ngs::Common_status_variables::m_bytes_received_compressed_payload>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_receive_after_decompression(
    const uint32_t bytes_transferred) {
  update_status<
      &ngs::Common_status_variables::m_bytes_received_uncompressed_frame>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_error_unknown_msg_type() {
  update_status<&ngs::Common_status_variables::m_errors_unknown_message_type>(
      m_client->session());
}

void Protocol_monitor::on_messages_sent(const uint32_t messages) {
  update_status<&ngs::Common_status_variables::m_messages_sent>(
      m_client->session(), messages);
}

}  // namespace xpl
