/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <string>

#include <gtest/gtest.h>

#include "components/keyrings/common/data/data.h"
#include "components/keyrings/common/data/data_extension.h"
#include "components/keyrings/common/data/meta.h"

using keyring_common::data::Data;
using keyring_common::data::Data_extension;
using keyring_common::meta::Metadata;

namespace data_unittest {

class KeyringCommonData_test : public ::testing::Test {};

class Dummy_extension {
 public:
  Dummy_extension(std::string ext_data) : ext_data_(ext_data) {}
  Dummy_extension() : Dummy_extension("") {}
  std::string ext_data() const { return ext_data_; }

 private:
  std::string ext_data_;
};

/* Tests for Metadata class */
TEST_F(KeyringCommonData_test, MetadataTest) {
  /* Valid metadata */
  Metadata metadata("key_id_1", "foo@bar.com");
  EXPECT_TRUE(metadata.valid());
  EXPECT_EQ(metadata.key_id(), "key_id_1");
  EXPECT_EQ(metadata.owner_id(), "foo@bar.com");
  std::string hash_key(metadata.key_id());
  hash_key.push_back('\0');
  hash_key.append(metadata.owner_id());
  EXPECT_EQ(metadata.hash_key(), hash_key);

  /* Invalid metadata */
  Metadata empty_metadata("", "");
  EXPECT_FALSE(empty_metadata.valid());
  EXPECT_EQ(empty_metadata.key_id(), "");
  EXPECT_EQ(empty_metadata.owner_id(), "");

  /* Copy constructor */
  Metadata copied_metadata(metadata);
  EXPECT_TRUE(copied_metadata.valid());
  EXPECT_EQ(copied_metadata.key_id(), "key_id_1");
  EXPECT_EQ(copied_metadata.owner_id(), "foo@bar.com");
  hash_key.assign(copied_metadata.key_id());
  hash_key.push_back('\0');
  hash_key.append(copied_metadata.owner_id());
  EXPECT_EQ(copied_metadata.hash_key(), hash_key);

  /* Assignment */
  Metadata assigned_metadata = copied_metadata;
  EXPECT_TRUE(assigned_metadata.valid());
  EXPECT_EQ(assigned_metadata.key_id(), "key_id_1");
  EXPECT_EQ(assigned_metadata.owner_id(), "foo@bar.com");
  hash_key.assign(assigned_metadata.key_id());
  hash_key.push_back('\0');
  hash_key.append(assigned_metadata.owner_id());
  EXPECT_EQ(assigned_metadata.hash_key(), hash_key);
}

/* Tests for data */
TEST_F(KeyringCommonData_test, DataTest) {
  /* Valid data */
  Data data("Data", "Type");
  EXPECT_TRUE(data.valid());
  EXPECT_EQ(data.data(), "Data");
  EXPECT_EQ(data.type(), "Type");

  /* Invalid data */
  Data invalid_data;
  EXPECT_FALSE(invalid_data.valid());
  EXPECT_EQ(invalid_data.data(), "");
  EXPECT_EQ(invalid_data.type(), "");

  /* Copy */
  Data copied_data(data);
  EXPECT_TRUE(copied_data.valid());
  EXPECT_EQ(copied_data.data(), "Data");
  EXPECT_EQ(copied_data.type(), "Type");

  /* Assignment */
  Data assigned_data = copied_data;
  EXPECT_TRUE(assigned_data.valid());
  EXPECT_EQ(assigned_data.data(), "Data");
  EXPECT_EQ(assigned_data.type(), "Type");
}

/* Tests for Data_extension */
TEST_F(KeyringCommonData_test, DataWrapperTest) {
  /* Valid data */
  Data data("Data", "Type");
  Dummy_extension ext("Ext");
  Data_extension<Dummy_extension> data_extension(data, ext);
  EXPECT_TRUE(data_extension.get_data().valid());
  EXPECT_EQ(data_extension.get_data().data(), "Data");
  EXPECT_EQ(data_extension.get_extension().ext_data(), "Ext");

  /* Empty data */
  Data_extension<Dummy_extension> empty_data_extension;
  EXPECT_FALSE(empty_data_extension.get_data().valid());
  EXPECT_EQ(empty_data_extension.get_extension().ext_data(), "");

  /* Set */
  Data_extension<Dummy_extension> set_data_extension;
  EXPECT_FALSE(set_data_extension.get_data().valid());
  set_data_extension.set_data(data);
  set_data_extension.set_extension(ext);
  EXPECT_TRUE(set_data_extension.get_data().valid());
  EXPECT_EQ(set_data_extension.get_data().data(), "Data");
  EXPECT_EQ(set_data_extension.get_extension().ext_data(), "Ext");
}

}  // namespace data_unittest
