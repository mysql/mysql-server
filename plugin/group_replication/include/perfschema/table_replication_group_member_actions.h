/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_REPLICATION_GROUP_MEMBER_ACTIONS_INCLUDE
#define TABLE_REPLICATION_GROUP_MEMBER_ACTIONS_INCLUDE

namespace gr {
namespace perfschema {

class Pfs_table_replication_group_member_actions : public Abstract_Pfs_table {
 public:
  Pfs_table_replication_group_member_actions() = default;
  ~Pfs_table_replication_group_member_actions() override = default;

  bool init() override;
  bool deinit() override;
};

}  // namespace perfschema
}  // namespace gr

#endif /* TABLE_REPLICATION_GROUP_MEMBER_ACTIONS_INCLUDE */
