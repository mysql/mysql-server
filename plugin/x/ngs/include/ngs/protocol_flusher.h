/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_FLUSHER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_FLUSHER_H_

#include <functional>
#include <memory>

#include "my_inttypes.h"

#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/src/global_timeouts.h"
#include "plugin/x/src/interface/protocol_flusher.h"

namespace protocol {

class Encoding_buffer;
class XProtocol_encoder;
class XMessage_encoder;

}  // namespace protocol

namespace ngs {

using Error_handler = std::function<void(int error)>;

class Protocol_flusher : public xpl::iface::Protocol_flusher {
 public:
  Protocol_flusher(protocol::Encoding_buffer *buffer,
                   protocol::XMessage_encoder *encoder,
                   Protocol_monitor_interface *protocol_monitor,
                   const std::shared_ptr<Vio_interface> &socket,
                   const Error_handler &error_handler)
      : m_buffer(buffer),
        m_encoder(encoder),
        m_protocol_monitor(protocol_monitor),
        m_socket(socket),
        m_on_error(error_handler) {}

  /**
    Force that next `try_flush` is going to dispatch data.
   */
  void trigger_flush_required() override;
  void trigger_on_message(const uint8 type) override;

  /**
    Check if flush is required and try to execute it

    Flush is not going to be executed when the flusher is locked or
    when no other conditions to flush were fulfilled.

    @return result of flush operation
      @retval == k_flushed     flush was successful
      @retval == k_not_flushed nothing important to flush
      @retval == k_error       flush IO was failed
   */
  Result try_flush() override;

  bool is_going_to_flush() override { return m_flush; }

  void set_write_timeout(const uint32_t timeout) override {
    m_write_timeout = timeout;
  }

 private:
  bool flush();

  protocol::Encoding_buffer *m_buffer;
  protocol::XMessage_encoder *m_encoder;
  //  Page_output_stream *m_page_output_stream;
  Protocol_monitor_interface *m_protocol_monitor;
  std::shared_ptr<Vio_interface> m_socket;
  uint32_t m_write_timeout =
      static_cast<uint32_t>(Global_timeouts::Default::k_write_timeout);

  bool m_flush = false;
  bool m_io_error = false;
  Error_handler m_on_error;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_FLUSHER_H_
