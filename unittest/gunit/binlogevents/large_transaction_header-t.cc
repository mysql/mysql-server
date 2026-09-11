/* Copyright (c) 2026, Oracle and/or its affiliates.

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
#include <cstring>
#include <vector>

#include "my_byteorder.h"
#include "mysql/binlog/event/binlog_event.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/binlog/event/large_transaction_header_event.h"

namespace mysql::binlog::event::unittests {

class LargeTrxHeaderTest : public ::testing::Test {
 protected:
  LargeTrxHeaderTest() : m_fde(BINLOG_VERSION, "9.7.0") {}

  /**
    Build a serialized LTH event with a full fixed body (version, offset,
    terminal event type). The version byte is caller-supplied so that
    invalid-version cases can be exercised without truncating the body.
  */
  std::vector<char> build_event(uint8_t version, uint64_t offset,
                                uint8_t terminal_type, size_t padding_size) {
    const size_t event_size = LOG_EVENT_MINIMAL_HEADER_LEN +
                              Large_transaction_header_event::kFixedBodyLength +
                              padding_size;
    std::vector<char> buf(event_size, '\0');
    uchar *p = reinterpret_cast<uchar *>(buf.data());
    int4store(p, 1755000000);  // timestamp
    p[EVENT_TYPE_OFFSET] = LARGE_TRANSACTION_HEADER_EVENT;
    int4store(p + SERVER_ID_OFFSET, 1);
    int4store(p + EVENT_LEN_OFFSET, static_cast<uint32_t>(event_size));
    int4store(p + LOG_POS_OFFSET, 0);
    int2store(p + FLAGS_OFFSET, LOG_EVENT_IGNORABLE_F);
    uchar *body = p + LOG_EVENT_MINIMAL_HEADER_LEN;
    body[0] = version;
    int8store(body + 1, offset);
    body[Large_transaction_header_event::kFixedBodyLength - 1] = terminal_type;
    return buf;
  }

  Format_description_event m_fde;
};

TEST_F(LargeTrxHeaderTest, DecodeRoundtrip) {
  const uint64_t offsets[] = {0, 1, UINT32_MAX, 0x1122334455667788ULL,
                              UINT64_MAX};
  const size_t paddings[] = {0, 1, 4096, 128 * 1024};
  for (uint64_t offset : offsets) {
    for (size_t padding : paddings) {
      auto buf = build_event(Large_transaction_header_event::kVersion, offset,
                             static_cast<uint8_t>(XID_EVENT), padding);
      Large_transaction_header_event ev(buf.data(), &m_fde);
      ASSERT_TRUE(ev.header()->get_is_valid());
      EXPECT_EQ(ev.get_version(), Large_transaction_header_event::kVersion);
      EXPECT_EQ(ev.get_terminating_event_offset(), offset);
      EXPECT_EQ(ev.get_terminating_event_type(),
                static_cast<uint8_t>(XID_EVENT));
      EXPECT_EQ(ev.get_padding_size(), padding);
    }
  }
}

TEST_F(LargeTrxHeaderTest, IgnorableFlagIsPreserved) {
  auto buf = build_event(Large_transaction_header_event::kVersion, 42,
                         static_cast<uint8_t>(XID_EVENT), 16);
  Large_transaction_header_event ev(buf.data(), &m_fde);
  ASSERT_TRUE(ev.header()->get_is_valid());
  EXPECT_NE(ev.header()->flags & LOG_EVENT_IGNORABLE_F, 0);
}

TEST_F(LargeTrxHeaderTest, RejectsUnknownVersion) {
  for (uint8_t bad_version : {uint8_t{0}, uint8_t{2}, uint8_t{255}}) {
    auto buf =
        build_event(bad_version, 42, static_cast<uint8_t>(XID_EVENT), 16);
    Large_transaction_header_event ev(buf.data(), &m_fde);
    EXPECT_FALSE(ev.header()->get_is_valid());
  }
}

TEST_F(LargeTrxHeaderTest, RejectsTruncatedBody) {
  /* Body shorter than the fixed fields: version byte only. */
  auto buf = build_event(Large_transaction_header_event::kVersion, 42,
                         static_cast<uint8_t>(XID_EVENT), 0);
  buf.resize(LOG_EVENT_MINIMAL_HEADER_LEN + 1);
  uchar *p = reinterpret_cast<uchar *>(buf.data());
  int4store(p + EVENT_LEN_OFFSET, static_cast<uint32_t>(buf.size()));
  Large_transaction_header_event ev(buf.data(), &m_fde);
  EXPECT_FALSE(ev.header()->get_is_valid());
}

}  // namespace mysql::binlog::event::unittests
