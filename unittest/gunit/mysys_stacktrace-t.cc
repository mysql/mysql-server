/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>
#include <stdio.h>
#include <sys/types.h>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_stacktrace.h"
#include "my_sys.h"
#include "scope_guard.h"

#ifdef HAVE_STACKTRACE

NO_INLINE
int function_one(const uchar *stack_bottom, const char *s) {
  fprintf(stderr, "%s", s);
  auto guard = create_scope_guard(
      [&]() { my_print_stacktrace(stack_bottom, my_thread_stack_size); });
  return 1;
}

TEST(Mysys, StackTrace) {
  uchar buf[1000]{};
  int ret = function_one(buf, "hello\n");
  EXPECT_EQ(1, ret);
}

#endif  // HAVE_STACKTRACE
