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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PROTOCOL_CONFIG_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PROTOCOL_CONFIG_H_

#include <stdint.h>
#include <list>
#include <memory>

#include "plugin/x/src/helper/chrono.h"

#include "plugin/x/ngs/include/ngs/compression_types.h"
#include "plugin/x/src/global_timeouts.h"
#include "plugin/x/src/xpl_system_variables.h"

namespace ngs {

class Protocol_global_config {
 public:
  uint32_t max_message_size;

  xpl::chrono::Seconds connect_timeout;
  xpl::chrono::Milliseconds connect_timeout_hysteresis;
  uint32_t m_wait_timeout = Global_timeouts::Default::k_wait_timeout;
  uint32_t m_interactive_timeout =
      Global_timeouts::Default::k_interactive_timeout;
  uint32_t m_read_timeout = Global_timeouts::Default::k_read_timeout;
  uint32_t m_write_timeout = Global_timeouts::Default::k_write_timeout;

  void set_global_timeouts(const Global_timeouts &timeouts) {
    m_interactive_timeout = timeouts.interactive_timeout;
    m_wait_timeout = timeouts.wait_timeout;
    m_read_timeout = timeouts.read_timeout;
    m_write_timeout = timeouts.write_timeout;
  }

  Global_timeouts get_global_timeouts() const {
    return {m_interactive_timeout, m_wait_timeout, m_read_timeout,
            m_write_timeout};
  }

  Protocol_global_config()
      : max_message_size(16 * 1024 * 1024),
        connect_timeout(0),
        connect_timeout_hysteresis(100) {}
};

class Protocol_config {
 public:
  explicit Protocol_config(
      const std::shared_ptr<Protocol_global_config> &global)
      : m_global(global) {}

  Compression_algorithm m_compression_algorithm = Compression_algorithm::k_none;
  Compression_style m_compression_server_style = Compression_style::k_none;
  Compression_style m_compression_client_style = Compression_style::k_none;
  std::shared_ptr<Protocol_global_config> m_global;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PROTOCOL_CONFIG_H_
