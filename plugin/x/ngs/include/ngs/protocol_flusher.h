/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "my_inttypes.h"

#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/page_output_stream.h"
#include "plugin/x/src/global_timeouts.h"

namespace ngs {

using Error_handler = std::function<void(int error)>;

class Protocol_flusher {
 public:
  Protocol_flusher(Page_output_stream *page_output_stream,
                   Protocol_monitor_interface *protocol_monitor,
                   const std::shared_ptr<Vio_interface> &socket,
                   const Error_handler &error_handler)
      : m_page_output_stream(page_output_stream),
        m_protocol_monitor(protocol_monitor),
        m_socket(socket),
        m_on_error(error_handler) {}

  /**
    Force that next `try_flush` is going to dispatch data.
   */
  void mark_flush();
  void on_message(const uint8 type);

  /**
    Check if flush is required and try to execute it

    Flush is not going to be executed when the flusher is locked or
    when no other conditions to flush were fulfilled.

    @return result of flush operation
      @retval == true   No flush was needed, or flush IO was successful
      @retval == false  flush IO was failed
   */
  bool try_flush();

  void set_write_timeout(const uint32_t timeout) { m_write_timeout = timeout; }

 private:
  bool flush();

  Page_output_stream *m_page_output_stream;
  Protocol_monitor_interface *m_protocol_monitor;
  std::shared_ptr<Vio_interface> m_socket;
  uint32_t m_write_timeout =
      static_cast<uint32_t>(Global_timeouts::Default::k_write_timeout);

  bool m_flush = false;
  Error_handler m_on_error;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_FLUSHER_H_
