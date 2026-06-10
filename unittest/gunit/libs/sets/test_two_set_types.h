// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef UNITTEST_LIBS_SETS_TEST_TWO_SET_TYPES_H
#define UNITTEST_LIBS_SETS_TEST_TWO_SET_TYPES_H

#include "mysql/sets/sets.h"  // Union_view

namespace unittest::libs::sets {

template <class Set1_t, class Set2_t>
void test_two_set_types() {
  auto empty_set_view = mysql::sets::make_empty_set_view_like<Set1_t>();
  using Empty_t = decltype(empty_set_view);
  static_assert(mysql::sets::Is_compatible_set<Empty_t, Set2_t>);

  ASSERT_EQ((mysql::sets::Union_view<Set1_t, Set2_t>(nullptr, nullptr)),
            empty_set_view);
  ASSERT_EQ((mysql::sets::Intersection_view<Set1_t, Set2_t>(nullptr, nullptr)),
            empty_set_view);
  ASSERT_EQ((mysql::sets::Subtraction_view<Set1_t, Set2_t>(nullptr, nullptr)),
            empty_set_view);

  ASSERT_EQ(
      (mysql::sets::Union_view<Empty_t, Set2_t>(&empty_set_view, nullptr)),
      empty_set_view);
  ASSERT_EQ((mysql::sets::Intersection_view<Empty_t, Set2_t>(&empty_set_view,
                                                             nullptr)),
            empty_set_view);
  ASSERT_EQ((mysql::sets::Subtraction_view<Empty_t, Set2_t>(&empty_set_view,
                                                            nullptr)),
            empty_set_view);

  ASSERT_EQ(
      (mysql::sets::Union_view<Set1_t, Empty_t>(nullptr, &empty_set_view)),
      empty_set_view);
  ASSERT_EQ((mysql::sets::Intersection_view<Set1_t, Empty_t>(nullptr,
                                                             &empty_set_view)),
            empty_set_view);
  ASSERT_EQ((mysql::sets::Subtraction_view<Set1_t, Empty_t>(nullptr,
                                                            &empty_set_view)),
            empty_set_view);

  ASSERT_EQ((mysql::sets::Union_view<Empty_t, Empty_t>(&empty_set_view,
                                                       &empty_set_view)),
            empty_set_view);
  ASSERT_EQ((mysql::sets::Intersection_view<Empty_t, Empty_t>(&empty_set_view,
                                                              &empty_set_view)),
            empty_set_view);
  ASSERT_EQ((mysql::sets::Subtraction_view<Empty_t, Empty_t>(&empty_set_view,
                                                             &empty_set_view)),
            empty_set_view);
}

}  // namespace unittest::libs::sets

#endif  // ifndef UNITTEST_LIBS_SETS_TEST_TWO_SET_TYPES_H
