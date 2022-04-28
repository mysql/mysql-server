/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "my_ref_counted.h"

/** default constructor */
my_ref_counted::my_ref_counted() : m_count(0) {}

/** copy constructor */
my_ref_counted::my_ref_counted(my_ref_counted &other)
    : m_count(other.m_count.load()) {}

/* virtual destructor */
my_ref_counted::~my_ref_counted() = default;

/**
  Increases a reference count.

  @return old value
*/
uint64 my_ref_counted::add_reference() { return m_count++; }

/**
  Decreases a reference count.
  Doesn't allow to decrease below 0.

  @param [out] new_count Pointer to variable to store new count. May be NULL.
  @return Status of performed operation
  @retval false success
  @retval true Failure. Will be returned in case counter is already 0.
*/
bool my_ref_counted::release_reference(uint64 *new_count) {
  uint64 old_val = m_count;
  do {
    if (old_val == 0) {
      return true;
    }
  } while (!m_count.compare_exchange_weak(old_val, old_val - 1));

  if (new_count != nullptr) {
    *new_count = old_val - 1;
  }
  return false;
}

/**
  Returns the reference counter value.

  @return current value
*/
uint64 my_ref_counted::get_reference_count() const { return m_count; }
