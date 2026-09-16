// Copyright (c) 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_SCHEDULER_CONSTANTS_H
#define MYSQL_SCHEDULER_CONSTANTS_H

#include <string>

namespace mysql::scheduler {

struct Constants {
  /// The maximum number of scheduler instances supported
  /// This value should be aligned with the 'MAX_CHANNELS' defined in the server
  static constexpr inline unsigned int max_instances = 256;
  /// The maximum number of threads in the thread pool
  /// This value should be aligned with the MTS_MAX_WORKERS defined in the
  /// server
  static constexpr inline unsigned int max_thread_count = 1024;
  /// Scheduler thread id constant, used by the scheduler thread (max)
  static constexpr inline unsigned int scheduler_thread_id = max_thread_count;
  /// Invalid thread id constant, used by the scheduler thread
  static constexpr inline unsigned int invalid_thread_id =
      std::numeric_limits<unsigned int>::max();
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_CONSTANTS_H
