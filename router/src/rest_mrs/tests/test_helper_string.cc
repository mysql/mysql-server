/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "helper/string/hex.h"

using namespace helper::string;

TEST(helper_string, hex_c_array_one_byte_with_zeros1) {
  uint8_t buffer[1]{0x0A};
  ASSERT_EQ("0a", hex(buffer));
}

TEST(helper_string, hex_c_array_one_byte_with_zeros2) {
  uint8_t buffer[1]{0xA0};
  ASSERT_EQ("a0", hex(buffer));
}

TEST(helper_string, hex_c_array_one_byte) {
  uint8_t buffer[1]{0xAA};
  ASSERT_EQ("aa", hex(buffer));
}

TEST(helper_string, hex_c_array_several_bytes) {
  uint8_t buffer[3]{0xAA, 0xcd, 0x12};
  ASSERT_EQ("aacd12", hex(buffer));
}
