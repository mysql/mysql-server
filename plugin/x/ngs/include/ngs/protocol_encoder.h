/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_ENCODER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_ENCODER_H_

#include <map>
#include <memory>
#include <vector>
#include "my_inttypes.h"

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/protocol/message.h"
#include "plugin/x/ngs/include/ngs/protocol/message_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/metadata_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/notice_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/page_pool.h"
#include "plugin/x/ngs/include/ngs/protocol/row_builder.h"
#include "plugin/x/ngs/include/ngs/protocol_fwd.h"
#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/xpl_system_variables.h"
#include "protocol/page_output_stream.h"

namespace ngs {

class Output_buffer;

class Protocol_encoder : public Protocol_encoder_interface {
 public:
  Protocol_encoder(const std::shared_ptr<Vio_interface> &socket,
                   Error_handler ehandler, Protocol_monitor_interface *pmon);

  Page_output_stream *get_buffer() override { return &m_page_output_stream; }
  Protocol_flusher *get_flusher() override { return &m_flusher; }
  Metadata_builder *get_metadata_builder() override {
    return &m_metadata_builder;
  }

  bool send_result(const Error_code &result) override;

  bool send_ok() override;
  bool send_ok(const std::string &message) override;
  bool send_init_error(const Error_code &error_code) override;

  void send_rows_affected(uint64_t value) override;

  bool send_notice(const Frame_type type, const Frame_scope scope,
                   const std::string &data,
                   const bool force_flush = false) override;

  void send_auth_ok(const std::string &data) override;
  void send_auth_continue(const std::string &data) override;

  bool send_exec_ok() override;
  bool send_result_fetch_done() override;
  bool send_result_fetch_suspended() override;
  bool send_result_fetch_done_more_results() override;
  bool send_result_fetch_done_more_out_params() override;

  bool send_column_metadata(const Encode_column_info *column_info) override;

  Row_builder &row_builder() override { return m_row_builder; }
  void start_row() override;
  void abort_row() override;
  // sends the row that was written directly into Encoder's buffer
  bool send_row() override;

  virtual bool send_message(uint8_t type, const Message &message,
                            bool force_buffer_flush = false) override;
  virtual void on_error(int error) override;

  virtual Protocol_monitor_interface &get_protocol_monitor() override;

  static void log_protobuf(const char *direction_name, const uint8 type,
                           const Message *msg);
  static void log_protobuf(const char *direction_name, const Message *request);
  static void log_protobuf(uint8_t type);

 private:
  Protocol_encoder(const Protocol_encoder &) = delete;
  Protocol_encoder &operator=(const Protocol_encoder &) = delete;

  virtual bool send_empty_message(const uint8_t message_id);

  // Temporary solution for all io
  static const Pool_config m_default_pool_config;
  Page_pool m_pool;
  Error_handler m_error_handler;
  Protocol_monitor_interface *m_protocol_monitor;

  Page_output_stream m_page_output_stream;
  Protocol_flusher m_flusher;

  Row_builder m_row_builder;
  Metadata_builder m_metadata_builder;
  Message_builder m_empty_msg_builder;
  Notice_builder m_notice_builder;

  bool on_message(const uint8_t type);
  bool send_raw_buffer(const uint8_t type);
};

#ifdef XPLUGIN_LOG_PROTOBUF
#define log_message_send(MESSAGE) \
  ::ngs::Protocol_encoder::log_protobuf("SEND", MESSAGE);
#define log_raw_message_send(ID) ::ngs::Protocol_encoder::log_protobuf(ID);
#define log_message_recv(REQUEST)                                           \
  ::ngs::Protocol_encoder::log_protobuf("RECV", REQUEST.get_message_type(), \
                                        REQUEST.get_message());
#else
#define log_message_send(MESSAGE) \
  do {                            \
  } while (0)
#define log_raw_message_send(ID) \
  do {                           \
  } while (0)
#define log_message_recv(MESSAGE) \
  do {                            \
  } while (0)
#endif  // XPLUGIN_LOG_PRTOBUF

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_ENCODER_H_
