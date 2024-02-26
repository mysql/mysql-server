/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef GROUP_REPLICATION_PFS_UTILITIES_H
#define GROUP_REPLICATION_PFS_UTILITIES_H

#include <mysql/components/services/registry.h>
#include <mysql/service_plugin_registry.h>

#include <vector>
#include "mysql/components/services/pfs_plugin_table_service.h"

namespace gr {
namespace perfschema {

class Registry_guard {
 private:
  SERVICE_TYPE(registry) * m_registry{nullptr};

 public:
  Registry_guard() : m_registry{mysql_plugin_registry_acquire()} {}

  ~Registry_guard() {
    if (m_registry) mysql_plugin_registry_release(m_registry);
  }

  SERVICE_TYPE(registry) * get_registry() { return m_registry; }
};

class Position {
 private:
  unsigned int m_index{0};
  unsigned int m_max{0};

 public:
  void set_max(unsigned int max) { m_max = max; }
  bool has_more() { return m_index < m_max; }
  void next() { m_index++; }
  void reset() { m_index = 0; }
  unsigned int get_index() { return m_index; }
  void set_at(unsigned int index) { m_index = index; }
  void set_at(Position *pos) { m_index = pos->get_index(); }
  void set_after(Position *pos) { m_index = pos->get_index() + 1; }
};

}  // namespace perfschema
}  // namespace gr

#endif
