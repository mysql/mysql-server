/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include <gtest/gtest.h>

#include "mysqlx_protocol.h"


namespace test
{

TEST(mysqlx_protocol, receive_invalid_message)
{
  const std::size_t empty_payload_length = 0;

  mysqlx::XProtocol protocol(
      mysqlx::Ssl_config(),
      0,     // timeout
      false, // dont_wait_for_disconnect
      mysqlx::IP_any);

  /*
   Mysqlx::Notice::Frame message has one or more required fields.
   Reception of message with empty payload, fails the check for
   required fields which is thrown below as mysqlx::Error.
   */
  EXPECT_THROW(
      protocol.recv_payload(
          Mysqlx::ServerMessages_Type_NOTICE,
          empty_payload_length),
      mysqlx::Error);
}

} // namespace test
