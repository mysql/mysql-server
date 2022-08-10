/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/kv_store_stats.h
TempTable key-value store stats description. */

#ifndef TEMPTABLE_KV_STORE_STATS_H
#define TEMPTABLE_KV_STORE_STATS_H

#include <thread>

namespace temptable {

/** This is a small convenience POD-like type which describes what kind of
 * details we are interested in when monitoring the behavior of Key_value_store.
 * Details directly correlate to the properties of the underlying data-structure
 * that Key_value_store is using.
 * */
struct Key_value_store_stats {
  enum class Event { EMPLACE, ERASE };
  Event event;
  size_t size;
  size_t bucket_count;
  double load_factor;
  double max_load_factor;
  size_t max_bucket_count;
  std::thread::id thread_id;
};

}  // namespace temptable

#endif /* TEMPTABLE_KV_STORE_STATS_H */
