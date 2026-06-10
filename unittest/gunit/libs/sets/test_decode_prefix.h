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

#ifndef UNITTEST_LIBS_SETS_TEST_DECODE_PREFIX_H
#define UNITTEST_LIBS_SETS_TEST_DECODE_PREFIX_H

#include <gtest/gtest.h>                      // ASSERT_TRUE
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/strconv/strconv.h"            // decode

namespace unittest::libs::sets {

/// Convert the set to string. For each prefix of that string, try to decode it,
/// and assert that the decoding failed.
///
/// @tparam Cont_t Type of the container to test.
///
/// @param cont The container to test.
///
/// @param format Encode/decode format. Default is `Binary_format{}`.
template <class Cont_t>
void test_decode_prefix(const Cont_t &cont,
                        const mysql::strconv::Is_format auto &format =
                            mysql::strconv::Binary_format{}) {
  auto str = mysql::strconv::throwing::encode(format, cont);
  for (std::size_t len = 0; len != str.size(); ++len) {
    std::string_view prefix(str.data(), len);
    Cont_t out_set;
    auto ret = mysql::strconv::decode(format, prefix, out_set);
    ASSERT_FALSE(ret.is_ok()) << prefix;
  }
}

}  // namespace unittest::libs::sets

#endif  // ifndef UNITTEST_LIBS_SETS_TEST_DECODE_PREFIX_H
