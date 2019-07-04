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

#include <errno.h>
#include <sys/types.h>
#include "my_dbug.h"
#include "my_io.h"
#include "my_systime.h"  // my_sleep

#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/ngs/include/ngs/protocol_encoder.h"

#include "plugin/x/ngs/include/ngs/protocol/page_buffer.h"
#include "plugin/x/ngs/include/ngs/protocol/page_output_stream.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

#include "plugin/x/src/xpl_server.h"

#undef ERROR  // Needed to avoid conflict with ERROR in mysqlx.pb.h

using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;

namespace ngs {

const Pool_config Protocol_encoder::m_default_pool_config = {0, 5,
                                                             BUFFER_PAGE_SIZE};

Protocol_encoder::Protocol_encoder(const std::shared_ptr<Vio_interface> &socket,
                                   Error_handler ehandler,
                                   Protocol_monitor_interface *pmon)
    : m_pool(m_default_pool_config),
      m_error_handler(ehandler),
      m_protocol_monitor(pmon),
      m_page_output_stream(m_pool),
      m_flusher(&m_page_output_stream, pmon, socket, ehandler) {}

void Protocol_encoder::start_row() { m_row_builder.start_row(get_buffer()); }

void Protocol_encoder::abort_row() { m_row_builder.abort_row(); }

bool Protocol_encoder::send_row() {
  m_row_builder.end_row();
  get_protocol_monitor().on_row_send();

  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_ROW);
}

bool Protocol_encoder::send_result(const Error_code &result) {
  if (result.error == 0) {
    Mysqlx::Ok ok;
    if (!result.message.empty()) ok.set_msg(result.message);
    return send_message(Mysqlx::ServerMessages::OK, ok);
  } else {
    if (result.severity == Error_code::FATAL)
      get_protocol_monitor().on_fatal_error_send();
    else
      get_protocol_monitor().on_error_send();

    Mysqlx::Error error;
    error.set_code(result.error);
    error.set_msg(result.message);
    error.set_sql_state(result.sql_state);
    error.set_severity(result.severity == Error_code::FATAL
                           ? Mysqlx::Error::FATAL
                           : Mysqlx::Error::ERROR);
    return send_message(Mysqlx::ServerMessages::ERROR, error);
  }
}

bool Protocol_encoder::send_ok() {
  return send_message(Mysqlx::ServerMessages::OK, Mysqlx::Ok());
}

bool Protocol_encoder::send_ok(const std::string &message) {
  Mysqlx::Ok ok;

  if (!message.empty()) ok.set_msg(message);

  return send_message(Mysqlx::ServerMessages::OK, ok);
}

bool Protocol_encoder::send_init_error(const Error_code &error_code) {
  if (error_code.severity == Error_code::FATAL)
    m_protocol_monitor->on_init_error_send();

  Mysqlx::Error error;

  error.set_code(error_code.error);
  error.set_msg(error_code.message);
  error.set_sql_state(error_code.sql_state);
  error.set_severity(error_code.severity == Error_code::FATAL
                         ? Mysqlx::Error::FATAL
                         : Mysqlx::Error::ERROR);

  return send_message(Mysqlx::ServerMessages::ERROR, error);
}

void Protocol_encoder::send_auth_ok(const std::string &data) {
  Mysqlx::Session::AuthenticateOk msg;

  msg.set_auth_data(data);

  send_message(Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK, msg);
}

void Protocol_encoder::send_auth_continue(const std::string &data) {
  Mysqlx::Session::AuthenticateContinue msg;

  msg.set_auth_data(data);

  DBUG_EXECUTE_IF("authentication_timeout", {
    int i = 0;
    int max_iterations = 1000;
    while ((*xpl::Server::get_instance())->server().is_running() &&
           i < max_iterations) {
      my_sleep(10000);
      ++i;
    }
  });

  send_message(Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE, msg);
}

bool Protocol_encoder::send_empty_message(uint8_t message_id) {
  log_raw_message_send(message_id);

  if (!m_empty_msg_builder.encode_empty_message(&m_page_output_stream,
                                                message_id))
    return false;

  return on_message(message_id);
}

bool Protocol_encoder::send_exec_ok() {
  return send_empty_message(Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK);
}

bool Protocol_encoder::send_result_fetch_done() {
  return send_empty_message(Mysqlx::ServerMessages::RESULTSET_FETCH_DONE);
}

bool Protocol_encoder::send_result_fetch_suspended() {
  return send_empty_message(Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED);
}

