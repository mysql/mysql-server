/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0bool_scope_guard.h
 The ut_bool_scope_guard class which sets boolean to true for
 the duration of scope. */

#ifndef ut0bool_scope_guard_h
#define ut0bool_scope_guard_h

namespace ut {
/** A RAII-style class, which sets a given boolean to true in constructor, and
to false in destructor, effectively making sure that it is true for the duration
of the object lifetime/scope. */
class bool_scope_guard_t {
  /** boolean to be manipulated, or nullptr if the object was moved from, or
  already destructed */
  bool *m_active;

 public:
  /** Creates the RAII guard which sets `active` to true for the duration of
  its lifetime.
  @param[in,out] active  the boolean which is to be manipulated */
  explicit bool_scope_guard_t(bool &active) : m_active(&active) {
    *m_active = true;
  }
  ~bool_scope_guard_t() {
    if (m_active != nullptr) {
      *m_active = false;
      m_active = nullptr;
    }
  }
  bool_scope_guard_t(bool_scope_guard_t const &) = delete;
  bool_scope_guard_t &operator=(bool_scope_guard_t const &) = delete;
  bool_scope_guard_t &operator=(bool_scope_guard_t &&) = delete;
  bool_scope_guard_t(bool_scope_guard_t &&old) {
    m_active = old.m_active;
    old.m_active = nullptr;
  }
};
}  // namespace ut
#endif /* ut0bool_scope_guard_h */
