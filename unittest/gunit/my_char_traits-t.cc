/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
#include <string>

#include "my_char_traits.h"

namespace my_char_traits_unittest {

using MyTraits = my_char_traits<unsigned char>;
using StdTraits = std::char_traits<char>;

TEST(CharTraitsTest, Basic) {
  const unsigned char abc[] = "abc";
  EXPECT_EQ(3, MyTraits::length(abc));
  EXPECT_EQ(3, StdTraits::length("abc"));

  EXPECT_EQ('b', *MyTraits::find(abc, 3, 'b'));
  EXPECT_EQ('b', *StdTraits::find("abc", 3, 'b'));

  EXPECT_EQ(nullptr, MyTraits::find(abc, 3, 'd'));
  EXPECT_EQ(nullptr, StdTraits::find("abc", 3, 'd'));

  int xxx = MyTraits::not_eof('a');
  int yyy = StdTraits::not_eof('a');
  EXPECT_EQ(xxx, yyy);
  EXPECT_NE(0, xxx);

  EXPECT_EQ(MyTraits::eof(), StdTraits::eof());

  EXPECT_EQ(nullptr, MyTraits::move(nullptr, nullptr, 0));
  EXPECT_EQ(nullptr, StdTraits::move(nullptr, nullptr, 0));

  unsigned char a1 = MyTraits::to_char_type(0x61);
  unsigned char a2 = StdTraits::to_char_type(0x61);
  EXPECT_EQ(a1, a2);
  EXPECT_EQ('a', a1);
}

}  // namespace my_char_traits_unittest
