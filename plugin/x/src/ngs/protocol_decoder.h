// Copyright (c) 2015, 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_DECODER_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_DECODER_H_

#include <cstdint>
#include <memory>

#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/waiting_for_io.h"
#include "plugin/x/src/io/vio_input_stream.h"
#include "plugin/x/src/ngs/message_decoder.h"
#include "plugin/x/src/ngs/protocol/protocol_config.h"

namespace ngs {

/**
 X Protocol decoder

 Decoder directly operates on VIO, passing the data directly to protobuf.
*/
class Protocol_decoder {
 public:
  using Decode_error = Message_decoder::Decode_error;

  using Message_dispatcher_interface =
      Message_decoder::Message_dispatcher_interface;

 public:
  Protocol_decoder(Message_dispatcher_interface *dispatcher,
                   std::shared_ptr<xpl::iface::Vio> vio,
                   xpl::iface::Protocol_monitor *protocol_monitor,
                   std::shared_ptr<Protocol_config> config);

  Decode_error read_and_decode(xpl::iface::Waiting_for_io *wait_for_io);

  void set_wait_timeout(const uint32_t wait_timeout_in_seconds);
  void set_read_timeout(const uint32_t read_timeout_in_seconds);

 private:
  std::shared_ptr<xpl::iface::Vio> m_vio;
  xpl::iface::Protocol_monitor *m_protocol_monitor;
  xpl::Vio_input_stream m_vio_input_stream;
  std::shared_ptr<Protocol_config> m_config;
  Message_decoder m_message_decoder;
  uint64_t m_wait_timeout_in_ms;
  uint64_t m_read_timeout_in_ms;

  Decode_error read_and_decode_impl(xpl::iface::Waiting_for_io *wait_for_io);
  bool read_header(uint8_t *message_type, uint32_t *message_size,
                   xpl::iface::Waiting_for_io *wait_for_io);
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_DECODER_H_
