/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#ifndef TABLE_BASE_INCLUDED
#define TABLE_BASE_INCLUDED

/* Common header for all Health Monitor tables. */

#include "component_table.h"
#include "health_monitor_constants.h"

namespace hm {

struct table_base_pos {
  table_base_pos() : m_index{0} {}
  void reset() { m_index = 0; }
  void set_at(const table_base_pos *other) { m_index = other->m_index; }
  void set_after(const table_base_pos *other) { m_index = other->m_index + 1; }
  // Record id, zero-based
  unsigned int m_index;
};

class table_base {
 public:
  typedef table_base_pos pos_t;

  table_base() = default;
  virtual ~table_base() = default;

  table_base(const table_base &) = default;
  table_base &operator=(const table_base &) = default;
  table_base(table_base &&) = default;
  table_base &operator=(table_base &&) = default;

  virtual int rnd_init(bool /*scan*/) { return 0; }
  virtual int rnd_next() = 0;
  virtual int rnd_pos() = 0;
  virtual void reset_position() { m_pos.reset(); }
  virtual int read_column_value(PSI_field *field, unsigned int index) = 0;
  virtual void close_table() {}

  /* C interface */
  static int rnd_init(PSI_table_handle *handle, bool /*scan*/);
  static int rnd_next(PSI_table_handle *handle);
  static int rnd_pos(PSI_table_handle *handle);
  static void reset_position(PSI_table_handle *handle);
  static int read_column_value(PSI_table_handle *handle, PSI_field *field,
                               unsigned int index);
  static void close_table(PSI_table_handle *handle);

  pos_t m_pos;
  pos_t m_next_pos;
};

}  // namespace hm

#endif  // TABLE_BASE_INCLUDED