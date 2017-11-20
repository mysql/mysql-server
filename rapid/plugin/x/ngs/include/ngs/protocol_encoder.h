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

#ifndef _NGS_PROTOCOL_ENCODER_H_
#define _NGS_PROTOCOL_ENCODER_H_

#include <vector>
#include <map>

#include "ngs/protocol/message.h"
#include "ngs/protocol/page_pool.h"
#include "ngs/protocol/output_buffer.h"
#include "ngs/interface/protocol_encoder_interface.h"
#include "ngs/error_code.h"
#include "ngs/memory.h"

#include "ngs/protocol_fwd.h"
#include "ngs_common/smart_ptr.h"
#include "ngs_common/chrono.h"
#include "ngs/protocol/message_builder.h"
#include "ngs/protocol/notice_builder.h"
#include "ngs/protocol/row_builder.h"
#include "ngs/protocol/metadata_builder.h"
#include "ngs/interface/protocol_monitor_interface.h"


namespace ngs
{

class Connection_vio;
class Output_buffer;

class Protocol_encoder: public Protocol_encoder_interface
{
public:
  typedef ngs::function<void (int error)> Error_handler;

  Protocol_encoder(const ngs::shared_ptr<Connection_vio> &socket,
                   Error_handler ehandler,
                   Protocol_monitor_interface &pmon);

  virtual ~Protocol_encoder();

  bool send_result(const Error_code &result) override;

  bool send_ok() override;
  bool send_ok(const std::string &message) override;
  bool send_init_error(const Error_code& error_code) override;

  void send_rows_affected(uint64_t value) override;

  void send_notice(
      const Frame_type type,
      const Frame_scope scope,
      const std::string &data,
      const bool force_flush = false) override;

  void send_auth_ok(const std::string &data) override;
  void send_auth_continue(const std::string &data) override;

  bool send_exec_ok() override;
  bool send_result_fetch_done() override;
  bool send_result_fetch_done_more_results() override;

  bool send_column_metadata(const std::string &catalog,
                            const std::string &db_name,
                            const std::string &table_name, const std::string &org_table_name,
                            const std::string &col_name, const std::string &org_col_name,
                            uint64_t collation, int type, int decimals,
                            uint32_t flags, uint32_t length, uint32_t content_type = 0) override;

  bool send_column_metadata(uint64_t collation, int type, int decimals,
                            uint32_t flags, uint32_t length, uint32_t content_type = 0) override;

  Row_builder& row_builder() override { return m_row_builder; }
  void start_row() override;
  void abort_row() override;
  // sends the row that was written directly into Encoder's buffer
  bool send_row() override;

  Output_buffer *get_buffer() override { return m_buffer.get(); }

  virtual bool send_message(int8_t type, const Message &message, bool force_buffer_flush = false) override;
  virtual void on_error(int error) override;

  virtual Protocol_monitor_interface &get_protocol_monitor() override;

  static void log_protobuf(const char *direction_name, Request &request);
  static void log_protobuf(const char *direction_name, const Message *request);
  static void log_protobuf(int8_t type);

private:
  Protocol_encoder(const Protocol_encoder &) = delete;
  Protocol_encoder &operator=(const Protocol_encoder &) = delete;

  virtual bool send_empty_message(uint8_t message_id);

  // Temporary solution for all io
  static const Pool_config m_default_pool_config;
  ngs::Page_pool m_pool;
  ngs::shared_ptr<Connection_vio> m_socket;
  Error_handler m_error_handler;
  Protocol_monitor_interface *m_protocol_monitor;

  Output_buffer_unique_ptr m_buffer;

  Row_builder       m_row_builder;
  Metadata_builder  m_metadata_builder;
  Message_builder   m_empty_msg_builder;
  Notice_builder    m_notice_builder;

  // add the m_out_buffer contents to the output queue... thread-safe
  bool flush_buffer(); // ownership of buffer is taken

  bool enqueue_buffer(int8_t type, bool force_flush = false);
  bool send_raw_buffer(int8_t type);
};

#ifdef XPLUGIN_LOG_PROTOBUF
#define log_message_send(MESSAGE)\
    ::ngs::Protocol_encoder::log_protobuf("SEND", MESSAGE);
#define log_raw_message_send(ID) \
    ::ngs::Protocol_encoder::log_protobuf(ID);
#define log_message_recv(MESSAGE)\
    ::ngs::Protocol_encoder::log_protobuf("RECV", MESSAGE);
#else
#define log_message_send(MESSAGE)    do {} while (0)
#define log_raw_message_send(ID)     do {} while (0)
#define log_message_recv(MESSAGE)    do {} while (0)
#endif // XPLUGIN_LOG_PRTOBUF

} // namespace ngs

#endif // _NGS_PROTOCOL_ENCODER_H_
