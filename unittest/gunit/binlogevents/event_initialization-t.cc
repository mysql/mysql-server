/* Copyright (c) 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with the
   program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <array>

#include <gtest/gtest.h>

#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/binlog/event/rows_event.h"

namespace mysql::binlog::event::unittests {

TEST(EventInitializationTest, LogEventHeaderDefaults) {
  Log_event_header header;

  EXPECT_EQ(0, header.when.tv_sec);
  EXPECT_EQ(0, header.when.tv_usec);
  EXPECT_EQ(ENUM_END_EVENT, header.type_code);
  EXPECT_EQ(0U, header.unmasked_server_id);
  EXPECT_EQ(0U, header.data_written);
  EXPECT_EQ(0U, header.log_pos);
  EXPECT_EQ(0U, header.flags);
  EXPECT_TRUE(header.get_is_valid());
}

TEST(EventInitializationTest, MalformedRowsEventHasDeterministicState) {
  const Format_description_event fde(BINLOG_VERSION, "8.0.0");
  std::array<char, LOG_EVENT_MINIMAL_HEADER_LEN> buffer{};
  buffer[EVENT_TYPE_OFFSET] = static_cast<char>(WRITE_ROWS_EVENT);
  buffer[EVENT_LEN_OFFSET] = static_cast<char>(buffer.size());

  Rows_event event(buffer.data(), &fde);

  EXPECT_FALSE(event.header()->get_is_valid());
  EXPECT_EQ(0U, event.get_table_id());
  EXPECT_EQ(0U, event.get_flags());
  EXPECT_EQ(0U, event.get_width());
  EXPECT_EQ(0U, event.get_null_bits_len());
}

}  // namespace mysql::binlog::event::unittests
