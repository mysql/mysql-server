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

#include <gtest/gtest.h>

#include "backend_mem.h"  // Memory keyring backend

#include "components/keyrings/common/operations/operations.h"

namespace operations_unittest {

class KeyringCommonOperations_test : public ::testing::Test {};

using keyring_common::data::Data;
using keyring_common::data::Type;
using keyring_common::iterator::Iterator;
using keyring_common::meta::Metadata;
using keyring_common::operations::Keyring_operations;

using keyring_common_unit::Memory_backend;

TEST_F(KeyringCommonOperations_test, OperationsTestWithoutCache) {
  Metadata metadata1("key1", "foo@bar.com"), metadata2("key2", "foo@bar.com"),
      metadata3("key3", "foo@bar.com");
  Metadata metadata4("key1", "bar@foo.com"), metadata5("key2", "bar@foo.com"),
      metadata6("key3", "bar@foo.com");
  Metadata metadata7("key1", "bar@baz.com"), metadata8("key2", "bar@baz.com"),
      metadata9("key3", "bar@baz.com");
  Data data1("Data1", "Type1"), data2("Data2", "Type2"),
      data3("Data3", "Type3");

  std::unique_ptr<Memory_backend> memory_backend =
      std::make_unique<Memory_backend>();
  Keyring_operations<Memory_backend> keyring_operations(
      false, memory_backend.release());

  /* Store */
  ASSERT_FALSE(keyring_operations.store(metadata1, data1));
  ASSERT_FALSE(keyring_operations.store(metadata4, data1));
  ASSERT_FALSE(keyring_operations.store(metadata7, data1));

  ASSERT_FALSE(keyring_operations.store(metadata2, data2));
  ASSERT_FALSE(keyring_operations.store(metadata5, data2));
  ASSERT_FALSE(keyring_operations.store(metadata8, data2));

  ASSERT_FALSE(keyring_operations.store(metadata3, data3));
  ASSERT_FALSE(keyring_operations.store(metadata6, data3));
  ASSERT_FALSE(keyring_operations.store(metadata9, data3));

  ASSERT_TRUE(keyring_operations.store(metadata2, data2));
  ASSERT_TRUE(keyring_operations.store(metadata4, data2));
  ASSERT_TRUE(keyring_operations.store(metadata8, data2));

  ASSERT_EQ(keyring_operations.keyring_size(), 9);

  // data does not matter
  ASSERT_TRUE(keyring_operations.store(metadata2, data1));
  ASSERT_TRUE(keyring_operations.store(metadata4, data1));
  ASSERT_TRUE(keyring_operations.store(metadata8, data1));

  /* Erase */
  ASSERT_FALSE(keyring_operations.erase(metadata1));
  ASSERT_FALSE(keyring_operations.erase(metadata5));
  ASSERT_FALSE(keyring_operations.erase(metadata9));

  ASSERT_EQ(keyring_operations.keyring_size(), 6);

  ASSERT_TRUE(keyring_operations.erase(metadata1));
  ASSERT_TRUE(keyring_operations.erase(metadata5));
  ASSERT_TRUE(keyring_operations.erase(metadata9));

  ASSERT_EQ(keyring_operations.keyring_size(), 6);

  /* Search */
  Data returned_data1, returned_data2, returned_data3;

  ASSERT_FALSE(keyring_operations.get(metadata4, returned_data1));
  ASSERT_FALSE(keyring_operations.get(metadata8, returned_data2));
  ASSERT_FALSE(keyring_operations.get(metadata6, returned_data3));

  ASSERT_TRUE(returned_data1 == data1);
  ASSERT_TRUE(returned_data2 == data2);
  ASSERT_TRUE(returned_data3 == data3);

  ASSERT_TRUE(keyring_operations.get(metadata1, returned_data1));
  ASSERT_TRUE(keyring_operations.get(metadata5, returned_data2));
  ASSERT_TRUE(keyring_operations.get(metadata9, returned_data3));

  /* Read iterators */
  std::unique_ptr<Iterator<Data>> read_it1, read_it2, read_it3;
  ASSERT_FALSE(keyring_operations.init_read_iterator(read_it1, metadata4));
  ASSERT_FALSE(keyring_operations.init_read_iterator(read_it2, metadata8));
  ASSERT_FALSE(keyring_operations.init_read_iterator(read_it3, metadata6));

  Metadata read_metadata1, read_metadata2, read_metadata3;
  ASSERT_FALSE(keyring_operations.get_iterator_data(read_it1, read_metadata1,
                                                    returned_data1));
  ASSERT_FALSE(keyring_operations.get_iterator_data(read_it2, read_metadata2,
                                                    returned_data2));
  ASSERT_FALSE(keyring_operations.get_iterator_data(read_it3, read_metadata3,
                                                    returned_data3));

  ASSERT_TRUE(read_metadata1 == metadata4);
  ASSERT_TRUE(read_metadata2 == metadata8);
  ASSERT_TRUE(read_metadata3 == metadata6);

  ASSERT_TRUE(returned_data1 == data1);
  ASSERT_TRUE(returned_data2 == data2);
  ASSERT_TRUE(returned_data3 == data3);

  keyring_operations.deinit_forward_iterator(read_it1);
  keyring_operations.deinit_forward_iterator(read_it2);
  keyring_operations.deinit_forward_iterator(read_it3);
  ASSERT_TRUE(read_it1.get() == nullptr);
  ASSERT_TRUE(read_it2.get() == nullptr);
  ASSERT_TRUE(read_it3.get() == nullptr);

  /* Generate */
  ASSERT_FALSE(keyring_operations.generate(metadata1, "Type1", 8));
  ASSERT_FALSE(keyring_operations.generate(metadata5, "Type2", 16));
  ASSERT_FALSE(keyring_operations.generate(metadata9, "Type3", 32));

  ASSERT_TRUE(keyring_operations.keyring_size() == 9);

  ASSERT_FALSE(keyring_operations.get(metadata1, returned_data1));
  ASSERT_FALSE(keyring_operations.get(metadata5, returned_data2));
  ASSERT_FALSE(keyring_operations.get(metadata9, returned_data3));

  ASSERT_TRUE(returned_data1.get_data().valid() &&
              returned_data1.get_data().type() == "Type1");
  ASSERT_TRUE(returned_data2.get_data().valid() &&
              returned_data2.get_data().type() == "Type2");
  ASSERT_TRUE(returned_data3.get_data().valid() &&
              returned_data3.get_data().type() == "Type3");

  ASSERT_FALSE(keyring_operations.erase(metadata1));
  ASSERT_FALSE(keyring_operations.erase(metadata5));
  ASSERT_FALSE(keyring_operations.erase(metadata9));

  ASSERT_TRUE(keyring_operations.keyring_size() == 6);

  /* Iterator */
  std::unique_ptr<Iterator<Data>> it;
  ASSERT_FALSE(keyring_operations.init_forward_iterator(it, false));
  Metadata returned_metadata;
  Data returned_data;

  for (size_t i = 0; i < keyring_operations.keyring_size(); ++i) {
    ASSERT_FALSE(keyring_operations.get_iterator_data(it, returned_metadata,
                                                      returned_data));
    keyring_operations.next(it);
  }
  keyring_operations.deinit_forward_iterator(it);
  ASSERT_TRUE(it.get() == nullptr);

  /* Mix operations */
  ASSERT_FALSE(keyring_operations.store(metadata1, data1));
  ASSERT_FALSE(keyring_operations.store(metadata5, data2));
  ASSERT_FALSE(keyring_operations.store(metadata9, data3));

  ASSERT_EQ(keyring_operations.keyring_size(), 9);

  ASSERT_FALSE(keyring_operations.erase(metadata2));
  ASSERT_FALSE(keyring_operations.get(metadata1, returned_data1));
  ASSERT_TRUE(returned_data1 == data1);
  ASSERT_FALSE(keyring_operations.get(metadata3, returned_data3));
  ASSERT_TRUE(returned_data3 == data3);
  ASSERT_FALSE(keyring_operations.erase(metadata6));
  ASSERT_FALSE(keyring_operations.store(metadata2, data2));
  ASSERT_FALSE(keyring_operations.erase(metadata7));
  ASSERT_TRUE(keyring_operations.get(metadata6, returned_data));
  ASSERT_FALSE(keyring_operations.generate(metadata6, "Type3", 8));

  ASSERT_EQ(keyring_operations.keyring_size(), 8);

  ASSERT_FALSE(keyring_operations.init_forward_iterator(it, false));
  ASSERT_FALSE(keyring_operations.get_iterator_data(it, returned_metadata,
                                                    returned_data));
  ASSERT_FALSE(keyring_operations.next(it));

  ASSERT_FALSE(keyring_operations.store(metadata7, data3));
  ASSERT_EQ(keyring_operations.keyring_size(), 9);

  ASSERT_TRUE(keyring_operations.get_iterator_data(it, returned_metadata,
                                                   returned_data));
  ASSERT_TRUE(keyring_operations.next(it));

  keyring_operations.deinit_forward_iterator(it);
  ASSERT_TRUE(it.get() == nullptr);
}

}  // namespace operations_unittest
