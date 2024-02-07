/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_FLUSHER_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_FLUSHER_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/interface/protocol_flusher.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/vio.h"

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
                   xpl::iface::Protocol_monitor *protocol_monitor,
                   const std::shared_ptr<xpl::iface::Vio> &socket,
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
  void trigger_on_message(const uint8_t type) override;

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

  xpl::iface::Vio *get_connection() override { return m_socket.get(); }

 private:
  bool flush();

  protocol::Encoding_buffer *m_buffer;
  protocol::XMessage_encoder *m_encoder;
  //  Page_output_stream *m_page_output_stream;
  xpl::iface::Protocol_monitor *m_protocol_monitor;
  uint32_t m_write_timeout;
  std::shared_ptr<xpl::iface::Vio> m_socket;
  bool m_flush = false;
  bool m_io_error = false;
  Error_handler m_on_error;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_FLUSHER_H_
