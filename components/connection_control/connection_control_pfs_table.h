/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef CONNECTION_CONTROL_PFS_TABLE_H
#define CONNECTION_CONTROL_PFS_TABLE_H

#include <mysql/components/services/pfs_plugin_table_service.h>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "connection_control_memory.h"

namespace connection_control {
bool register_pfs_table();
bool unregister_pfs_table();

template <typename T>
class CustomAllocator : public Connection_control_alloc {
 public:
  using value_type = T;

  CustomAllocator() = default;

  template <typename U>
  explicit CustomAllocator(const CustomAllocator<U> &) {}

  // Allocate memory
  T *allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      throw std::bad_alloc();
    }
    // Use ::operator new with proper alignment for T
    T *temp = static_cast<T *>(operator new(n * sizeof(T)));
    if (temp == nullptr) throw std::bad_alloc();
    return temp;
  }

  // Deallocate memory
  void deallocate(T *ptr, std::size_t) {
    // Use ::operator delete with alignment
    operator delete(ptr);
  }
};

// Stores row data for
// performance_schema.connection_control_failed_login_attempts table
class Connection_control_pfs_table_data_row {
 public:
  // Constructor taking parameters
  Connection_control_pfs_table_data_row(const std::string &userhost,
                                        const PSI_ulong &failed_attempts);
  std::string m_userhost;
  PSI_ulong m_failed_attempts;
};

typedef std::vector<Connection_control_pfs_table_data_row,
                    CustomAllocator<Connection_control_pfs_table_data_row>>
    Connection_control_pfs_table_data;

}  // namespace connection_control

#endif /* CONNECTION_CONTROL_PFS_TABLE_H */
