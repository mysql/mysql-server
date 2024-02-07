/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

#include <errno.h>
#include <sys/types.h>

#include "my_dbug.h"     // NOLINT(build/include_subdir)
#include "my_io.h"       // NOLINT(build/include_subdir)
#include "my_systime.h"  // my_sleep NOLINT(build/include_subdir)

#include "plugin/x/src/interface/vio.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/ngs/protocol/protocol_config.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/ngs/protocol_encoder.h"

#undef ERROR  // Needed to avoid conflict with ERROR in mysqlx.pb.h

namespace ngs {

// Lets make the usage of protobuf easier
using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;

// Alias for return types
using Flush_result = xpl::iface::Protocol_flusher::Result;

Protocol_encoder::Protocol_encoder(
    const std::shared_ptr<xpl::iface::Vio> &socket, Error_handler ehandler,
    xpl::iface::Protocol_monitor *pmon, Memory_block_pool *memory_block_pool)
    : m_error_handler(ehandler),
      m_protocol_monitor(pmon),
      m_pool{10, memory_block_pool},  // TODO(lkotula): benchmark first argument
                                      // (shouldn't be in review)
      m_flusher(new Protocol_flusher(&m_xproto_buffer, &m_xproto_encoder, pmon,
                                     socket, ehandler)) {}

Metadata_builder *Protocol_encoder::get_metadata_builder() {
  return &m_metadata_builder;
}

protocol::XMessage_encoder *Protocol_encoder::raw_encoder() {
  return &m_xproto_encoder;
}

std::unique_ptr<xpl::iface::Protocol_flusher> Protocol_encoder::set_flusher(
    std::unique_ptr<xpl::iface::Protocol_flusher> flusher) {
  std::unique_ptr<xpl::iface::Protocol_flusher> result = std::move(m_flusher);
  m_flusher = std::move(flusher);

  return result;
}

void Protocol_encoder::start_row() {
  m_row_builder.begin_row();
  m_row = true;
}

void Protocol_encoder::abort_row() {
  m_row_builder.abort_row();
  m_row = false;
}

bool Protocol_encoder::send_row() {
  m_row_builder.end_row();
  get_protocol_monitor().on_row_send();
  m_row = false;

  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_ROW);
}

bool Protocol_encoder::send_result(const Error_code &result) {
  if (result.error == 0) return send_ok(result.message);

  if (result.severity == Error_code::FATAL)
    get_protocol_monitor().on_fatal_error_send();
  else
    get_protocol_monitor().on_error_send();

  return send_error(result);
}

bool Protocol_encoder::send_ok() {
  m_xproto_encoder.encode_ok();
  return send_raw_buffer(Mysqlx::ServerMessages::OK);
}

bool Protocol_encoder::send_ok(const std::string &message) {
  if (message.empty()) {
    m_xproto_encoder.encode_ok();
    return send_raw_buffer(Mysqlx::ServerMessages::OK);
  }

  m_xproto_encoder.encode_ok(message);

  return send_raw_buffer(Mysqlx::ServerMessages::OK);
}

bool Protocol_encoder::send_error(const Error_code &error_code,
                                  const bool init_error) {
  if (init_error && error_code.severity == Error_code::FATAL)
    m_protocol_monitor->on_init_error_send();

  m_xproto_encoder.encode_error(
      error_code.severity == Error_code::FATAL ? Mysqlx::Error::FATAL
                                               : Mysqlx::Error::ERROR,
      error_code.error, error_code.message, error_code.sql_state);

  return send_raw_buffer(Mysqlx::ServerMessages::ERROR);
}

void Protocol_encoder::send_auth_ok(const std::string &data) {
  std::string out_serialized_msg;
  Mysqlx::Session::AuthenticateOk msg;

  msg.set_auth_data(data);
  msg.SerializeToString(&out_serialized_msg);

  m_xproto_encoder
      .encode_xmessage<Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK>(
          out_serialized_msg);
  send_raw_buffer(Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK);
}

void Protocol_encoder::send_auth_continue(const std::string &data) {
  std::string out_serialized_msg;
  Mysqlx::Session::AuthenticateContinue msg;

  msg.set_auth_data(data);
  msg.SerializeToString(&out_serialized_msg);

  DBUG_EXECUTE_IF("authentication_timeout", {
    int i = 0;
    int max_iterations = 1000;

    while (modules::Module_mysqlx::get_instance_server()->is_running() &&
           i < max_iterations) {
      my_sleep(10000);
      ++i;
    }
  });

  m_xproto_encoder
      .encode_xmessage<Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE>(
          out_serialized_msg);
  send_raw_buffer(Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE);
}

bool Protocol_encoder::send_exec_ok() {
  m_xproto_encoder.encode_stmt_execute_ok();
  return send_raw_buffer(Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK);
}

