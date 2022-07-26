/* Copyright (c) 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/xa.h"
#include "sql/xa/xid_extract.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace xid_extract_unittests {

class XID_extract_test : public ::testing::Test {
 protected:
  XID_extract_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

static void expect_valid(xa::XID_extractor &tokenizer,
                         std::string const &to_validate, char const *gtrid,
                         size_t gtrid_len, char const *bqual, size_t bqual_len,
                         int64_t format_id) {
  EXPECT_EQ(1, tokenizer.extract(to_validate));

  if (tokenizer.size() != 0) {
    XID valid;
    valid.set(format_id, gtrid, gtrid_len, bqual, bqual_len);
    std::ostringstream oss;
    oss << valid << std::flush;
    std::string valid_str = oss.str();
    oss.str("");
    oss << tokenizer[0] << std::flush;
    std::string to_validate_str = oss.str();

    EXPECT_EQ(valid, tokenizer[0]);
    EXPECT_EQ(valid_str, to_validate_str);
  }
}

TEST_F(XID_extract_test, Input_Output_test) {
  std::string extract_from{
      "X'feedaaaa0000',X'beefaaaa0000',2021 XA COMMIT X'32adfe873947' "
      ",\tX'2aaddef767782001'\n,\n1; XA "
      "ROLLBACK X'23' , X'12'\n,\t1;X'as X' "
      "\n something something X'',X'',1 X'' got it"};

  xa::XID_extractor tokenizer{extract_from};
  EXPECT_EQ(4, tokenizer.size());
  for (auto token : tokenizer) {
    EXPECT_EQ(false, token.is_null());
  }

  std::string hex64;
  std::string str64;
  for (int i = 0; i < 64; ++i) {
    str64 += '\0';
    hex64 += "00";
  }

  // normal xid
  expect_valid(tokenizer, "X'aABb' , X'1234',  2", "\xaa\xbb", 2, "\x12\x34", 2,
               2);
  // zero length strings
  expect_valid(tokenizer, "X'',X'',2", "", 0, "", 0, 2);
  // whitespace
  expect_valid(tokenizer, " X'' , X'' , 1 ", "", 0, "", 0, 1);
  // zero-valued bytes
  expect_valid(tokenizer, "X'00', X'0000', 1", "\x00", 1, "\x00\x00", 2, 1);
  // 64 byte length
  expect_valid(tokenizer, "X'" + hex64 + "', X'" + hex64 + "', 1", str64.data(),
               64, str64.data(), 64, 1);
  // formatid 0
  expect_valid(tokenizer, "X'', X'', 0", "", 0, "", 0, 0);
  // formatid 2^31-1
  expect_valid(tokenizer, "X'', X'', 2147483647", "", 0, "", 0,
               std::numeric_limits<int32_t>::max());

  // xid_t::operator!= test
  XID xid;
  xid.set(2, "", 0, "", 0);
  tokenizer.extract("X'',X'',1");
  EXPECT_TRUE(xid != tokenizer[0]);

  // ivalid formatid
  EXPECT_EQ(0, tokenizer.extract("X'', X'', MYSQL"));
  // out of range formatid
  EXPECT_EQ(0, tokenizer.extract("X'', X'', -1"));
  EXPECT_EQ(0, tokenizer.extract("X'', X'', -2"));
  EXPECT_EQ(0, tokenizer.extract("X'', X'', 9223372036854775808"));
  EXPECT_EQ(0, tokenizer.extract("X'', X'', 10000000000000000000"));
  EXPECT_EQ(0, tokenizer.extract("X'', X'', 20000000000000000000"));
  // whitespace in hex string
  EXPECT_EQ(0, tokenizer.extract("X'  a 1', X'', 1"));
  EXPECT_EQ(0, tokenizer.extract("X' ', X'', 1"));
  // whitespace between X and '
  EXPECT_EQ(0, tokenizer.extract("X 'a1', X'', 1"));
  EXPECT_EQ(0, tokenizer.extract("X'a1', X '', 1"));
  // garbage between strings
  EXPECT_EQ(0, tokenizer.extract("XA COMMIT X'32' hello! ,X'32' ['world'],2'"));
  // missing commas
  EXPECT_EQ(0, tokenizer.extract("X'' X'' 1"));
  // odd-length hex strings
  EXPECT_EQ(0, tokenizer.extract("X'0', X'', 1"));
  EXPECT_EQ(0, tokenizer.extract("X'', X'123', 1"));
  // missing commas
  EXPECT_EQ(0, tokenizer.extract("XA COMMIT X'32' X'32' 2"));
  // garbage and extra commas between strings
  EXPECT_EQ(0, tokenizer.extract("XA COMMIT X'32', X, X'32', 2"));
  // garbage and extra non-hex strings between strings
  EXPECT_EQ(0, tokenizer.extract("XA COMMIT X'32', 'foo', X'32', 2"));
  // Too long string
  EXPECT_EQ(0, tokenizer.extract("X'" + hex64 + "01', X'', 1"));
  EXPECT_EQ(0, tokenizer.extract("X'', X'" + hex64 + "01', 1"));
  EXPECT_EQ(0, tokenizer.extract("X'" + hex64 + hex64 + hex64 + hex64 +
                                 "01', X'', 1"));
  EXPECT_EQ(0, tokenizer.extract("X'', X'" + hex64 + hex64 + hex64 + hex64 +
                                 "01', 1"));
}

}  // namespace xid_extract_unittests
