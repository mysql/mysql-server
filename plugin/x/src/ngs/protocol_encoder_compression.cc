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

#include <errno.h>
#include <sys/types.h>
#include <utility>

#include "my_io.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/ngs/protocol_encoder_compression.h"
#include "plugin/x/src/ngs/protocol_flusher_compression.h"

namespace ngs {

Protocol_encoder_compression::Protocol_encoder_compression(
    Protocol_encoder_ptr encoder, xpl::iface::Protocol_monitor *monitor,
    Error_handler ehandler, Memory_block_pool *memory_block) {
  m_encoder.reset(encoder.release());

  auto old_flusher = m_encoder->set_flusher(nullptr);
  std::unique_ptr<xpl::iface::Protocol_flusher> new_flusher(
      new Protocol_flusher_compression(std::move(old_flusher), raw_encoder(),
                                       monitor, ehandler, memory_block));

  m_encoder->set_flusher(std::move(new_flusher));
}

xpl::iface::Protocol_flusher *Protocol_encoder_compression::get_flusher() {
  return m_encoder->get_flusher();
}

std::unique_ptr<xpl::iface::Protocol_flusher>
Protocol_encoder_compression::set_flusher(
    std::unique_ptr<xpl::iface::Protocol_flusher> flusher) {
  return m_encoder->set_flusher(std::move(flusher));
}

Metadata_builder *Protocol_encoder_compression::get_metadata_builder() {
  return m_encoder->get_metadata_builder();
}

bool Protocol_encoder_compression::send_result(const Error_code &result) {
  handle_compression(result.error == 0 ? protocol::tags::Ok::server_id
                                       : protocol::tags::Error::server_id,
                     false);
  return m_encoder->send_result(result);
}

bool Protocol_encoder_compression::send_ok() {
  handle_compression(protocol::tags::Ok::server_id, false);

  return m_encoder->send_ok();
}

bool Protocol_encoder_compression::send_ok(const std::string &message) {
  handle_compression(protocol::tags::Ok::server_id, false);

  return m_encoder->send_ok(message);
}

bool Protocol_encoder_compression::send_error(const Error_code &error_code,
                                              const bool init_error) {
  handle_compression(protocol::tags::Error::server_id, false);
  return m_encoder->send_error(error_code, init_error);
}

void Protocol_encoder_compression::send_notice_rows_affected(
    const uint64_t value) {
  handle_compression(protocol::tags::Frame::server_id, true);
  m_encoder->send_notice_rows_affected(value);
}

void Protocol_encoder_compression::send_notice_client_id(const uint64_t id) {
  handle_compression(protocol::tags::Frame::server_id, false);
  m_encoder->send_notice_client_id(id);
}

void Protocol_encoder_compression::send_notice_last_insert_id(
    const uint64_t id) {
  handle_compression(protocol::tags::Frame::server_id, true);
  m_encoder->send_notice_last_insert_id(id);
}

void Protocol_encoder_compression::send_notice_txt_message(
    const std::string &message) {
  handle_compression(protocol::tags::Frame::server_id, true);
  m_encoder->send_notice_txt_message(message);
}

void Protocol_encoder_compression::send_notice_account_expired() {
  handle_compression(protocol::tags::Frame::server_id, false);
  m_encoder->send_notice_account_expired();
}

void Protocol_encoder_compression::send_notice_generated_document_ids(
    const std::vector<std::string> &ids) {
  if (ids.empty()) return;
  handle_compression(protocol::tags::Frame::server_id, true);
  m_encoder->send_notice_generated_document_ids(ids);
}

bool Protocol_encoder_compression::send_notice(
    const xpl::iface::Frame_type type, const xpl::iface::Frame_scope scope,
    const std::string &data, const bool force_flush) {
  log_debug("Protocol_encoder_compression::send_notice");
  handle_compression(protocol::tags::Frame::server_id,
                     scope == xpl::iface::Frame_scope::k_local);
  return m_encoder->send_notice(type, scope, data, force_flush);
}

void Protocol_encoder_compression::send_auth_ok(const std::string &data) {
  handle_compression(protocol::tags::AuthenticateOk::server_id, false);
  m_encoder->send_auth_ok(data);
}

void Protocol_encoder_compression::send_auth_continue(const std::string &data) {
  handle_compression(protocol::tags::AuthenticateContinue::server_id, false);
  m_encoder->send_auth_continue(data);
}

bool Protocol_encoder_compression::send_exec_ok() {
  handle_compression(protocol::tags::StmtExecuteOk::server_id, false);
  return m_encoder->send_exec_ok();
}

bool Protocol_encoder_compression::send_result_fetch_done() {
  handle_compression(protocol::tags::FetchDone::server_id, true);
  return m_encoder->send_result_fetch_done();
}

bool Protocol_encoder_compression::send_result_fetch_suspended() {
  handle_compression(protocol::tags::FetchSuspended::server_id, true);
  return m_encoder->send_result_fetch_suspended();
}

bool Protocol_encoder_compression::send_result_fetch_done_more_results() {
  handle_compression(protocol::tags::FetchDoneMoreResultsets::server_id, true);
  return m_encoder->send_result_fetch_done_more_results();
}

bool Protocol_encoder_compression::send_result_fetch_done_more_out_params() {
  handle_compression(protocol::tags::FetchDoneMoreOutParams::server_id, true);
  return m_encoder->send_result_fetch_done_more_out_params();
}

bool Protocol_encoder_compression::send_column_metadata(
    const Encode_column_info *column_info) {
  handle_compression(protocol::tags::ColumnMetaData::server_id, true);
  return m_encoder->send_column_metadata(column_info);
}

bool Protocol_encoder_compression::is_building_row() const {
  return m_encoder->is_building_row();
}

protocol::XMessage_encoder *Protocol_encoder_compression::raw_encoder() {
  return m_encoder->raw_encoder();
}

protocol::XRow_encoder *Protocol_encoder_compression::row_builder() {
  return m_encoder->row_builder();
}

void Protocol_encoder_compression::start_row() {
  handle_compression(protocol::tags::Row::server_id, true);
  m_encoder->start_row();
}

void Protocol_encoder_compression::abort_row() {
  m_encoder->abort_row();
  get_comp_flusher()->abort_last_compressed();
}

bool Protocol_encoder_compression::send_row() { return m_encoder->send_row(); }

bool Protocol_encoder_compression::send_protobuf_message(
    const uint8_t type, const Message &message, bool force_buffer_flush) {
  handle_compression(type, false);
  return m_encoder->send_protobuf_message(type, message, force_buffer_flush);
}

void Protocol_encoder_compression::on_error(int error) {
  m_encoder->on_error(error);
}

xpl::iface::Protocol_monitor &
Protocol_encoder_compression::get_protocol_monitor() {
  return m_encoder->get_protocol_monitor();
}

void Protocol_encoder_compression::set_compression_options(
    const Compression_algorithm algo, const Compression_style style,
    const int64_t max_msg, const int32_t level) {
  get_comp_flusher()->set_compression_options(algo, style, max_msg, level);
}

Protocol_flusher_compression *Protocol_encoder_compression::get_comp_flusher() {
  auto flusher = get_flusher();

  return reinterpret_cast<Protocol_flusher_compression *>(flusher);
}
void Protocol_encoder_compression::handle_compression(
    const uint8_t id, const bool can_be_compressed) {
  get_comp_flusher()->handle_compression(id, can_be_compressed);
}

}  // namespace ngs
