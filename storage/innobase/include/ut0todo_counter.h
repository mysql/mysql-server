/*****************************************************************************

Copyright (c) 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/
#pragma once

#include <cstddef>
#include "os0event.h"
#include "ut0dbg.h"

namespace ut {
/** A counter which tracks number of things left to do, which can be
incremented or decremented, and lets one await the value drop to zero.
Enforcing a total order on calls to increment(), decrement() and value() is
a responsibility of a user of this class.
With above assumption the await_zero() can be called safely at any moment,
as increment() and decrement() take care of resetting and setting the awaited
event object properly. */
class Todo_counter {
 public:
  /** Initializes the counter to 0. */
  Todo_counter() { os_event_set(m_is_zero); }

  /** Increments the value of the counter */
  void increment() {
    if (!m_todos++) {
      os_event_reset(m_is_zero);
    }
  }

  /** Decrements the value of the counter */
  void decrement() {
    ut_a(0 < m_todos);
    if (!--m_todos) {
      os_event_set(m_is_zero);
    }
  }

  /** Returns when the value is zero */
  void await_zero() { os_event_wait(m_is_zero); }

  /** Returns current value of the counter */
  size_t value() { return m_todos; }

 private:
  size_t m_todos{0};
  Os_event_t m_is_zero{};
};
}  // namespace ut