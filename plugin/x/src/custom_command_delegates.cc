/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/src/custom_command_delegates.h"

#include <cinttypes>

#include "plugin/x/src/xpl_log.h"

namespace xpl {

Cursor_command_delegate::Cursor_command_delegate(
    iface::Session *session, const bool ignore_fetch_suspended_at_cursor_open)
    : Streaming_command_delegate(session),
      m_ignore_fetch_suspended(ignore_fetch_suspended_at_cursor_open) {}

int Cursor_command_delegate::end_result_metadata(uint32_t server_status,
                                                 uint32_t warn_count) {
  end_result_metadata_handle_fetch(server_status);
  return Streaming_command_delegate::end_result_metadata(server_status,
                                                         warn_count);
}

void Cursor_command_delegate::handle_ok(uint32_t server_status,
                                        uint32_t statement_warn_count,
                                        uint64_t affected_rows,
                                        uint64_t last_insert_id,
                                        const char *const message) {
  log_debug("Cursor_command_delegate::handle_ok %" PRIu32 ", warnings: %" PRIu32
            ", affected_rows: %" PRIu64 ", last_insert_id: %" PRIu64
            ", msg: %s",
            server_status, statement_warn_count, affected_rows, last_insert_id,
            message);

  m_got_eof = !(server_status & SERVER_STATUS_CURSOR_EXISTS) ||
              (server_status & SERVER_STATUS_LAST_ROW_SENT);

  if (server_status & SERVER_STATUS_CURSOR_EXISTS) {
    if (!m_ignore_fetch_suspended) m_proto->send_result_fetch_suspended();

    // Ignore first fetch suspended !
    // First time it can be called for Cursor.Open
    m_ignore_fetch_suspended = false;

    // Calling 'handle_ok' directly, makes Command_delegate to remember
    // the arguments.
    Command_delegate::handle_ok(server_status, statement_warn_count,
                                affected_rows, last_insert_id, message);

    return;
  }

  handle_fetch_done_more_results(server_status);
  m_handle_ok_received = false;

  if (m_sent_result) {
    if (server_status & SERVER_MORE_RESULTS_EXISTS) {
      if (!(server_status & SERVER_PS_OUT_PARAMS)) m_handle_ok_received = true;
    } else {
      m_proto->send_result_fetch_done();
    }
  }
  Command_delegate::handle_ok(server_status, statement_warn_count,
                              affected_rows, last_insert_id, message);
}

Crud_command_delegate::Crud_command_delegate(iface::Session *session)
    : Streaming_command_delegate(session) {}

Crud_command_delegate::~Crud_command_delegate() { on_destruction(); }

bool Crud_command_delegate::try_send_notices(
    const uint32_t server_status, const uint32_t statement_warn_count,
    const uint64_t affected_rows, const uint64_t last_insert_id,
    const char *const message) {
  if (defer_on_warning(server_status, statement_warn_count, affected_rows,
                       last_insert_id, message))
    return false;

  if (message && strlen(message) != 0)
    m_proto->send_notice_txt_message(message);

  return true;
}

Stmt_command_delegate::Stmt_command_delegate(iface::Session *session)
    : Streaming_command_delegate(session) {}

Stmt_command_delegate::~Stmt_command_delegate() { on_destruction(); }

bool Stmt_command_delegate::try_send_notices(
    const uint32_t server_status, const uint32_t statement_warn_count,
    const uint64_t affected_rows, const uint64_t last_insert_id,
    const char *const message) {
  if (defer_on_warning(server_status, statement_warn_count, affected_rows,
                       last_insert_id, message))
    return false;

  m_proto->send_notice_rows_affected(affected_rows);

  if (last_insert_id > 0) m_proto->send_notice_last_insert_id(last_insert_id);

  if (message && strlen(message) != 0)
    m_proto->send_notice_txt_message(message);

  return true;
}

void Stmt_command_delegate::handle_ok(uint32_t server_status,
                                      uint32_t statement_warn_count,
                                      uint64_t affected_rows,
                                      uint64_t last_insert_id,
                                      const char *const message) {
  handle_out_param_in_handle_ok(server_status);
  Streaming_command_delegate::handle_ok(server_status, statement_warn_count,
                                        affected_rows, last_insert_id, message);
}

int Stmt_command_delegate::end_result_metadata(uint32_t server_status,
                                               uint32_t warn_count) {
  handle_fetch_done_more_results(server_status);
  return Streaming_command_delegate::end_result_metadata(server_status,
                                                         warn_count);
}

Prepare_command_delegate::Prepare_command_delegate(iface::Session *session)
    : Streaming_command_delegate(session) {}

Prepare_command_delegate::~Prepare_command_delegate() { on_destruction(); }

bool Prepare_command_delegate::try_send_notices(
    const uint32_t server_status, const uint32_t statement_warn_count,
    const uint64_t affected_rows, const uint64_t last_insert_id,
    const char *const message) {
  if (defer_on_warning(server_status, statement_warn_count, affected_rows,
                       last_insert_id, message))
    return false;

  if (message && strlen(message) != 0)
    m_proto->send_notice_txt_message(message);

  if (m_notice_level.test(Notice_level_flags::k_send_affected_rows))
    m_proto->send_notice_rows_affected(affected_rows);

  if (m_notice_level.test(Notice_level_flags::k_send_generated_insert_id))
    if (last_insert_id > 0) m_proto->send_notice_last_insert_id(last_insert_id);

  if (m_notice_level.test(Notice_level_flags::k_send_generated_document_ids))
    m_proto->send_notice_generated_document_ids(
        m_session->get_document_id_aggregator().get_ids());

  return true;
}

int Prepare_command_delegate::end_result_metadata(uint32_t server_status,
                                                  uint32_t warn_count) {
  end_result_metadata_handle_fetch(server_status);
  return Streaming_command_delegate::end_result_metadata(server_status,
                                                         warn_count);
}

void Prepare_command_delegate::handle_ok(uint32_t server_status,
                                         uint32_t statement_warn_count,
                                         uint64_t affected_rows,
                                         uint64_t last_insert_id,
                                         const char *const message) {
  handle_out_param_in_handle_ok(server_status);
  Streaming_command_delegate::handle_ok(server_status, statement_warn_count,
                                        affected_rows, last_insert_id, message);
}

}  // namespace xpl
