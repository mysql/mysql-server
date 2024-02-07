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

#ifndef AGGREGATED_STATS_H
#define AGGREGATED_STATS_H

#include "aggregated_stats_buffer.h"
#include "include/my_thread_local.h"  // my_thread_id

/**
   To facilitate calculating values of status variables aggregated per all THDs
   in real-time, each THD will update its stats into a matching buffer shard.
   Later we will aggregate the values accross all the shards to get the final
   values.

   This mechanism avoids possible contentions if all THDs would write directly
   to a single shared global buffer.
*/
struct aggregated_stats {
  constexpr static size_t STAT_SHARD_COUNT = 64;

  inline aggregated_stats_buffer &get_shard(my_thread_id thread_id) {
    const size_t shard_idx = thread_id % STAT_SHARD_COUNT;
    return shards_[shard_idx];
  }
  void flush();
  void get_total(aggregated_stats_buffer &result);
  uint64_t get_single_total(size_t offset);

 protected:
  aggregated_stats_buffer shards_[STAT_SHARD_COUNT];
};

#endif /* AGGREGATES_STATS_H */
