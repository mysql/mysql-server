/* Copyright (c) 2014, 2026, Oracle and/or its affiliates.

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
#include <sql/item.h>
#include "my_config.h"

#include "test_utils.h"

namespace item_param_unittest {
using my_testing::Server_initializer;

class ItemParamTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_initializer.SetUp();
    // An Item expects to be owned by current_thd->free_list, so allocate with
    // new, and do not delete it.
  }

  void TearDown() override { m_initializer.TearDown(); }

  Server_initializer m_initializer;
};
}  // namespace item_param_unittest
