/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <string>

#include <gtest/gtest.h>

#include "components/keyrings/common/data/data_extension.h"
#include "components/keyrings/common/memstore/cache.h"

namespace cache_unittest {

using keyring_common::cache::Datacache;
using keyring_common::data::Data;
using keyring_common::data::Data_extension;
using keyring_common::meta::Metadata;

class KeyringCommonCache_test : public ::testing::Test {};

class Dummy_extension {
 public:
  Dummy_extension(std::string ext_data) : ext_data_(ext_data) {}
  Dummy_extension() : Dummy_extension(std::string{""}) {}
  std::string ext_data() const { return ext_data_; }

 private:
  std::string ext_data_;
};

TEST_F(KeyringCommonCache_test, CacheDataTest) {
  Metadata metadata1("key1", "foo@bar.com"), metadata2("key2", "foo@bar.com"),
      metadata3("key3", "foo@bar.com");
  Metadata metadata4("key1", "bar@foo.com"), metadata5("key2", "bar@foo.com"),
      metadata6("key3", "bar@foo.com"), metadata7("key4", "bar@foo.com");
  Metadata metadata8("key", "foo@bar.com");

  Data data1("Data1", "Type1"), data2("Data2", "Type2"),
      data3("Data3", "Type3");
  Data data4("Data1", "Type1");
  Data invalid_data;

  Datacache<Data> cache;

  size_t cached_entries = 0;

  /* Store operations */
  EXPECT_TRUE(cache.store(metadata1, data1));
  EXPECT_EQ(cache.size(), ++cached_entries);
  EXPECT_TRUE(cache.store(metadata2, data2));
  EXPECT_TRUE(cache.store(metadata3, data3));
  EXPECT_TRUE(cache.store(metadata4, data4));
  cached_entries += 3;
  EXPECT_EQ(cache.size(), cached_entries);

  /* Duplicate keys */
  EXPECT_FALSE(cache.store(metadata1, data1));
  EXPECT_FALSE(cache.store(metadata4, data4));
  EXPECT_EQ(cache.size(), cached_entries);

  /* Duplicate data */
  EXPECT_TRUE(cache.store(metadata5, data1));
  EXPECT_TRUE(cache.store(metadata6, data2));
  cached_entries += 2;
  EXPECT_EQ(cache.size(), cached_entries);

  /* Invalid data */
  EXPECT_TRUE(cache.store(metadata7, invalid_data));
  EXPECT_EQ(cache.size(), ++cached_entries);

  /* Fetch */
  Data returned_data;
  EXPECT_TRUE(cache.get(metadata1, returned_data));
  EXPECT_EQ(returned_data.data(), "Data1");
  EXPECT_EQ(returned_data.type(), "Type1");

  returned_data = invalid_data;
  EXPECT_TRUE(cache.get(metadata4, returned_data));
  EXPECT_EQ(returned_data.data(), "Data1");
  EXPECT_EQ(returned_data.type(), "Type1");

  returned_data = invalid_data;
  EXPECT_TRUE(cache.get(metadata6, returned_data));
  EXPECT_EQ(returned_data.data(), "Data2");
  EXPECT_EQ(returned_data.type(), "Type2");

  EXPECT_TRUE(cache.get(metadata7, returned_data));
  EXPECT_FALSE(returned_data.valid());

  returned_data = invalid_data;
  EXPECT_FALSE(cache.get(metadata8, returned_data));
  EXPECT_FALSE(returned_data.valid());

  /* Erase */
  EXPECT_TRUE(cache.erase(metadata2));
  EXPECT_EQ(cache.size(), --cached_entries);
  EXPECT_TRUE(cache.erase(metadata6));
  EXPECT_EQ(cache.size(), --cached_entries);

  returned_data = invalid_data;
  EXPECT_FALSE(cache.get(metadata2, returned_data));
  EXPECT_FALSE(cache.get(metadata6, returned_data));

  /* Misc operations */
  EXPECT_TRUE(cache.store(metadata6, data3));
  EXPECT_EQ(cache.size(), ++cached_entries);
  EXPECT_TRUE(cache.get(metadata3, returned_data));
  EXPECT_EQ(returned_data.data(), "Data3");
  EXPECT_EQ(returned_data.type(), "Type3");
  EXPECT_TRUE(cache.get(metadata5, returned_data));
  EXPECT_EQ(returned_data.data(), "Data1");
  EXPECT_EQ(returned_data.type(), "Type1");
  EXPECT_TRUE(cache.erase(metadata5));
  EXPECT_EQ(cache.size(), --cached_entries);
  EXPECT_TRUE(cache.get(metadata7, returned_data));
  EXPECT_FALSE(returned_data.valid());
  EXPECT_TRUE(cache.erase(metadata1));
  EXPECT_TRUE(cache.erase(metadata3));
  cached_entries -= 2;
  EXPECT_EQ(cache.size(), cached_entries);

  EXPECT_TRUE(cache.begin() != cache.end());
}

TEST_F(KeyringCommonCache_test, CacheDataWrapperTest) {
  Metadata metadata1("key1", "foo@bar.com"), metadata2("key2", "foo@bar.com");
  Metadata metadata3("key1", "bar@foo.com"), metadata4("key2", "bar@foo.com");
  Data data1("Data1", "Type1"), data2("Data2", "Type2"),
      data3("Data3", "Type3"), data4("Data4", "Type4");
  Dummy_extension dummy1("ext1"), dummy2("ext2"), dummy3("ext3"),
      dummy4("ext4");
  Data_extension<Dummy_extension> dw1(data1, dummy1), dw2(data2, dummy2),
      dw3(data3, dummy3), dw4(data4, dummy4), fetched_dw;

  Datacache<Data_extension<Dummy_extension>> cache;

  EXPECT_TRUE(cache.store(metadata1, dw1));
  EXPECT_TRUE(cache.store(metadata2, dw2));
  EXPECT_TRUE(cache.store(metadata3, dw3));
  EXPECT_EQ(cache.size(), 3);

  EXPECT_TRUE(cache.get(metadata2, fetched_dw));
  EXPECT_EQ(fetched_dw.get_extension().ext_data(), dummy2.ext_data());
  EXPECT_EQ(fetched_dw.get_data().data(), data2.data());
  EXPECT_EQ(fetched_dw.get_data().type(), data2.type());

  EXPECT_TRUE(cache.erase(metadata2));
  EXPECT_EQ(cache.size(), 2);
  EXPECT_FALSE(cache.get(metadata2, fetched_dw));
  EXPECT_FALSE(cache.erase(metadata2));
  EXPECT_EQ(cache.size(), 2);

  EXPECT_FALSE(cache.store(metadata1, dw1));
  EXPECT_TRUE(cache.store(metadata4, dw4));
  EXPECT_EQ(cache.size(), 3);

  EXPECT_TRUE(cache.erase(metadata1));
  EXPECT_EQ(cache.size(), 2);
  EXPECT_FALSE(cache.empty());
}

}  // namespace cache_unittest
