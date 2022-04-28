/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_ENCODER_COMPRESSION_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_ENCODER_COMPRESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)

#include "plugin/x/protocol/encoders/encoding_xrow.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/ngs/compression_types.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/memory.h"

namespace ngs {

class Protocol_flusher_compression;

class Protocol_encoder_compression : public xpl::iface::Protocol_encoder {
 public:
  using Protocol_encoder_ptr =
      ngs::Memory_instrumented<xpl::iface::Protocol_encoder>::Unique_ptr;

  Protocol_encoder_compression(Protocol_encoder_ptr protocol_encoder,
                               xpl::iface::Protocol_monitor *monitor,
                               Error_handler ehandler,
                               Memory_block_pool *memory_block_pool);

 public:  // Protocol_encoder_interface
  xpl::iface::Protocol_flusher *get_flusher() override;
  bool is_building_row() const override;
  std::unique_ptr<xpl::iface::Protocol_flusher> set_flusher(
      std::unique_ptr<xpl::iface::Protocol_flusher> flusher) override;
  Metadata_builder *get_metadata_builder() override;

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

  protocol::XMessage_encoder *raw_encoder() override;
  protocol::XRow_encoder *row_builder() override;
  void start_row() override;
  void abort_row() override;
  bool send_row() override;

  bool send_protobuf_message(const uint8_t type, const Message &message,
                             bool force_buffer_flush = false) override;
  void on_error(int error) override;

  xpl::iface::Protocol_monitor &get_protocol_monitor() override;

 public:
  void set_compression_options(const Compression_algorithm algo,
                               const Compression_style style,
                               const int64_t max_msg, const int32_t level);

 private:
  Protocol_flusher_compression *get_comp_flusher();
  void handle_compression(const uint8_t id, const bool can_be_compressed);

  ngs::Memory_instrumented<xpl::iface::Protocol_encoder>::Unique_ptr m_encoder;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_ENCODER_COMPRESSION_H_
