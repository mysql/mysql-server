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

#ifndef UNITTEST_LIBS_SETS_TEST_ONE_CONTAINER_H
#define UNITTEST_LIBS_SETS_TEST_ONE_CONTAINER_H

#include <gtest/gtest.h>                          // ASSERT_TRUE
#include <algorithm>                              // move
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "set_assertions.h"                       // assert_equal_sets

namespace unittest::libs::sets {

/// Exercise container operations (read/write) that are common to all set types,
/// and operate on only one set. This includes `clear`, and
/// assign/inplace_union/inplace_intersect/inplace_subtract with the set itself
/// as the operand.
///
/// @tparam Cont_t Type of the container to test.
///
/// @param cont The container to test.
template <class Cont_t>
void test_one_container(const Cont_t &cont) {
  MY_SCOPED_TRACE("test_one_container");

  // self-assign and clear
  {
    Cont_t cont1;
    auto ret = mysql::utils::void_to_ok([&] { return cont1.assign(cont); });
    ASSERT_OK(ret);

    ret = mysql::utils::void_to_ok([&] { return cont1.assign(cont1); });
    ASSERT_OK(ret);
    assert_equal_sets(cont1, cont);

    ret = mysql::utils::void_to_ok(
        [&] { return cont1.assign(std::move(cont1)); });
    ASSERT_OK(ret);
    assert_equal_sets(cont1, cont);

    cont1.clear();
    ASSERT_TRUE(cont1.empty());
    ASSERT_FALSE((bool)cont1);
    ASSERT_EQ(cont1.size(), 0);
  }

  // self-inplace_union, self-inplace_intersect, and self-inplace_subtract
  {
    // initialize cont1
    Cont_t cont1;
    auto ret = mysql::utils::void_to_ok([&] { return cont1.assign(cont); });
    ASSERT_OK(ret);

    // self-inplace_union is a no-op
    ret = mysql::utils::void_to_ok([&] { return cont1.inplace_union(cont1); });
    ASSERT_OK(ret);
    assert_equal_sets(cont1, cont);

    // should not alter the parameter despite being rvalue reference
    ret = mysql::utils::void_to_ok(
        [&] { return cont1.inplace_union(std::move(cont1)); });
    ASSERT_OK(ret);
    assert_equal_sets(cont1, cont);

    // self-inplace_intersect is a no-op
    ret = mysql::utils::void_to_ok(
        [&] { return cont1.inplace_intersect(cont1); });
    ASSERT_OK(ret);
    assert_equal_sets(cont1, cont);

    // should not alter the parameter despite being rvalue reference
    ret = mysql::utils::void_to_ok(
        [&] { return cont1.inplace_intersect(std::move(cont1)); });
    ASSERT_OK(ret);
    assert_equal_sets(cont1, cont);

    // self-inplace_subtract is equivalent to clear
    ret =
        mysql::utils::void_to_ok([&] { return cont1.inplace_subtract(cont1); });
    ASSERT_OK(ret);
    ASSERT_TRUE(cont1.empty());
    ASSERT_FALSE((bool)cont1);
    ASSERT_EQ(cont1.size(), 0);

    // restore cont1
    ret = mysql::utils::void_to_ok([&] { return cont1.assign(cont); });
    ASSERT_OK(ret);

    // should not alter the parameter despite being rvalue reference
    ret = mysql::utils::void_to_ok(
        [&] { return cont1.inplace_subtract(std::move(cont1)); });
    ASSERT_OK(ret);
    ASSERT_TRUE(cont1.empty());
    ASSERT_FALSE((bool)cont1);
    ASSERT_EQ(cont1.size(), 0);
  }
}

}  // namespace unittest::libs::sets

#endif  // ifndef UNITTEST_LIBS_SETS_TEST_ONE_CONTAINER_H
