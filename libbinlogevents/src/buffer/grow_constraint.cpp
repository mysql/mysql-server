/* Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <buffer/grow_constraint.h>

#include <cassert>

namespace mysqlns::buffer {

void Grow_constraint::set_max_size(Size_t max_size) { m_max_size = max_size; }

Grow_constraint::Size_t Grow_constraint::get_max_size() const {
  return m_max_size;
}

void Grow_constraint::set_grow_factor(double grow_factor) {
  assert(grow_factor >= 1.0);
  m_grow_factor = grow_factor;
}

double Grow_constraint::get_grow_factor() const { return m_grow_factor; }

void Grow_constraint::set_grow_increment(Size_t grow_increment) {
  assert(grow_increment > 0);
  m_grow_increment = grow_increment;
}

Grow_constraint::Size_t Grow_constraint::get_grow_increment() const {
  return m_grow_increment;
}

void Grow_constraint::set_block_size(Size_t block_size) {
  m_block_size = block_size;
}

Grow_constraint::Size_t Grow_constraint::get_block_size() const {
  return m_block_size;
}

}  // namespace mysqlns::buffer
