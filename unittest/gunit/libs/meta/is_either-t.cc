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

#include <gtest/gtest.h>           // TEST
#include "mysql/meta/is_either.h"  // Is_either

// Verify at compile time that Is_either works as expected.
//
// There is nothing to execute, this is a compile-time-only test.

using namespace mysql::meta;

static_assert(Is_either<int, int>);
static_assert(Is_either<int, int, int>);
static_assert(Is_either<int, const int &&, int>);
static_assert(Is_either<int, float, int>);
static_assert(Is_either<int, int, float>);
static_assert(Is_either<int, float, double, char, int>);

static_assert(!Is_either<int>);
static_assert(!Is_either<int, float>);
static_assert(!Is_either<int, float, double, char>);
static_assert(!Is_either<int, int &, const int, int *, int &&, int[], int[3]>);

TEST(LibsMetaIsEither, Basic) {}
