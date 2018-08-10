/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <string.h>
#include <cstdlib>
#include <cstring>

#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::NotNull;
using std::string;

using namespace mysql_protocol;

class MySQLProtocolTest : public ::testing::Test {
 public:
  mysql_protocol::Packet::vector_t case_w_sqlstate = {
      0x1d, 0x00, 0x00, 0x00, 0xff, 0x9f, 0x0f, 0x23, 0x58, 0x59, 0x31,
      0x32, 0x33, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61,
      0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72,
  };

  mysql_protocol::Packet::vector_t case_wo_sqlstate = {
      0x17, 0x00, 0x00, 0x00, 0xff, 0x9f, 0x0f, 0x54, 0x68,
      0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74,
      0x65, 0x73, 0x74, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72,
  };

 protected:
  virtual void SetUp() {}
};

TEST_F(MySQLProtocolTest, Constructor) {
  string msg = "This is a test error";
  uint16_t code = 3999;

  auto error_packet = mysql_protocol::ErrorPacket(0, code, msg, "XY123");

  ASSERT_EQ(0U, error_packet.get_capabilities().bits());
  ASSERT_EQ(case_wo_sqlstate.size(), error_packet.size());
  ASSERT_THAT(error_packet, ContainerEq(case_wo_sqlstate));
}

TEST_F(MySQLProtocolTest, ConstructorBufferCapabilities) {
  {
    // Without SQL State; CLIENT_PROTOCOL_41 capability flag not set
    auto p = mysql_protocol::ErrorPacket(case_wo_sqlstate);

    ASSERT_EQ(0U, p.get_capabilities().bits());
    ASSERT_EQ(case_wo_sqlstate.size(), p.size());
    ASSERT_THAT(p, ContainerEq(case_wo_sqlstate));
    ASSERT_EQ("", p.get_sql_state());
    ASSERT_EQ("This is a test error", p.get_message());
  }

  {
    // With SQL State; CLIENT_PROTOCOL_41 capability flag set
    auto p =
        mysql_protocol::ErrorPacket(case_w_sqlstate, Capabilities::PROTOCOL_41);

    ASSERT_EQ(Capabilities::PROTOCOL_41, p.get_capabilities());
    ASSERT_EQ(case_w_sqlstate.size(), p.size());
    ASSERT_THAT(p, ContainerEq(case_w_sqlstate));
    ASSERT_EQ("XY123", p.get_sql_state());
    ASSERT_EQ("This is a test error", p.get_message());
  }

  {
    // With SQL State; CLIENT_PROTOCOL_41 capability flag not set
    auto p = mysql_protocol::ErrorPacket(case_w_sqlstate);

    ASSERT_EQ(0U, p.get_capabilities().bits());
    ASSERT_EQ(case_w_sqlstate.size(), p.size());
    ASSERT_THAT(p, ContainerEq(case_w_sqlstate));
    ASSERT_EQ("XY123", p.get_sql_state());
    ASSERT_EQ("This is a test error", p.get_message());
  }
}

TEST_F(MySQLProtocolTest, ConstructorWithCapabilities) {
  string msg = "This is a test error";
  uint16_t code = 3999;

  auto error_packet = mysql_protocol::ErrorPacket(0, code, msg, "XY123",
                                                  Capabilities::PROTOCOL_41);

  ASSERT_EQ(error_packet.get_capabilities(), Capabilities::PROTOCOL_41);
  ASSERT_EQ(case_w_sqlstate.size(), error_packet.size());
  ASSERT_THAT(error_packet, ContainerEq(case_w_sqlstate));
}

TEST_F(MySQLProtocolTest, ParsePayloadErrors) {
  {
    // One byte missing; payload size incorrect
    auto buffer = mysql_protocol::Packet::vector_t(case_w_sqlstate.begin(),
                                                   case_w_sqlstate.end() - 1);

    ASSERT_THROW({ mysql_protocol::ErrorPacket e(buffer); },
                 mysql_protocol::packet_error);
    try {
      mysql_protocol::ErrorPacket e(buffer);
    } catch (const mysql_protocol::packet_error &exc) {
      ASSERT_THAT(exc.what(), HasSubstr("Incorrect payload size"));
    }
  }

  {
    // 0xff not found as 5th byte
    auto buffer = case_w_sqlstate;
    buffer[4] = 0xfe;
    ASSERT_THROW({ mysql_protocol::ErrorPacket e(buffer); },
                 mysql_protocol::packet_error);
    try {
      mysql_protocol::ErrorPacket e(buffer);
    } catch (const mysql_protocol::packet_error &exc) {
      ASSERT_THAT(exc.what(), HasSubstr("Error packet marker 0xff not found"));
    }
  }

  {
    // SQLState should be present
    std::vector<uint8_t> buffer = {
        0x17, 0x00, 0x00, 0x00, 0xff, 0x9f, 0x0f, 0x54, 0x68,
        0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74,
        0x65, 0x73, 0x74, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72,
    };

    ASSERT_THROW(
        { mysql_protocol::ErrorPacket e(buffer, Capabilities::PROTOCOL_41); },
        mysql_protocol::packet_error);
    try {
      mysql_protocol::ErrorPacket e(buffer, Capabilities::PROTOCOL_41);
    } catch (const mysql_protocol::packet_error &exc) {
      ASSERT_THAT(exc.what(),
                  HasSubstr("Error packet does not contain SQL state"));
    }
  }
}
