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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_PROTOCOL_ENCODER_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_PROTOCOL_ENCODER_INTERFACE_H_

#include <string>

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/protocol/message.h"
#include "plugin/x/ngs/include/ngs/protocol/output_buffer.h"
#include "plugin/x/ngs/include/ngs/protocol/row_builder.h"

namespace ngs {

typedef uint32_t Session_id;
typedef uint32_t Cursor_id;
typedef uint32_t Prepared_stmt_id;

class Protocol_monitor_interface;

enum class Frame_scope {
  k_local = Mysqlx::Notice::Frame_Scope_LOCAL,
  k_global = Mysqlx::Notice::Frame_Scope_GLOBAL
};

enum class Frame_type {
  k_warning = Mysqlx::Notice::Frame_Type_WARNING,
  k_session_variable_changed =
      Mysqlx::Notice::Frame_Type_SESSION_VARIABLE_CHANGED,
  k_session_state_changed = Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED,
  k_group_replication_state_changed =
      Mysqlx::Notice::Frame_Type_GROUP_REPLICATION_STATE_CHANGED
};

struct Encode_column_info {
  const char *m_catalog = "";
  const char *m_db_name = "";
  const char *m_table_name = "";
  const char *m_org_table_name = "";
  const char *m_col_name = "";
  const char *m_org_col_name = "";

  uint64_t m_collation{0};
  bool m_has_collation{false};

  int32_t m_type{0};

  int32_t m_decimals{0};
  int32_t m_has_decimals{false};

  uint32_t m_flags{0};
  bool m_has_flags{false};

  uint32_t m_length{0};
  bool m_has_length{false};

  uint32_t m_content_type{0};

  bool m_compact{true};
};

class Protocol_encoder_interface {
 public:
  virtual ~Protocol_encoder_interface() = default;

  virtual bool send_result(const Error_code &result) = 0;

  virtual bool send_ok() = 0;
  virtual bool send_ok(const std::string &message) = 0;
  virtual bool send_init_error(const Error_code &error_code) = 0;

  virtual void send_rows_affected(uint64_t value) = 0;

  virtual bool send_notice(const Frame_type type, const Frame_scope scope,
                           const std::string &data,
                           const bool force_flush = false) = 0;

  virtual void send_auth_ok(const std::string &data) = 0;
  virtual void send_auth_continue(const std::string &data) = 0;

  virtual bool send_exec_ok() = 0;
  virtual bool send_result_fetch_done() = 0;
  virtual bool send_result_fetch_done_more_results() = 0;
  virtual bool send_column_metadata(const Encode_column_info *column_info) = 0;

  virtual Row_builder &row_builder() = 0;
  virtual void start_row() = 0;
  virtual void abort_row() = 0;
  // sends the row that was written directly into Encoder's buffer
  virtual bool send_row() = 0;

  virtual Output_buffer *get_buffer() = 0;

  virtual bool send_message(int8_t type, const Message &message,
                            bool force_buffer_flush = false) = 0;
  virtual void on_error(int error) = 0;

  virtual Protocol_monitor_interface &get_protocol_monitor() = 0;

  virtual void set_write_timeout(const uint32_t timeout) = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_PROTOCOL_ENCODER_INTERFACE_H_
