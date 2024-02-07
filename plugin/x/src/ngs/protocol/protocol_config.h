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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_PROTOCOL_CONFIG_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_PROTOCOL_CONFIG_H_

#include <cstdint>
#include <limits>
#include <list>
#include <memory>

#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/ngs/compression_types.h"
#include "plugin/x/src/variables/system_variables_defaults.h"
#include "plugin/x/src/variables/timeout_config.h"

namespace ngs {

class Protocol_global_config {
 public:
  uint32_t max_message_size;

  xpl::chrono::Seconds connect_timeout;
  xpl::chrono::Milliseconds connect_timeout_hysteresis;
  xpl::Timeouts_config m_timeouts;

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
  int32_t m_compression_level = std::numeric_limits<int32_t>::min();
  std::shared_ptr<Protocol_global_config> m_global;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_PROTOCOL_CONFIG_H_
