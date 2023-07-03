/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include "m_ctype.h"
#include "my_sys.h"

namespace charset_bug32788301_unittest {

CHARSET_INFO *init_collation(const char *name) {
  MY_CHARSET_LOADER loader;
  return my_collation_get_by_name(&loader, name, MYF(0));
}

TEST(CharsetBug32788301Unittest, LoadUninitLoad) {
  CHARSET_INFO *cs1 = init_collation("utf8mb4_ja_0900_as_cs");
  EXPECT_NE(nullptr, cs1);
  charset_uninit();
  CHARSET_INFO *cs2 = init_collation("utf8mb4_ja_0900_as_cs");
  EXPECT_EQ(cs1, cs2);
}

}  // namespace charset_bug32788301_unittest
