// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "plugin/group_replication/include/certification/gtid_generator.h"
#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin_utils.h"

namespace gr {

std::pair<rpl_gno, mysql::utils::Return_status>
Gtid_generator::get_next_available_gtid(const char *member_uuid,
                                        rpl_sidno sidno,
                                        const Gtid_set &gtid_set) {
  DBUG_TRACE;
  auto [iterator, is_inserted] = m_gtid_generator_for_sidno.try_emplace(
      sidno, sidno, m_gtid_assignment_block_size);
  auto &gtid_generator_for_sidno = iterator->second;
  if (is_inserted) {
    gtid_generator_for_sidno.compute_group_available_gtid_intervals(gtid_set);
  }
  auto res =
      gtid_generator_for_sidno.get_next_available_gtid(member_uuid, gtid_set);
  // If we did log a view change event we need to recompute
  // intervals, so that all members start from the same
  // intervals.
  if (member_uuid == nullptr && m_gtid_assignment_block_size > 1) {
    recompute(gtid_set);
  }
  auto status = res.second;
  return std::make_pair(res.first, status);
}

void Gtid_generator::recompute(const Gtid_set &gtid_set) {
  DBUG_TRACE;
  for (auto &gtid_generator_for_sidno_entry : m_gtid_generator_for_sidno) {
    gtid_generator_for_sidno_entry.second
        .compute_group_available_gtid_intervals(gtid_set);
  }
}

void Gtid_generator::initialize(uint64 gtid_assignment_block_size) {
  DBUG_TRACE;
  m_gtid_assignment_block_size = gtid_assignment_block_size;
}

}  // namespace gr
