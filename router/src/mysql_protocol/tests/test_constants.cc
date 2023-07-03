/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "mysqlrouter/mysql_protocol.h"

/**
 * @file
 * @brief Tests to test mysql_protocol Capabilities flags
 */

using namespace mysql_protocol;

class MySQLProtocolCapabilitiesTest : public ::testing::Test {};

/**
 * @test test constructors
 */
TEST_F(MySQLProtocolCapabilitiesTest, constructor) {
  {
    Capabilities::Flags cap;
    EXPECT_EQ(0U, cap.bits());
  }
  {
    Capabilities::Flags cap(0x1234);
    EXPECT_EQ(0x1234U, cap.bits());
  }
  {
    Capabilities::Flags cap(Capabilities::LONG_PASSWORD);
    EXPECT_EQ(Capabilities::LONG_PASSWORD.bits(), cap.bits());
  }
}

/**
 * @test test operator=()
 */
TEST_F(MySQLProtocolCapabilitiesTest, assignment) {
  {
    Capabilities::Flags cap;
    cap = Capabilities::Flags(0x1234);
    EXPECT_EQ(0x1234U, cap.bits());
  }
  {
    Capabilities::Flags cap;
    cap = Capabilities::LONG_PASSWORD;
    EXPECT_EQ(Capabilities::LONG_PASSWORD.bits(), cap.bits());
  }
}

/**
 * @test test operator==() and operator!=()
 */
TEST_F(MySQLProtocolCapabilitiesTest, comparison) {
  Capabilities::Flags cap1(Capabilities::LONG_PASSWORD);
  Capabilities::Flags cap2(Capabilities::FOUND_ROWS);

  EXPECT_TRUE(cap1 == cap1);
  EXPECT_TRUE(cap1 != cap2);
  EXPECT_FALSE(cap1 != cap1);
  EXPECT_FALSE(cap1 == cap2);
}

/**
 * @test test methods that modify flags
 */
TEST_F(MySQLProtocolCapabilitiesTest, write) {
  using namespace Capabilities;

  Flags cap(LONG_PASSWORD | FOUND_ROWS | LONG_FLAG | CONNECT_WITH_DB);

  cap.clear(FOUND_ROWS | LONG_FLAG);
  EXPECT_EQ(LONG_PASSWORD | CONNECT_WITH_DB, cap);

  cap.set(PLUGIN_AUTH | DEPRECATE_EOF);
  EXPECT_EQ(LONG_PASSWORD | CONNECT_WITH_DB | PLUGIN_AUTH | DEPRECATE_EOF, cap);

  {
    Flags cap2 = cap;
    cap2.clear_low_16_bits();
    EXPECT_EQ(PLUGIN_AUTH | DEPRECATE_EOF, cap2);
  }

  {
    Flags cap2 = cap;
    cap2.clear_high_16_bits();
    EXPECT_EQ(LONG_PASSWORD | CONNECT_WITH_DB, cap2);
  }

  cap.reset();
  EXPECT_EQ(ALL_ZEROS, cap);

  Flags cap1(FOUND_ROWS | LONG_FLAG | PLUGIN_AUTH | DEPRECATE_EOF);
  Flags cap2(LONG_PASSWORD | LONG_FLAG | CONNECT_WITH_DB | DEPRECATE_EOF);
  EXPECT_EQ(LONG_FLAG | DEPRECATE_EOF, cap1 & cap2);
}

/**
 * @test test methods that return flags
 */
TEST_F(MySQLProtocolCapabilitiesTest, read) {
  using namespace Capabilities;

  Capabilities::Flags cap(LONG_PASSWORD | NO_SCHEMA |
                          SSL |  // these are in low bits
                          MULTI_STATEMENTS | CONNECT_ATTRS |
                          DEPRECATE_EOF);  // these are in high bits

  // test for one bit at a time
  EXPECT_TRUE(cap.test(LONG_PASSWORD));
  EXPECT_TRUE(cap.test(SSL));
  EXPECT_TRUE(cap.test(CONNECT_ATTRS));
  EXPECT_FALSE(cap.test(LONG_FLAG));
  EXPECT_FALSE(cap.test(FOUND_ROWS));

  // test for many bits at a time
  EXPECT_TRUE(cap.test(LONG_PASSWORD | SSL |
                       CONNECT_ATTRS));  // testing subset of set bits
  EXPECT_FALSE(cap.test(LONG_PASSWORD | SSL | CONNECT_ATTRS |
                        LONG_FLAG));  // LONG_FLAG not set

  // test low/high bits
  EXPECT_EQ((LONG_PASSWORD | NO_SCHEMA | SSL).bits(),
            static_cast<AllFlags>(cap.low_16_bits()));
  EXPECT_EQ((MULTI_STATEMENTS | CONNECT_ATTRS | DEPRECATE_EOF).bits(),
            static_cast<AllFlags>(cap.high_16_bits() << 16));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
