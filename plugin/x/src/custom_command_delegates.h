/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_CURSOR_COMMAND_DELEGATE_H_
#define PLUGIN_X_SRC_CURSOR_COMMAND_DELEGATE_H_

#include <sys/types.h>
#include <bitset>

#include "plugin/x/src/streaming_command_delegate.h"

namespace xpl {

class Cursor_command_delegate : public Streaming_command_delegate {
 public:
  Cursor_command_delegate(iface::Session *session,
                          const bool ignore_fetch_suspended_at_cursor_open);

  void reset() override {}

  int end_result_metadata(uint32_t server_status, uint32_t warn_count) override;

  void handle_ok(uint32_t server_status, uint32_t statement_warn_count,
                 uint64_t affected_rows, uint64_t last_insert_id,
                 const char *const message) override;

 private:
  bool m_ignore_fetch_suspended = false;
};

class Crud_command_delegate : public Streaming_command_delegate {
 public:
  Crud_command_delegate(iface::Session *session);
  ~Crud_command_delegate() override;

  bool try_send_notices(const uint32_t server_status,
                        const uint32_t statement_warn_count,
                        const uint64_t affected_rows,
                        const uint64_t last_insert_id,
                        const char *const message) override;
};

class Stmt_command_delegate : public Streaming_command_delegate {
 public:
  Stmt_command_delegate(iface::Session *session);
  ~Stmt_command_delegate() override;

  bool try_send_notices(const uint32_t server_status,
                        const uint32_t statement_warn_count,
                        const uint64_t affected_rows,
                        const uint64_t last_insert_id,
                        const char *const message) override;

  void handle_ok(uint32_t server_status, uint32_t statement_warn_count,
                 uint64_t affected_rows, uint64_t last_insert_id,
                 const char *const message) override;

  int end_result_metadata(uint32_t server_status, uint32_t warn_count) override;
};

class Prepare_command_delegate : public Streaming_command_delegate {
 public:
  enum Notice_level_flags {
    k_send_affected_rows,
    k_send_generated_insert_id,
    k_send_generated_document_ids,
    k_notice_level_flags_size
  };
  using Notice_level =
      std::bitset<Notice_level_flags::k_notice_level_flags_size>;

  Prepare_command_delegate(iface::Session *session);
  ~Prepare_command_delegate() override;

  bool try_send_notices(const uint32_t server_status,
                        const uint32_t statement_warn_count,
                        const uint64_t affected_rows,
                        const uint64_t last_insert_id,
                        const char *const message) override;

  int end_result_metadata(uint32_t server_status, uint32_t warn_count) override;

  void handle_ok(uint32_t server_status, uint32_t statement_warn_count,
                 uint64_t affected_rows, uint64_t last_insert_id,
                 const char *const message) override;

  void set_notice_level(Notice_level level) { m_notice_level = level; }

 private:
  Notice_level m_notice_level;
};

}  // namespace xpl

#endif  //  PLUGIN_X_SRC_CURSOR_COMMAND_DELEGATE_H_
