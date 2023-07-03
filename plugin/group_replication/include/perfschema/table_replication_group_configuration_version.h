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

#include "plugin/group_replication/include/perfschema/pfs.h"
#include "plugin/group_replication/include/services/registry.h"

#ifndef TABLE_REPLICATION_GROUP_CONFIGURATION_VERSION_INCLUDE
#define TABLE_REPLICATION_GROUP_CONFIGURATION_VERSION_INCLUDE

namespace gr {
namespace perfschema {

class Pfs_table_replication_group_configuration_version
    : public Abstract_Pfs_table {
 public:
  Pfs_table_replication_group_configuration_version() = default;
  ~Pfs_table_replication_group_configuration_version() override = default;

  bool init() override;
  bool deinit() override;

  static unsigned long long get_row_count();
  static int rnd_init(PSI_table_handle *handle [[maybe_unused]],
                      bool scan [[maybe_unused]]);
  static int rnd_next(PSI_table_handle *handle);
  static int rnd_pos(PSI_table_handle *handle);
  static void reset_position(PSI_table_handle *handle);
  static int read_column_value(PSI_table_handle *handle, PSI_field *field,
                               unsigned int index);
  static PSI_table_handle *open_table(PSI_pos **pos [[maybe_unused]]);
  static void close_table(PSI_table_handle *handle);
};

}  // namespace perfschema
}  // namespace gr

#endif /* TABLE_REPLICATION_GROUP_CONFIGURATION_VERSION_INCLUDE */
