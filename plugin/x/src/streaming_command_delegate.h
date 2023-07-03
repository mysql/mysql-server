/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_STREAMING_COMMAND_DELEGATE_H_
#define PLUGIN_X_SRC_STREAMING_COMMAND_DELEGATE_H_

#include <sys/types.h>

#include <cstdint>
#include <vector>

#include "plugin/x/src/interface/notice_output_queue.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/ngs/command_delegate.h"
#include "plugin/x/src/ngs/protocol/message.h"
#include "plugin/x/src/ngs/protocol/metadata_builder.h"
#include "plugin/x/src/notices.h"

namespace xpl {

class Streaming_command_delegate : public ngs::Command_delegate {
 public:
  explicit Streaming_command_delegate(iface::Session *session);
  Streaming_command_delegate(const Streaming_command_delegate &) = default;
  ~Streaming_command_delegate() override;

  void set_compact_metadata(bool flag) { m_compact_metadata = flag; }
  bool compact_metadata() const { return m_compact_metadata; }

  void reset() override;

 protected:
  int start_result_metadata(uint32_t num_cols, uint32_t flags,
                            const CHARSET_INFO *resultcs) override;
  int field_metadata(struct st_send_field *field,
                     const CHARSET_INFO *charset) override;
  int end_result_metadata(uint32_t server_status, uint32_t warn_count) override;

  int start_row() override;
  int end_row() override;
  void abort_row() override;
  ulong get_client_capabilities() override;
  int get_null() override;
  int get_integer(longlong value) override;
  int get_longlong(longlong value, uint32_t unsigned_flag) override;
  int get_decimal(const decimal_t *value) override;
  int get_double(double value, uint32_t decimals) override;
  int get_date(const MYSQL_TIME *value) override;
  int get_time(const MYSQL_TIME *value, uint32_t decimals) override;
  int get_datetime(const MYSQL_TIME *value, uint32_t decimals) override;
  int get_string(const char *const value, size_t length,
                 const CHARSET_INFO *const valuecs) override;
  void handle_ok(uint32_t server_status, uint32_t statement_warn_count,
                 uint64_t affected_rows, uint64_t last_insert_id,
                 const char *const message) override;
  virtual bool try_send_notices(const uint32_t server_status,
                                const uint32_t statement_warn_count,
                                const uint64_t affected_rows,
                                const uint64_t last_insert_id,
                                const char *const message);

  void handle_error(uint32_t sql_errno, const char *const err_msg,
                    const char *const sqlstate) override;
  bool connection_alive() override;

  enum cs_text_or_binary representation() const override {
    return CS_BINARY_REPRESENTATION;
  }

  void handle_fetch_done_more_results(uint32_t server_status);
  void end_result_metadata_handle_fetch(uint32_t server_status);
  void handle_out_param_in_handle_ok(uint32_t server_status);
  void on_destruction();
  bool defer_on_warning(const uint32_t server_status,
                        const uint32_t statement_warn_count,
                        const uint64_t affected_rows,
                        const uint64_t last_insert_id,
                        const char *const message);

  iface::Protocol_encoder *m_proto;
  ngs::Metadata_vector &m_metadata;
  const CHARSET_INFO *m_resultcs = nullptr;
  iface::Notice_output_queue *m_notice_queue = nullptr;
  bool m_sent_result = false;
  bool m_wait_for_fetch_done = false;
  bool m_compact_metadata = false;
  bool m_handle_ok_received = false;
  bool m_send_notice_deferred = false;
  int m_filled_column_counter = 0;
  iface::Session *m_session;
};

}  // namespace xpl

#endif  //  PLUGIN_X_SRC_STREAMING_COMMAND_DELEGATE_H_
