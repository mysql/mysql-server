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


// "ngs_common/protocol_protobuf.h" has to come before boost includes, because of build
// issue in Solaris (unqualified map used, which clashes with some other map defined
// in Solaris headers)
#include "ngs_common/protocol_protobuf.h"
#include "ngs_common/connection_vio.h"

#include "ngs/protocol/buffer.h"
#include "ngs/protocol/output_buffer.h"
#include "ngs/protocol/protocol_config.h"
#include "ngs/protocol_encoder.h"
#include "ngs/protocol_monitor.h"
#include "ngs/log.h"

#undef ERROR // Needed to avoid conflict with ERROR in mysqlx.pb.h


using namespace ngs;

const Pool_config Protocol_encoder::m_default_pool_config = { 0, 5, BUFFER_PAGE_SIZE };

Protocol_encoder::Protocol_encoder(const ngs::shared_ptr<Connection_vio> &socket,
                                   Error_handler ehandler,
                                   Protocol_monitor_interface &pmon)
: m_pool(m_default_pool_config),
  m_socket(socket),
  m_error_handler(ehandler),
  m_protocol_monitor(&pmon)
{
  m_buffer.reset(ngs::allocate_object<Output_buffer>(ngs::ref(m_pool)));
}

Protocol_encoder::~Protocol_encoder()
{
}

void Protocol_encoder::start_row()
{
  m_row_builder.start_row(get_buffer());
}

void Protocol_encoder::abort_row()
{
  m_row_builder.abort_row();
}

bool Protocol_encoder::send_row()
{
  m_row_builder.end_row();
  get_protocol_monitor().on_row_send();

  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_ROW);
}

bool Protocol_encoder::send_result(const Error_code &result)
{
  if (result.error == 0)
  {
    Mysqlx::Ok ok;
    if (!result.message.empty())
      ok.set_msg(result.message);
    return send_message(Mysqlx::ServerMessages::OK, ok);
  }
  else
  {
    if (result.severity == ngs::Error_code::FATAL)
      get_protocol_monitor().on_fatal_error_send();
    else
      get_protocol_monitor().on_error_send();

    Mysqlx::Error error;
    error.set_code(result.error);
    error.set_msg(result.message);
    error.set_sql_state(result.sql_state);
    error.set_severity(result.severity == Error_code::FATAL ? Mysqlx::Error::FATAL : Mysqlx::Error::ERROR);
    return send_message(Mysqlx::ServerMessages::ERROR, error);
  }
}


bool Protocol_encoder::send_ok()
{
  return send_message(Mysqlx::ServerMessages::OK, Mysqlx::Ok());
}


bool Protocol_encoder::send_ok(const std::string &message)
{
  Mysqlx::Ok ok;

  if (!message.empty())
    ok.set_msg(message);

  return send_message(Mysqlx::ServerMessages::OK, ok);
}


bool Protocol_encoder::send_init_error(const Error_code& error_code)
{
  m_protocol_monitor->on_init_error_send();

  Mysqlx::Error error;

  error.set_code(error_code.error);
  error.set_msg(error_code.message);
  error.set_sql_state(error_code.sql_state);
  error.set_severity(Mysqlx::Error::FATAL);

  return send_message(Mysqlx::ServerMessages::ERROR, error);
}


void Protocol_encoder::send_local_notice(Notice_type type,
                                         const std::string &data,
                                         bool force_flush)
{
  get_protocol_monitor().on_notice_other_send();

  send_notice(type, data, FRAME_SCOPE_LOCAL, force_flush);
}

/*
NOTE: Commented for coverage. Uncomment when needed.

void Protocol_encoder::send_global_notice(Notice_type type, const std::string &data)
{
  get_protocol_monitor().on_notice_other_send();

  send_notice(type, data, FRAME_SCOPE_GLOBAL, true);
}
*/

void Protocol_encoder::send_local_warning(const std::string &data, bool force_flush)
{
  get_protocol_monitor().on_notice_warning_send();

  send_notice(k_notice_warning, data, FRAME_SCOPE_LOCAL, force_flush);
}


void Protocol_encoder::send_auth_ok(const std::string &data)
{
  Mysqlx::Session::AuthenticateOk msg;

  msg.set_auth_data(data);

  send_message(Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK, msg);
}

void Protocol_encoder::send_auth_continue(const std::string &data)
{
  Mysqlx::Session::AuthenticateContinue msg;

  msg.set_auth_data(data);

  send_message(Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE, msg);
}

bool Protocol_encoder::send_empty_message(uint8_t message_id)
{
  log_raw_message_send(message_id);

  m_empty_msg_builder.encode_empty_message(m_buffer.get(), message_id);

  return enqueue_buffer(message_id);
}

bool Protocol_encoder::send_exec_ok()
{
  return send_empty_message(Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK);
}


bool Protocol_encoder::send_result_fetch_done()
{
  return send_empty_message(Mysqlx::ServerMessages::RESULTSET_FETCH_DONE);
}


bool Protocol_encoder::send_result_fetch_done_more_results()
{
  return send_empty_message(Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS);
}


Protocol_monitor_interface &Protocol_encoder::get_protocol_monitor()
{
  return *m_protocol_monitor;
}

