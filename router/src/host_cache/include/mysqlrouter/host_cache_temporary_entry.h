/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_HOST_CACHE_TEMPORARY_ENTRY_H_
#define MYSQLROUTER_HOST_CACHE_TEMPORARY_ENTRY_H_

#include <atomic>
#include <chrono>

#include "mysql/harness/utility/wait_variable.h"
#include "mysqlrouter/host_cache_entry.h"

namespace host_cache {

class TemporaryEntry {
 public:
  enum class State { Initializing, Resolving, Finished };
  using steady_clock = std::chrono::steady_clock;
  using time_point = steady_clock::time_point;

  template <typename ValueType>
  using WaitableVariable = mysql_harness::utility::WaitableVariable<ValueType>;

 public:
  Entry host_entry_;
  WaitableVariable<State> state_{State::Initializing};
  std::atomic<uint64_t> resolve_waiters_peak_{1};
  time_point creation_time_{steady_clock::now()};
};

}  // namespace host_cache

#endif  // MYSQLROUTER_HOST_CACHE_TEMPORARY_ENTRY_H_
