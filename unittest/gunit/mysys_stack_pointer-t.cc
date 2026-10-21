/* Copyright (c) 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <gtest/gtest.h>
#include <cstdio>

#include "my_sys.h"

#if defined(HAVE_ASAN) && (defined(__GNUC__) || defined(__clang__))
#include <sanitizer/asan_interface.h>
#endif

/**
  Tests resolve_stack_pointer(), which supplies stack addresses for
  MySQL's stack-overrun accounting.

  Normal builds must preserve a caller's conventional local stack
  marker. In GCC and Clang ASan builds, a local marker can reside on
  ASan's fake stack; the resolver must return a physical-stack address
  instead. The ASan-specific assertions are conditional because
  fake-stack/use-after-return mode is a runtime option and may not be
  enabled for every test invocation.
*/
namespace {

TEST(MyStackPointerTest, ResolvesFakeStackMarker) {
  char stack_marker = 0;
  const char *const resolved_stack_pointer =
      resolve_stack_pointer(&stack_marker);

#if defined(HAVE_ASAN) && (defined(__GNUC__) || defined(__clang__))
  void *fake_stack = __asan_get_current_fake_stack();
  void *frame_begin = nullptr;
  void *frame_end = nullptr;
  const bool marker_is_on_fake_stack =
      fake_stack != nullptr &&
      __asan_addr_is_in_fake_stack(fake_stack, &stack_marker, &frame_begin,
                                   &frame_end) != nullptr;

  char resolved_name[128];
  void *resolved_region = nullptr;
  size_t resolved_region_size = 0;
  const char *const resolved_kind = __asan_locate_address(
      const_cast<char *>(resolved_stack_pointer), resolved_name,
      sizeof(resolved_name), &resolved_region, &resolved_region_size);

  // printf("resolve_stack_pointer: ASan %s fake-stack marker "
  //        "(marker=%p, resolved=%p)\n",
  //        marker_is_on_fake_stack ? "used" : "did not use",
  //        static_cast<const void *>(&stack_marker),
  //        static_cast<const void *>(resolved_stack_pointer));

  // ASan builds must always use a physical-stack address for
  // accounting.
  EXPECT_STREQ("stack", resolved_kind);

  // A relocated marker must be replaced with a physical-stack address.
  EXPECT_TRUE(!marker_is_on_fake_stack ||
              resolved_stack_pointer != &stack_marker);
#else
  // printf("resolve_stack_pointer: non-ASan marker path "
  //        "(marker=%p, resolved=%p)\n",
  //        static_cast<const void *>(&stack_marker),
  //        static_cast<const void *>(resolved_stack_pointer));
  EXPECT_EQ(&stack_marker, resolved_stack_pointer);
#endif
}

}  // namespace
