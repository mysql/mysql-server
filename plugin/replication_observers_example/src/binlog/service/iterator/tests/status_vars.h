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

#include <atomic>

#ifndef PLUGIN_PFS_STATUS_VARS_BINLOG_STORAGE_ITERATOR_TESTS_H_
#define PLUGIN_PFS_STATUS_VARS_BINLOG_STORAGE_ITERATOR_TESTS_H_

namespace binlog::service::iterators::tests {

/// @brief counts the number of reallocations done when the read buffer was not
///        large enough
extern std::atomic<uint64_t> global_status_var_count_buffer_reallocations;

/// @brief the sum of memory allocation requests
extern std::atomic<uint64_t> global_status_var_sum_buffer_size_requested;

/// @brief Registers the status variables.
/// @return false on success, true otherwise.
bool register_status_variables();

/// @brief unregisters the status variables.
/// @return false on success, true otherwise.
bool unregister_status_variables();

}  // namespace binlog::service::iterators::tests

#endif /*  */