bool Protocol_encoder::send_result_fetch_done() {
  m_xproto_encoder.encode_fetch_done();
  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_FETCH_DONE);
}

bool Protocol_encoder::send_result_fetch_suspended() {
  m_xproto_encoder.encode_fetch_suspended();
  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED);
}

bool Protocol_encoder::send_result_fetch_done_more_results() {
  m_xproto_encoder.encode_fetch_more_resultsets();
  return send_raw_buffer(
      Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS);
}

bool Protocol_encoder::send_result_fetch_done_more_out_params() {
  m_xproto_encoder.encode_fetch_out_params();
  return send_raw_buffer(
      Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS);
}

xpl::iface::Protocol_monitor &Protocol_encoder::get_protocol_monitor() {
  return *m_protocol_monitor;
}

bool Protocol_encoder::send_protobuf_message(const uint8_t type,
                                             const Message &message,
                                             bool force_buffer_flush) {
  log_message_send(m_id, &message);

  if (!message.IsInitialized()) {
    log_warning(ER_XPLUGIN_UNINITIALIZED_MESSAGE,
                message.InitializationErrorString().c_str());
  }

  std::string out_serialized;
  message.SerializeToString(&out_serialized);

  auto xmsg_start = m_xproto_encoder.begin_xmessage<100>(type);
  m_xproto_encoder.encode_raw(
      reinterpret_cast<const uint8_t *>(out_serialized.c_str()),
      out_serialized.length());
  m_xproto_encoder.end_xmessage(xmsg_start);

  if (force_buffer_flush) m_flusher->trigger_flush_required();

  return on_message(type);
}

void Protocol_encoder::on_error(int error) { m_error_handler(error); }

void Protocol_encoder::log_protobuf(const unsigned id,
                                    const char *direction_name,
                                    const uint8_t type, const Message *msg) {
  if (nullptr == msg) {
    log_protobuf(id, type);
    return;
  }

  log_protobuf(id, direction_name, msg);
}

void Protocol_encoder::log_protobuf(const unsigned id,
                                    const char *direction_name [[maybe_unused]],
                                    const Message *message [[maybe_unused]]) {
#ifdef USE_MYSQLX_FULL_PROTO
  std::string text_message;

  if (message)
    google::protobuf::TextFormat::PrintToString(*message, &text_message);

  if (text_message.length()) {
    const std::size_t index_of_last_enter = text_message.find_last_of("\n");

    text_message.resize(index_of_last_enter);

    log_debug("%u: %s, Type: %s, Payload:\n%s", id, direction_name,
              message->GetTypeName().c_str(), text_message.c_str());
  } else {
    log_debug("%u: %s, Type: ??, Payload: (none)", id, direction_name);
  }
#else
  log_debug("%u: %s, Type: %s", id, direction_name,
            message->GetTypeName().c_str());
#endif
}

std::string message_type_to_string(const uint8_t type_id) {
  switch (type_id) {
    case Mysqlx::ServerMessages_Type_OK:
      return "OK";
    case Mysqlx::ServerMessages_Type_ERROR:
      return "ERROR";
    case Mysqlx::ServerMessages_Type_CONN_CAPABILITIES:
      return "CONN_CAPABILITIES";
    case Mysqlx::ServerMessages_Type_SESS_AUTHENTICATE_CONTINUE:
      return "AUTHENTICATE_CONTINUE";
    case Mysqlx::ServerMessages_Type_SESS_AUTHENTICATE_OK:
      return "AUTHENTICATE_OK";
    case Mysqlx::ServerMessages_Type_NOTICE:
      return "NOTICE";
    case Mysqlx::ServerMessages_Type_RESULTSET_COLUMN_META_DATA:
      return "COLUMN_META_DATA";
    case Mysqlx::ServerMessages_Type_RESULTSET_ROW:
      return "ROW";
    case Mysqlx::ServerMessages_Type_RESULTSET_FETCH_DONE:
      return "FETCH_DONE";
    case Mysqlx::ServerMessages_Type_RESULTSET_FETCH_SUSPENDED:
      return "FETCH_SUSPENDED";
    case Mysqlx::ServerMessages_Type_RESULTSET_FETCH_DONE_MORE_RESULTSETS:
      return "RESULTSET_FETCH_DONE_MORE_RESULTSETS";
    case Mysqlx::ServerMessages_Type_SQL_STMT_EXECUTE_OK:
      return "STMT_EXECUTE_OK";
    case Mysqlx::ServerMessages_Type_RESULTSET_FETCH_DONE_MORE_OUT_PARAMS:
      return "FETCH_DONE_MORE_OUT_PARAMS";

    default:
      return std::to_string(type_id);
  }
}