bool Protocol_encoder::send_message(int8_t type, const Message &message, bool force_buffer_flush)
{
  const size_t header_size = 5;

  log_message_send(&message);

  if (Memory_allocated != m_buffer->reserve(header_size + message.ByteSize()))
  {
    on_error(ENOMEM);
    return true;
  }
  if (!message.IsInitialized())
  {
    log_warning("Message is not properly initialized: %s", message.InitializationErrorString().c_str());
  }

  // header
  m_buffer->add_int32(message.ByteSize() + 1);
  m_buffer->add_int8(type);

  message.SerializeToZeroCopyStream(m_buffer.get());

  return enqueue_buffer(type, force_buffer_flush);
}


void Protocol_encoder::on_error(int error)
{
  m_error_handler(error);
}


void Protocol_encoder::log_protobuf(const char *direction_name, Request &request)
{
  const Message *message = request.message();

  if (NULL == message)
  {
    log_protobuf(request.get_type());
    return;
  }

  log_protobuf(direction_name, message);
}


void Protocol_encoder::log_protobuf(const char *direction_name, const Message *message)
{
#ifdef USE_MYSQLX_FULL_PROTO
  std::string text_message;

  if (message)
    google::protobuf::TextFormat::PrintToString(*message, &text_message);

  if (text_message.length())
  {
    const std::size_t index_of_last_enter = text_message.find_last_of("\n");

    text_message.resize(index_of_last_enter);

    log_debug("%s: Type: %s, Payload:\n%s", direction_name, message->GetTypeName().c_str(), text_message.c_str());
  }
  else
  {
    log_debug("%s: Type: ??, Payload: (none)", direction_name);
  }
#else
  log_debug("%s: Type: %s", direction_name, message->GetTypeName().c_str());
#endif
}

// for message sent as raw buffer only logging its type tag now
void Protocol_encoder::log_protobuf(int8_t type)
{
  log_debug("SEND RAW: Type: %d", type);
}


void Protocol_encoder::send_notice(uint32_t type, const std::string &data,
  Frame_scope scope, bool force_flush)
{
  int iscope = (scope == FRAME_SCOPE_GLOBAL) ? static_cast<int>(Mysqlx::Notice::Frame_Scope_GLOBAL) :
    static_cast<int>(Mysqlx::Notice::Frame_Scope_LOCAL);

  log_raw_message_send(Mysqlx::ServerMessages::NOTICE);

  m_notice_builder.encode_frame(m_buffer.get(), type, data, iscope);
  enqueue_buffer(Mysqlx::ServerMessages::NOTICE, force_flush);
}

void Protocol_encoder::send_rows_affected(uint64_t value)
{
  get_protocol_monitor().on_notice_other_send();
  log_raw_message_send(Mysqlx::ServerMessages::NOTICE);

  m_notice_builder.encode_rows_affected(m_buffer.get(), value);
  enqueue_buffer(Mysqlx::ServerMessages::NOTICE);
}

bool Protocol_encoder::send_column_metadata(const std::string &catalog,
  const std::string &db_name,
  const std::string &table_name, const std::string &org_table_name,
  const std::string &col_name, const std::string &org_col_name,
  uint64_t collation, int type, int decimals,
  uint32_t flags, uint32_t length, uint32_t content_type)
{
  m_metadata_builder.encode_metadata(m_buffer.get(),
    catalog, db_name, table_name, org_table_name,
    col_name, org_col_name, collation, type, decimals,
    flags, length, content_type);

  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);
}

bool Protocol_encoder::send_column_metadata(uint64_t collation, int type, int decimals,
  uint32_t flags, uint32_t length, uint32_t content_type)
{
  m_metadata_builder.encode_metadata(m_buffer.get(),
    collation, type, decimals,
    flags, length, content_type);

  return send_raw_buffer(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);
}


bool Protocol_encoder::flush_buffer()
{
  const bool is_valid_socket = INVALID_SOCKET != m_socket->get_socket_id();

  if (is_valid_socket)
  {
    const ssize_t result = m_socket->write(m_buffer->get_buffers());
    if (result <= 0)
    {
      log_info("Error writing to client: %s (%i)", strerror(errno), errno);
      on_error(errno);
      return false;
    }

    m_protocol_monitor->on_send(static_cast<long>(result));
  }

  m_buffer->reset();

  return true;
}


bool Protocol_encoder::send_raw_buffer(int8_t type)
{
  log_raw_message_send(type);

  return enqueue_buffer(type);
}


bool Protocol_encoder::enqueue_buffer(int8_t type, bool force_flush)
{

  bool can_buffer = (!force_flush) &&
    (
    (type == Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA) ||
    (type == Mysqlx::ServerMessages::RESULTSET_ROW) ||
    (type == Mysqlx::ServerMessages::NOTICE) ||
    (type == Mysqlx::ServerMessages::RESULTSET_FETCH_DONE)
    );

  // todo: more testing for this thold
  if (!can_buffer || (m_buffer->ByteCount() > BUFFER_PAGE_SIZE * 4))
  {
    return flush_buffer();
  }

  return true;
}
