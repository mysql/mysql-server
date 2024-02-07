/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "plugin/group_replication/include/perfschema/pfs.h"

#ifndef GROUP_REPLICATION_TABLE_COMMUNICATION_INFORMATION_H
#define GROUP_REPLICATION_TABLE_COMMUNICATION_INFORMATION_H

namespace gr {
namespace perfschema {

class Pfs_table_communication_information : public Abstract_Pfs_table {
 public:
  Pfs_table_communication_information() = default;
  ~Pfs_table_communication_information() override = default;

  bool init() override;
  bool deinit() override;

  static unsigned long long get_row_count();
  static int rnd_next(PSI_table_handle *handle);
  static int rnd_init(PSI_table_handle *handle [[maybe_unused]],
                      bool scan [[maybe_unused]]);
  static int rnd_pos(PSI_table_handle *handle [[maybe_unused]]);
  static void reset_position(PSI_table_handle *handle);
  static int read_column_value(PSI_table_handle *handle, PSI_field *field,
                               unsigned int index [[maybe_unused]]);
  static PSI_table_handle *open_table(PSI_pos **pos);
  static void close_table(PSI_table_handle *handle);

  static constexpr int SUCCESS = 0;
  static constexpr unsigned long long NR_ROWS{1};
};

}  // namespace perfschema
}  // namespace gr

#endif /* GROUP_REPLICATION_TABLE_COMMUNICATION_INFORMATION_H */
