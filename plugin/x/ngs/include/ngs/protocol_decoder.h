// Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_DECODER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_DECODER_H_

#include <memory>

#include "my_inttypes.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/message_decoder.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/src/interface/waiting_for_io.h"
#include "plugin/x/src/io/vio_input_stream.h"

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
                   std::shared_ptr<Vio_interface> vio,
                   Protocol_monitor_interface *protocol_monitor,
                   std::shared_ptr<Protocol_config> config,
                   const uint32 wait_timeout_in_seconds,
                   const uint32 read_timeout_in_seconds)
      : m_vio(vio),
        m_protocol_monitor(protocol_monitor),
        m_vio_input_stream(m_vio),
        m_config(config),
        m_message_decoder(dispatcher, m_protocol_monitor, config),
        m_wait_timeout_in_ms(wait_timeout_in_seconds * 1000),
        m_read_timeout_in_ms(read_timeout_in_seconds * 1000) {}

  Decode_error read_and_decode(xpl::iface::Waiting_for_io *wait_for_io);

  void set_wait_timeout(const uint32 wait_timeout_in_seconds);
  void set_read_timeout(const uint32 read_timeout_in_seconds);

 private:
  std::shared_ptr<Vio_interface> m_vio;
  Protocol_monitor_interface *m_protocol_monitor;
  xpl::Vio_input_stream m_vio_input_stream;
  std::shared_ptr<Protocol_config> m_config;
  Message_decoder m_message_decoder;
  uint64_t m_wait_timeout_in_ms;
  uint64_t m_read_timeout_in_ms;

  Decode_error read_and_decode_impl(xpl::iface::Waiting_for_io *wait_for_io);
  bool read_header(uint8 *message_type, uint32 *message_size,
                   xpl::iface::Waiting_for_io *wait_for_io);
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_DECODER_H_