bool Protocol_encoder::send_result_fetch_done_more_results() {
  return send_empty_message(
      Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS);
}

bool Protocol_encoder::send_result_fetch_done_more_out_params() {
  return send_empty_message(
      Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS);
}

Protocol_monitor_interface &Protocol_encoder::get_protocol_monitor() {
  return *m_protocol_monitor;
}

bool Protocol_encoder::send_message(uint8_t type, const Message &message,
                                    bool force_buffer_flush) {
  log_message_send(&message);

  if (!message.IsInitialized()) {
    log_warning(ER_XPLUGIN_UNINITIALIZED_MESSAGE,
                message.InitializationErrorString().c_str());
  }

  // header
  const int header_size = 5;
  uint8 *header_ptr =
      static_cast<uint8 *>(m_page_output_stream.reserve_space(header_size));
  const auto payload_start_position = m_page_output_stream.ByteCount();

  if (nullptr == header_ptr) return false;

  message.SerializeToZeroCopyStream(&m_page_output_stream);

  CodedOutputStream::WriteLittleEndian32ToArray(
      m_page_output_stream.ByteCount() - payload_start_position + 1,
      header_ptr);
  header_ptr[4] = type;

  if (force_buffer_flush) m_flusher.mark_flush();

  return on_message(type);
}

void Protocol_encoder::on_error(int error) { m_error_handler(error); }

void Protocol_encoder::log_protobuf(const char *direction_name,
                                    const uint8 type, const Message *msg) {
  if (nullptr == msg) {
    log_protobuf(type);
    return;
  }

  log_protobuf(direction_name, msg);
}

void Protocol_encoder::log_protobuf(
    const char *direction_name MY_ATTRIBUTE((unused)),
    const Message *message MY_ATTRIBUTE((unused))) {
#ifdef USE_MYSQLX_FULL_PROTO
  std::string text_message;

  if (message)
    google::protobuf::TextFormat::PrintToString(*message, &text_message);

  if (text_message.length()) {
    const std::size_t index_of_last_enter = text_message.find_last_of("\n");

    text_message.resize(index_of_last_enter);

    log_debug("%s: Type: %s, Payload:\n%s", direction_name,
              message->GetTypeName().c_str(), text_message.c_str());
  } else {
    log_debug("%s: Type: ??, Payload: (none)", direction_name);
  }
#else
  log_debug("%s: Type: %s", direction_name, message->GetTypeName().c_str());
#endif
}

std::string message_type_to_string(const uint8 type_id) {
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
void Protocol_encoder::log_protobuf(uint8_t type MY_ATTRIBUTE((unused))) {
  log_debug("SEND RAW: Type: %s", message_type_to_string(type).c_str());
}

bool Protocol_encoder::send_notice(const Frame_type type,
                                   const Frame_scope scope,
                                   const std::string &data,
                                   const bool force_flush) {
  const bool is_global = Frame_scope::k_global == scope;

  if (Frame_type::k_warning == type)
    get_protocol_monitor().on_notice_warning_send();
  else if (is_global)
    get_protocol_monitor().on_notice_global_send();
  else
    get_protocol_monitor().on_notice_other_send();

  log_raw_message_send(Mysqlx::ServerMessages::NOTICE);

  m_notice_builder.encode_frame(&m_page_output_stream, static_cast<int>(type),
                                !is_global, data);

  if (force_flush) m_flusher.mark_flush();

  return on_message(Mysqlx::ServerMessages::NOTICE);
}

void Protocol_encoder::send_rows_affected(uint64_t value) {
  get_protocol_monitor().on_notice_other_send();
  log_raw_message_send(Mysqlx::ServerMessages::NOTICE);

  m_notice_builder.encode_rows_affected(&m_page_output_stream, value);
  on_message(Mysqlx::ServerMessages::NOTICE);
}

bool Protocol_encoder::send_column_metadata(
    const Encode_column_info *column_info) {
  m_metadata_builder.start_metadata_encoding();
  m_metadata_builder.encode_metadata(column_info);

  const auto &meta = m_metadata_builder.stop_metadata_encoding();

  CodedOutputStream(get_buffer()).WriteString(meta);

  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);
}

bool Protocol_encoder::send_raw_buffer(const uint8_t type) {
  log_raw_message_send(type);

  return on_message(type);
}

bool Protocol_encoder::on_message(const uint8_t type) {
  m_flusher.on_message(type);

  return m_flusher.try_flush();
}

}  // namespace ngs
