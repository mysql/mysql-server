/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_CURSOR_COMMAND_DELEGATE_H_
#define PLUGIN_X_SRC_CURSOR_COMMAND_DELEGATE_H_

#include <sys/types.h>

#include "plugin/x/ngs/include/ngs/interface/notice_output_queue_interface.h"
#include "plugin/x/src/streaming_command_delegate.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

class Cursor_command_delegate : public Streaming_command_delegate {
 public:
  Cursor_command_delegate(ngs::Protocol_encoder_interface *proto,
                          ngs::Notice_output_queue_interface *notice_queue,
                          const bool ignore_fetch_suspended_at_cursor_open)
      : Streaming_command_delegate(proto, notice_queue),
        m_ignore_fetch_suspended(ignore_fetch_suspended_at_cursor_open) {}

  void reset() override {}

  void handle_ok(uint server_status, uint statement_warn_count,
                 ulonglong affected_rows, ulonglong last_insert_id,
                 const char *const message) override {
    log_debug(
        "Cursor_command_delegate::handle_ok %i, warnings: %i, "
        "affected_rows:%i, last_insert_id: %i, msg: %s",
        (int)server_status, (int)statement_warn_count, (int)affected_rows,
        (int)last_insert_id, message);

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

    Streaming_command_delegate::handle_ok(server_status, statement_warn_count,
                                          affected_rows, last_insert_id,
                                          message);
  }

 private:
  bool m_ignore_fetch_suspended = false;
};

}  // namespace xpl

#endif  //  PLUGIN_X_SRC_CURSOR_COMMAND_DELEGATE_H_
