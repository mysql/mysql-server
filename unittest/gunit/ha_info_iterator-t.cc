/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "sql/transaction_info.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ha_info_iterator_unittests {

class Ha_info_iterator_test : public ::testing::Test {
 protected:
  Ha_info_iterator_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Ha_info_iterator_test, Iterator_test) {
  Ha_trx_info ha_trx_info[3];
  Transaction_ctx::THD_TRANS thd_trans;

  // `register_ha` inserts the objects at the list head so, while iterating
  // they will be shown in reverse order to the insertion
  thd_trans.m_ha_list = &ha_trx_info[0];
  ha_trx_info[1].register_ha(&thd_trans, nullptr);
  ha_trx_info[2].register_ha(&thd_trans, nullptr);

  Ha_trx_info_list ha_list1{&ha_trx_info[2]};
  size_t idx{2};
  for (auto &ha : ha_list1) {
    EXPECT_EQ(&ha, &ha_trx_info[idx]);
    --idx;
  }

  // it1 now points to ha_trx_info[2]
  auto it1 = ha_list1.begin();
  // it2 now points to ha_trx_info[2] and it1 now points to ha_trx_info[1]
  auto it2 = it1++;
  // it0 now points to ha_trx_info[2]
  auto it0 = ha_list1.begin();
  // it0 now points to ha_trx_info[1]
  ++it0;
  // it0 now points to ha_trx_info[0]
  ++it0;
  EXPECT_TRUE(it2 == ha_trx_info[2]);
  EXPECT_TRUE(it1 == ha_trx_info[1]);
  EXPECT_TRUE(it0 == ha_trx_info[0]);
  EXPECT_TRUE(it0 == ha_trx_info);

  EXPECT_TRUE(it1 != ha_trx_info);
  EXPECT_TRUE(it2 != ha_trx_info[1]);

  it1 = it2;
  EXPECT_TRUE(it1 == it2);
  EXPECT_TRUE(it1 != it0);

  Ha_trx_info_list ha_list2;
  ha_list2 = ha_list1;
  Ha_trx_info_list ha_list3 = ha_list2;
  Ha_trx_info_list const ha_list4 = std::move(ha_list2);
  // after std::move, ha_list2 is empty
  EXPECT_TRUE(ha_list2.begin() == ha_list2.end());
  // both ha_list3 and ha_list4 where assigned from ha_list2 so, they must
  // evaluate to equal
  EXPECT_TRUE(ha_list3 == ha_list4);
  // ha_list2 is empty so it must evaluate to not equal to ha_list3
  EXPECT_TRUE(ha_list2 != ha_list3);

  // testing `operator*`
  EXPECT_TRUE(&(*ha_list3) == &ha_trx_info[2]);
  EXPECT_TRUE(&(*ha_list4) == &ha_trx_info[2]);
  EXPECT_TRUE(&(*ha_list3) != &ha_trx_info[1]);
  EXPECT_TRUE(&(*ha_list4) != &ha_trx_info[1]);

  // testing `operator==(Ha_trx_info)`
  EXPECT_TRUE(ha_list3 == &ha_trx_info[2]);
  EXPECT_TRUE(ha_list3 != &ha_trx_info[1]);

  // testing `operator==(std::nullptr_t)`
  EXPECT_TRUE(ha_list2 == nullptr);
  EXPECT_TRUE(ha_list3 != nullptr);

  // testing `operator->`
  EXPECT_TRUE(!ha_list3->is_started());
  EXPECT_TRUE(!ha_list4->is_started());
}

}  // namespace ha_info_iterator_unittests
