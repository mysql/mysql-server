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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_ENCODER_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_ENCODER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/protocol/encoders/encoding_xrow.h"
#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/vio.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/memory.h"
#include "plugin/x/src/ngs/protocol/message.h"
#include "plugin/x/src/ngs/protocol/page_pool.h"
#include "plugin/x/src/ngs/protocol_fwd.h"

namespace ngs {

class Output_buffer;

class Protocol_encoder : public xpl::iface::Protocol_encoder {
 public:
  Protocol_encoder(const std::shared_ptr<xpl::iface::Vio> &socket,
                   Error_handler ehandler, xpl::iface::Protocol_monitor *pmon,
                   Memory_block_pool *memory_block);

  bool is_building_row() const override { return m_row; }

  xpl::iface::Protocol_flusher *get_flusher() override {
    return m_flusher.get();
  }
  std::unique_ptr<xpl::iface::Protocol_flusher> set_flusher(
      std::unique_ptr<xpl::iface::Protocol_flusher> flusher) override;

  protocol::XMessage_encoder *raw_encoder() override;
  bool send_result(const Error_code &result) override;

  bool send_ok() override;
  bool send_ok(const std::string &message) override;
  bool send_error(const Error_code &error_code,
                  const bool init_error = false) override;

  void send_notice_rows_affected(const uint64_t value) override;
  void send_notice_client_id(const uint64_t id) override;
  void send_notice_last_insert_id(const uint64_t id) override;
  void send_notice_txt_message(const std::string &message) override;
  void send_notice_account_expired() override;
  void send_notice_generated_document_ids(
      const std::vector<std::string> &ids) override;

  bool send_notice(const xpl::iface::Frame_type type,
                   const xpl::iface::Frame_scope scope, const std::string &data,
                   const bool force_flush = false) override;

  void send_auth_ok(const std::string &data) override;
  void send_auth_continue(const std::string &data) override;

  bool send_exec_ok() override;
  bool send_result_fetch_done() override;
  bool send_result_fetch_suspended() override;
  bool send_result_fetch_done_more_results() override;
  bool send_result_fetch_done_more_out_params() override;

  bool send_column_metadata(const Encode_column_info *column_info) override;

  protocol::XRow_encoder *row_builder() override { return &m_row_builder; }
  Metadata_builder *get_metadata_builder() override;

  void start_row() override;
  void abort_row() override;
  // sends the row that was written directly into Encoder's buffer
  bool send_row() override;

  bool send_protobuf_message(const uint8_t type, const Message &message,
                             bool force_buffer_flush = false) override;
  void on_error(int error) override;

  xpl::iface::Protocol_monitor &get_protocol_monitor() override;

  static void log_protobuf(const unsigned id, const char *direction_name,
                           const uint8_t type, const ngs::Message *msg);
  static void log_protobuf(const unsigned id, const char *direction_name,
                           const Message *request);
  static void log_protobuf(const unsigned id, uint8_t type);

 private:
  Protocol_encoder(const Protocol_encoder &) = delete;
  Protocol_encoder &operator=(const Protocol_encoder &) = delete;

  Error_handler m_error_handler;
  xpl::iface::Protocol_monitor *m_protocol_monitor;

  Metadata_builder m_metadata_builder;
  protocol::Encoding_pool m_pool;
  protocol::Encoding_buffer m_xproto_buffer{&m_pool};
  protocol::XMessage_encoder m_xproto_encoder{&m_xproto_buffer};
  protocol::XRow_encoder m_row_builder{&m_xproto_encoder};
  std::unique_ptr<xpl::iface::Protocol_flusher> m_flusher;
  uint32_t m_messages_sent{0};
  uint64_t m_id{0};
  bool m_row{false};

  bool on_message(const uint8_t type);
  bool send_raw_buffer(const uint8_t type);
};  // namespace ngs

#ifdef XPLUGIN_LOG_PROTOBUF
#define log_message_send(CID, MESSAGE) \
  ::ngs::Protocol_encoder::log_protobuf(CID, "SEND", MESSAGE);
#define log_raw_message_send(CID, ID) \
  ::ngs::Protocol_encoder::log_protobuf(CID, ID);
#define log_message_recv(CID, REQUEST)   \
  ::ngs::Protocol_encoder::log_protobuf( \
      CID, "RECV", REQUEST->get_message_type(), REQUEST->get_message());
#else
#define log_message_send(CID, MESSAGE) \
  do {                                 \
  } while (0)
#define log_raw_message_send(CID, ID) \
  do {                                \
  } while (0)
#define log_message_recv(CID, MESSAGE) \
  do {                                 \
  } while (0)
#endif  // XPLUGIN_LOG_PRTOBUF

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_ENCODER_H_
