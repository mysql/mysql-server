/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "plugin/x/ngs/include/ngs/document_id_generator.h"

#include <cinttypes>
#include <limits>

#include "plugin/x/ngs/include/ngs_common/chrono.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace ngs {

Document_id_generator::Document_id_generator()
    : m_timestamp{static_cast<uint64_t>(
          chrono::to_seconds(chrono::system_clock::now().time_since_epoch()))},
      m_serial{0},
      m_generate_mutex{KEY_mutex_x_document_id_generate} {}

Document_id_generator::Document_id_generator(const uint64_t timestamp,
                                             const uint64_t serial)
    : m_timestamp{timestamp},
      m_serial{serial},
      m_generate_mutex{KEY_mutex_x_document_id_generate} {}

std::string Document_id_generator::generate(const Variables &vars) {
  MUTEX_LOCK(lock, m_generate_mutex);
  const uint64_t k_prev_serial = m_serial;
  if (vars.increment > 1) {
    uint64_t tmp = (m_serial + vars.increment - vars.offset) /
                   static_cast<uint64_t>(vars.increment);
    m_serial = tmp * static_cast<uint64_t>(vars.increment) + vars.offset;
  } else {
    ++m_serial;
  }

  if (m_serial <= k_prev_serial) {
    ++m_timestamp;
    m_serial = vars.offset;
  }

  char buff[32] = {0};
  snprintf(buff, sizeof(buff), "%s%08" PRIx64 "%016" PRIx64, vars.prefix,
           m_timestamp, m_serial);

  return std::string(buff);
}

}  // namespace ngs
