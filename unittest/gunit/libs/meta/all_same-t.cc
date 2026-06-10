// Copyright (c) 2023, 2026, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>                // TEST
#include "mysql/meta/is_same_as_all.h"  // Is_same_as_all

// Verify at compile time that Is_same_as_all works as expected.
//
// There is nothing to execute, this is a compile-time-only test.

using namespace mysql::meta;

// Apparently zero-argument concepts are disallowed on el7-arm-64bit gcc 10.1.
// Uncomment the following line if that changes.
// static_assert(Is_same_as_all<>);

static_assert(Is_same_as_all<int>);
static_assert(Is_same_as_all<int, int>);
static_assert(Is_same_as_all<int, int, int>);
static_assert(!Is_same_as_all<int, float>);
static_assert(!Is_same_as_all<float, int, int>);
static_assert(!Is_same_as_all<int, float, int>);
static_assert(!Is_same_as_all<int, int, float>);

TEST(LibsMetaAllSame, Basic) {}
