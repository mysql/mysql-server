/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gtest/gtest.h>

#include <mysql.h>

#include "helper/mysql_column_types.h"

namespace {

constexpr unsigned int kBinaryCharset = 63;
constexpr unsigned int kUtf8mb4Charset = 255;

struct BlobTypeData {
  enum_field_types mysql_type;
  unsigned int charsetnr;
  const char *expected;
};

class BlobTypeTests : public testing::TestWithParam<BlobTypeData> {};

TEST_P(BlobTypeTests, preserves_blob_family_type_name) {
  MYSQL_FIELD field{};
  const auto &param = GetParam();
  field.type = param.mysql_type;
  field.charsetnr = param.charsetnr;

  EXPECT_EQ(param.expected, helper::txt_from_mysql_column_type(&field));
}

INSTANTIATE_TEST_SUITE_P(
    BlobTypes, BlobTypeTests,
    testing::Values(
        BlobTypeData{MYSQL_TYPE_TINY_BLOB, kBinaryCharset, "TINYBLOB"},
        BlobTypeData{MYSQL_TYPE_TINY_BLOB, kUtf8mb4Charset, "TINYTEXT"},
        BlobTypeData{MYSQL_TYPE_MEDIUM_BLOB, kBinaryCharset, "MEDIUMBLOB"},
        BlobTypeData{MYSQL_TYPE_MEDIUM_BLOB, kUtf8mb4Charset, "MEDIUMTEXT"},
        BlobTypeData{MYSQL_TYPE_LONG_BLOB, kBinaryCharset, "LONGBLOB"},
        BlobTypeData{MYSQL_TYPE_LONG_BLOB, kUtf8mb4Charset, "LONGTEXT"},
        BlobTypeData{MYSQL_TYPE_BLOB, kBinaryCharset, "BLOB"},
        BlobTypeData{MYSQL_TYPE_BLOB, kUtf8mb4Charset, "TEXT"}));

}  // namespace
