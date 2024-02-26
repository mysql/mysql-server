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

#include <gtest/gtest.h>

#include "components/keyrings/common/data/data_extension.h"
#include "components/keyrings/common/memstore/iterator.h"

namespace iterator_unittest {

using keyring_common::cache::Datacache;
using keyring_common::data::Data;
using keyring_common::data::Data_extension;
using keyring_common::iterator::Iterator;
using keyring_common::meta::Metadata;

class KeyringCommonIterator_test : public ::testing::Test {};

class Dummy_extension {
 public:
  Dummy_extension(std::string ext_data) : ext_data_(ext_data) {}
  Dummy_extension() : Dummy_extension(std::string{""}) {}
  std::string ext_data() const { return ext_data_; }

 private:
  std::string ext_data_;
};

TEST_F(KeyringCommonIterator_test, IteratorTest) {
  Metadata metadata1("key1", "foo@bar.com"), metadata2("key2", "foo@bar.com"),
      metadata3("key2", "bar@foo.com"), metadata4("key1", "bar@foo.com");
  Data data1("data1", "type1"), data2("data2", "type2");
  Dummy_extension ext1("ex1"), ext2("ex2");
  Data_extension<Dummy_extension> dw1(data1, ext1), dw2(data2, ext2);

  Datacache<Data_extension<Dummy_extension>> cache;

  ASSERT_TRUE(cache.store(metadata1, dw1));
  ASSERT_TRUE(cache.store(metadata2, dw2));
  ASSERT_TRUE(cache.store(metadata3, dw1));
  ASSERT_EQ(cache.size(), 3);

  Metadata returned_metadata;
  Data_extension<Dummy_extension> returned_dw;

  Iterator<Data_extension<Dummy_extension>> it1(cache, false);
  ASSERT_TRUE(it1.valid(cache.version()));

  ASSERT_TRUE(it1.metadata(cache.version(), returned_metadata));
  ASSERT_TRUE(it1.data(cache.version(), returned_dw));

  ASSERT_TRUE(it1.next(cache.version()));
  ASSERT_TRUE(it1.valid(cache.version()));

  ASSERT_TRUE(it1.metadata(cache.version(), returned_metadata));
  ASSERT_TRUE(it1.data(cache.version(), returned_dw));

  ASSERT_TRUE(returned_metadata.valid());
  ASSERT_TRUE(returned_dw.get_data().valid());

  ASSERT_TRUE(it1.next(cache.version()));
  ASSERT_TRUE(it1.valid(cache.version()));

  ASSERT_TRUE(cache.store(metadata4, dw2));
  ASSERT_FALSE(it1.next(cache.version()));

  Iterator<Data_extension<Dummy_extension>> it2(cache, false);
  ASSERT_TRUE(it2.valid(cache.version()));

  ASSERT_TRUE(it2.metadata(cache.version(), returned_metadata));
  ASSERT_TRUE(it2.data(cache.version(), returned_dw));

  ASSERT_TRUE(returned_metadata.valid());
  ASSERT_TRUE(returned_dw.get_data().valid());

  ASSERT_TRUE(it2.next(cache.version()));
  ASSERT_TRUE(it2.valid(cache.version()));

  ASSERT_TRUE(cache.erase(metadata3));
  ASSERT_FALSE(it2.valid(cache.version()));

  Iterator<Data_extension<Dummy_extension>> it3(cache, false);
  ASSERT_TRUE(it3.valid(cache.version()));

  ASSERT_TRUE(it3.metadata(cache.version(), returned_metadata));
  ASSERT_TRUE(it3.data(cache.version(), returned_dw));

  ASSERT_TRUE(returned_metadata.valid());
  ASSERT_TRUE(returned_dw.get_data().valid());

  ASSERT_TRUE(cache.store(metadata3, data1));
  ASSERT_FALSE(it3.metadata(cache.version(), returned_metadata));

  Iterator<Data_extension<Dummy_extension>> it4(cache, false);
  ASSERT_TRUE(it4.valid(cache.version()));

  ASSERT_TRUE(it4.metadata(cache.version(), returned_metadata));
  ASSERT_TRUE(it4.data(cache.version(), returned_dw));

  ASSERT_TRUE(returned_metadata.valid());
  ASSERT_TRUE(returned_dw.get_data().valid());

  ASSERT_TRUE(cache.erase(metadata2));
  ASSERT_FALSE(it4.data(cache.version(), returned_dw));

  Iterator<Data_extension<Dummy_extension>> it5(cache, true);
  ASSERT_TRUE(it5.valid(cache.version()));

  ASSERT_TRUE(it5.metadata(cache.version(), returned_metadata));
  ASSERT_TRUE(it5.data(cache.version(), returned_dw));

  ASSERT_TRUE(returned_metadata.valid());
  ASSERT_TRUE(returned_dw.get_data().valid());

  ASSERT_TRUE(cache.store(metadata2, dw2));
  ASSERT_TRUE(it5.data(cache.version(), returned_dw));
}

TEST_F(KeyringCommonIterator_test, IteratorAtTest) {
  Metadata metadata1("key1", "foo@bar.com"), metadata2("key2", "foo@bar.com"),
      metadata3("key2", "bar@foo.com"), metadata4("key1", "bar@foo.com");
  Data data1("data1", "type1"), data2("data2", "type2");
  Dummy_extension ext1("ex1"), ext2("ex2");
  Data_extension<Dummy_extension> dw1(data1, ext1), dw2(data2, ext2);

  Datacache<Data_extension<Dummy_extension>> cache;

  ASSERT_TRUE(cache.store(metadata1, dw1));
  ASSERT_TRUE(cache.store(metadata2, dw2));
  ASSERT_TRUE(cache.store(metadata3, dw1));
  ASSERT_EQ(cache.size(), 3);

  Metadata returned_metadata;
  Data_extension<Dummy_extension> returned_dw;

  Iterator<Data_extension<Dummy_extension>> it1(cache, metadata2);
  ASSERT_TRUE(it1.valid(cache.version()));

  ASSERT_TRUE(it1.metadata(cache.version(), returned_metadata));
  ASSERT_TRUE(it1.data(cache.version(), returned_dw));

  ASSERT_TRUE(returned_metadata == metadata2);
  ASSERT_TRUE(returned_dw == dw2);

  ASSERT_TRUE(cache.store(metadata4, dw2));
  ASSERT_EQ(cache.size(), 4);

  ASSERT_FALSE(it1.metadata(cache.version(), returned_metadata));
  ASSERT_FALSE(it1.data(cache.version(), returned_dw));
}
}  // namespace iterator_unittest
