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

#include "mrs_component_table_access.h"

#include "components/mysql_rest_service/mrs_component.h"

namespace mrs_component {

Table_accessor::Table_accessor(MYSQL_THD thd, size_t max_tables) : thd_(thd) {
  ta_ = table_access_factory->create(thd_, max_tables);
}

Table_accessor::~Table_accessor() {
  if (ta_ != nullptr) {
    table_access->rollback(ta_);
    table_access_factory->destroy(ta_);
  }
}

Table_accessor Table_accessor::make_with_current_ctx(size_t max_tables) {
  MYSQL_THD thd = nullptr;
  if (mysql_service_mysql_current_thread_reader->get(&thd) != 0 ||
      (thd == nullptr)) {
    throw std::runtime_error("Failed to get current thread context");
  }
  return {thd, max_tables};
}

Table_accessor::Ticket Table_accessor::add_table_read(const char *schema,
                                                      const char *table) {
  return add_table(schema, table, TA_READ);
}

Table_accessor::Ticket Table_accessor::add_table_write(const char *schema,
                                                       const char *table) {
  return add_table(schema, table, TA_WRITE);
}

bool Table_accessor::begin() {
  if (!valid()) return false;

  return table_access->begin(ta_) == 0;
}

bool Table_accessor::materialize_tables(std::vector<Ticket> &tickets) {
  if (!valid()) return false;

  for (auto &tk : tickets) {
    if (tk.id == SIZE_MAX) return false;

    TA_table t = table_access->get(ta_, tk.id);

    if (t == nullptr) return false;
    tk.tbl = t;
  }
  return true;
}

bool Table_accessor::materialize_table(Ticket &ticket) {
  if (!valid() || ticket.id == SIZE_MAX) return false;

  ticket.tbl = table_access->get(ta_, ticket.id);
  return ticket.tbl != nullptr;
}

bool Table_accessor::scan(TA_table t,
                          const std::function<bool(TA_table)> &on_row) {
  if (!valid()) return false;

  if (table_access_scan->init(ta_, t) != 0) return false;

  bool ok = true;
  while (table_access_scan->next(ta_, t) == 0) {
    if (!on_row(t)) {
      ok = true;
      break;
    }  // stop scanning early
  }

  table_access_scan->end(ta_, t);
  return ok;
}

bool Table_accessor::get_varchar(TA_table t, int col, std::string &out) const {
  if (!valid() || (t == nullptr)) return false;

  my_h_string hs = nullptr;
  if (string_factory->create(&hs) != 0) return false;

  bool ok = false;
  do {
    if (field_varchar_access->get(ta_, t, col, hs) != 0) break;

    const char *buf = nullptr;
    size_t len = 0;
    CHARSET_INFO_h cs = nullptr;
    if (string_get_data_in_charset->get_data(hs, &buf, &len, &cs) != 0) break;

    // Allocate space for the null terminator to avoid truncation
    out.assign(len + 1, '\0');

    // Convert to "utf8mb4"
    if (string_converter->convert_to_buffer(hs, out.data(), out.size(),
                                            "utf8mb4") != 0) {
      break;
    }

    // Drop the trailing null and clear unused space
    out.resize(std::strlen(out.c_str()));

    ok = true;
  } while (false);

  string_factory->destroy(hs);
  return ok;
}

Table_accessor::Ticket Table_accessor::add_table(const char *schema,
                                                 const char *table,
                                                 enum TA_lock_type mode) {
  Ticket t{};
  if (!valid()) return t;

  const size_t ticket = table_access->add(ta_, schema, std::strlen(schema),
                                          table, std::strlen(table), mode);

  t.id = ticket;

  return t;
}

}  // namespace mrs_component
