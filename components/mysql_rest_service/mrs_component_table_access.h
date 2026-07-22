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

#ifndef MRS_COMPONENT_TABLE_ACCESS_H
#define MRS_COMPONENT_TABLE_ACCESS_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mysql/components/services/bits/table_access_bits.h"

namespace mrs_component {

/**
  RAII wrapper around MySQL Table Access services.
  Handles:
  - session creation/destruction
  - table registration/open
  - simple row scanning
  - VARCHAR field getter
*/
class Table_accessor {
 public:
  struct Ticket {
    size_t id{SIZE_MAX};    // Ticket ID assigned by table_access
    TA_table tbl{nullptr};  // Filled after begin()
  };

  // Factory: create Table_accessor bound to current THD
  static Table_accessor make_with_current_ctx(size_t max_tables = 4);

  ~Table_accessor();

  Table_accessor() = delete;
  Table_accessor(const Table_accessor &) = delete;
  Table_accessor &operator=(const Table_accessor &) = delete;
  Table_accessor(Table_accessor &&) = delete;
  Table_accessor &operator=(Table_accessor &&) = delete;

  [[nodiscard]] bool valid() const { return ta_ != nullptr; }

  Ticket add_table_read(const char *schema, const char *table);
  Ticket add_table_write(const char *schema, const char *table);

  bool begin();
  bool materialize_tables(std::vector<Ticket> &tickets);
  bool materialize_table(Ticket &ticket);

  // Full table scan, calls on_row(table) for each row. Return false to stop it
  bool scan(TA_table t, const std::function<bool(TA_table)> &on_row);

  // VARCHAR getter
  bool get_varchar(TA_table t, int col, std::string &out) const;

 private:
  Table_access ta_{nullptr};
  MYSQL_THD thd_{nullptr};

  Table_accessor(MYSQL_THD thd, size_t max_tables);
  Ticket add_table(const char *schema, const char *table, TA_lock_type mode);
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_TABLE_ACCESS_H
