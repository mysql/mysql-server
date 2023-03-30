/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_HARNESS_ASSERT_INCLUDED
#define MYSQL_HARNESS_HARNESS_ASSERT_INCLUDED

#include <stdlib.h>

/** Improved assert()
 *
 * This macro is meant to provide analogous functionality to the well-known
 * assert() macro. In contrast to the original, it should also work in
 * release builds.
 */
#define harness_assert(COND) \
  if (!(COND)) abort()

/** assert(0) idiom with an explicit name
 *
 * This is essentially the assert(0) idiom, but with more explicit name
 * to clarify the intent.
 */
[[noreturn]] inline void harness_assert_this_should_not_execute() {
  harness_assert("If execution reached this line, you have a bug" == nullptr);
}

#endif /* MYSQL_HARNESS_HARNESS_ASSERT_INCLUDED */
