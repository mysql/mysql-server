/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "aggregated_stats.h"
#include <cassert>

void aggregated_stats::flush() {
  for (auto &shard : shards_) {
    shard.flush();
  }
}

void aggregated_stats::get_total(aggregated_stats_buffer &result) {
  // assume result object initially empty
  for (auto &shard : shards_) {
    result.add_from(shard);
  }
}

uint64_t aggregated_stats::get_single_total(size_t offset) {
  assert(offset < sizeof(aggregated_stats_buffer));

  uint64_t total = 0ULL;
  for (auto &shard : shards_) {
    total += shard.get_counter(offset);
  }
  return total;
}
