/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <cstdio> /* std::remove */
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "components/keyrings/common/data_file/reader.h"
#include "components/keyrings/common/data_file/writer.h"

namespace file_unittest {

class File_test : public ::testing::Test {};

using keyring_common::data_file::File_reader;
using keyring_common::data_file::File_writer;

TEST_F(File_test, FileWriteReadTest) {
  std::string data("Quick Brown Fox jumped over the lazy dog.");
  File_writer file_writer("file_writer_test", data);
  ASSERT_TRUE(file_writer.valid());
  std::string read_data;
  File_reader file_reader("file_writer_test", false, read_data);
  ASSERT_TRUE(file_reader.valid());
  ASSERT_TRUE(data == read_data);
  ASSERT_TRUE(std::remove("file_writer_test") == 0);
}

TEST_F(File_test, FileBackupReadTest) {
  std::string data("Quick Brown Fox jumped over the lazy dog.");
  std::ofstream writer("file_reader_test.backup");
  writer.write(data.c_str(), data.length());
  writer.close();

  std::string read_data;
  File_reader file_reader("file_reader_test", false, read_data);
  ASSERT_TRUE(file_reader.valid());
  ASSERT_TRUE(data == read_data);
  ASSERT_TRUE(std::remove("file_reader_test") == 0);
}

}  // namespace file_unittest
