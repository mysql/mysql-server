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

#ifndef GROUP_REPLICATION_PFS_H
#define GROUP_REPLICATION_PFS_H

#include <mysql/components/services/registry.h>
#include <mysql/service_plugin_registry.h>
#include <memory>
#include <vector>
#include "mysql/components/services/pfs_plugin_table_service.h"

namespace gr {
namespace perfschema {

class Abstract_Pfs_table {
 protected:
  PFS_engine_table_share_proxy m_share;

 public:
  Abstract_Pfs_table() {}
  virtual ~Abstract_Pfs_table() {}

  PFS_engine_table_share_proxy *get_share() { return &m_share; }

  virtual bool init() = 0;
  virtual bool deinit() = 0;
};

class Perfschema_module {
 public:
  using Pfs_tables = std::vector<std::unique_ptr<Abstract_Pfs_table>>;

 protected:
  Pfs_tables m_tables{};

 public:
  Perfschema_module() = default;
  virtual ~Perfschema_module() = default;

  virtual bool initialize();
  virtual bool finalize();

  // PFS functionality
  bool register_pfs_tables(Pfs_tables &tables);
  bool unregister_pfs_tables(Pfs_tables &tables);
};

}  // namespace perfschema
}  // namespace gr

#endif