// for message sent as raw buffer only logging its type tag now
void Protocol_encoder::log_protobuf(const unsigned id,
                                    uint8_t type [[maybe_unused]]) {
  log_debug("%u: SEND RAW- Type: %s", id, message_type_to_string(type).c_str());
}

#ifdef XPLUGIN_LOG_DEBUG
static std::string get_name(const xpl::iface::Frame_type type) {
  using Type = xpl::iface::Frame_type;
  switch (type) {
    case Type::k_warning:
      return "warning";
    case Type::k_group_replication_state_changed:
      return "group_replication_state_changed";
    case Type::k_server_hello:
      return "server_hello";
    case Type::k_session_state_changed:
      return "session_state_changed";
    case Type::k_session_variable_changed:
      return "session_variable_changed";

    default:
      assert(0 && "This shouldn't happen.");
      return "unknown";
  }
}
#endif

bool Protocol_encoder::send_notice(const xpl::iface::Frame_type type,
                                   const xpl::iface::Frame_scope scope,
                                   const std::string &data,
                                   const bool force_flush) {
  const bool is_global = xpl::iface::Frame_scope::k_global == scope;
  log_debug("send_notice, global: %s, name: %s", (is_global ? "yes" : "no"),
            get_name(type).c_str());

  if (xpl::iface::Frame_type::k_warning == type)
    get_protocol_monitor().on_notice_warning_send();
  else if (is_global)
    get_protocol_monitor().on_notice_global_send();
  else
    get_protocol_monitor().on_notice_other_send();

  if (is_global)
    m_xproto_encoder.encode_global_notice(static_cast<uint32_t>(type), data);
  else
    m_xproto_encoder.encode_notice(static_cast<uint32_t>(type),
                                   static_cast<uint32_t>(scope), data);

  if (force_flush) m_flusher->trigger_flush_required();

  return send_raw_buffer(Mysqlx::ServerMessages::NOTICE);
}

void Protocol_encoder::send_notice_rows_affected(uint64_t value) {
  get_protocol_monitor().on_notice_other_send();

  m_xproto_encoder.encode_notice_rows_affected(value);
  send_raw_buffer(Mysqlx::ServerMessages::NOTICE);
}

void Protocol_encoder::send_notice_client_id(const uint64_t id) {
  m_id = id;
  get_protocol_monitor().on_notice_other_send();

  m_xproto_encoder.encode_notice_client_id(id);
  send_raw_buffer(Mysqlx::ServerMessages::NOTICE);
}

void Protocol_encoder::send_notice_account_expired() {
  get_protocol_monitor().on_notice_other_send();

  m_xproto_encoder.encode_notice_expired();
  send_raw_buffer(Mysqlx::ServerMessages::NOTICE);
}

void Protocol_encoder::send_notice_txt_message(const std::string &message) {
  get_protocol_monitor().on_notice_other_send();

  m_xproto_encoder.encode_notice_text_message(message);
  send_raw_buffer(Mysqlx::ServerMessages::NOTICE);
}

void Protocol_encoder::send_notice_generated_document_ids(
    const std::vector<std::string> &ids) {
  if (ids.empty()) return;

  std::string serialized_change;
  Mysqlx::Notice::SessionStateChanged change;
  change.set_param(Mysqlx::Notice::SessionStateChanged::GENERATED_DOCUMENT_IDS);
  for (const auto &id : ids) {
    Mysqlx::Datatypes::Scalar *v = change.mutable_value()->Add();
    v->set_type(Mysqlx::Datatypes::Scalar::V_OCTETS);
    v->mutable_v_octets()->set_value(id);
  }

  // Optimize me
  change.SerializeToString(&serialized_change);

  send_notice(xpl::iface::Frame_type::k_session_state_changed,
              xpl::iface::Frame_scope::k_local, serialized_change, false);
}

void Protocol_encoder::send_notice_last_insert_id(const uint64_t id) {
  get_protocol_monitor().on_notice_other_send();

  m_xproto_encoder.encode_notice_generated_insert_id(id);
  send_raw_buffer(Mysqlx::ServerMessages::NOTICE);
}

bool Protocol_encoder::send_column_metadata(
    const Encode_column_info *column_info) {
  m_xproto_encoder.encode_metadata(column_info);

  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);
}

bool Protocol_encoder::send_raw_buffer(const uint8_t type) {
  log_raw_message_send(m_id, type);

  return on_message(type);
}

bool Protocol_encoder::on_message(const uint8_t type) {
  ++m_messages_sent;
  m_flusher->trigger_on_message(type);

  const auto result = m_flusher->try_flush();

  if (Flush_result::k_flushed == result) {
    m_protocol_monitor->on_messages_sent(m_messages_sent);
    m_messages_sent = 0;
  }

  return Flush_result::k_error != result;
}

}  // namespace ngs
