/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef NGS_PROTOCOL_ENCODER_INTERFACE_H_
#define NGS_PROTOCOL_ENCODER_INTERFACE_H_

#include <string>

#include "ngs/error_code.h"
#include "ngs/protocol/row_builder.h"
#include "ngs/protocol/output_buffer.h"
#include "ngs/protocol/message.h"

namespace ngs {

typedef uint32_t Session_id;
typedef uint32_t Cursor_id;
typedef uint32_t Prepared_stmt_id;

class Protocol_monitor_interface;

enum class Frame_scope {
  LOCAL = Mysqlx::Notice::Frame_Scope_LOCAL,
  GLOBAL = Mysqlx::Notice::Frame_Scope_GLOBAL
};

enum class Frame_type {
  WARNING = Mysqlx::Notice::Frame_Type_WARNING,
  SESSION_VARIABLE_CHANGED = Mysqlx::Notice::Frame_Type_SESSION_VARIABLE_CHANGED,
  SESSION_STATE_CHANGED = Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED
};

class Protocol_encoder_interface {
 public:
  virtual ~Protocol_encoder_interface() = default;

  virtual bool send_result(const Error_code &result) = 0;

  virtual bool send_ok() = 0;
  virtual bool send_ok(const std::string &message) = 0;
  virtual bool send_init_error(const Error_code &error_code) = 0;

  virtual void send_rows_affected(uint64_t value) = 0;

  virtual void send_notice(const Frame_type type, const Frame_scope scope,
                           const std::string &data,
                           const bool force_flush = false) = 0;

  virtual void send_auth_ok(const std::string &data) = 0;
  virtual void send_auth_continue(const std::string &data) = 0;

  virtual bool send_exec_ok() = 0;
  virtual bool send_result_fetch_done() = 0;
  virtual bool send_result_fetch_done_more_results() = 0;

  virtual bool send_column_metadata(
      const std::string &catalog, const std::string &db_name,
      const std::string &table_name, const std::string &org_table_name,
      const std::string &col_name, const std::string &org_col_name,
      uint64_t collation, int type, int decimals, uint32_t flags,
      uint32_t length, uint32_t content_type = 0) = 0;

  virtual bool send_column_metadata(uint64_t collation, int type, int decimals,
                                    uint32_t flags, uint32_t length,
                                    uint32_t content_type = 0) = 0;

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
};

}  // namespace ngs

#endif  // NGS_PROTOCOL_ENCODER_INTERFACE_H_
