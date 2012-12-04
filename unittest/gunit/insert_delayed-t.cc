/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include "parsertest.h"
#include "test_utils.h"
#include "thr_lock.h"
#include "sql_lex.h"

namespace insert_delayed_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class InsertDelayed : public ParserTest
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }
};

TEST_F(InsertDelayed, InsertDelayed)
{
  SELECT_LEX *sl1=
    parse("INSERT INTO t1 VALUES (1)", 0);

  thr_lock_type expected_lock_type= sl1->table_list.first->lock_type;

  SELECT_LEX *sl2=
    parse("INSERT DELAYED INTO t1 VALUES (1)", ER_WARN_LEGACY_SYNTAX_CONVERTED);

  EXPECT_EQ(expected_lock_type, sl2->table_list.first->lock_type);
}

TEST_F(InsertDelayed, ReplaceDelayed)
{
  SELECT_LEX *sl1=
    parse("REPLACE INTO t1 VALUES (1)", 0);

  thr_lock_type expected_lock_type= sl1->table_list.first->lock_type;

  SELECT_LEX *sl2=
    parse("REPLACE DELAYED INTO t1 VALUES (1)", ER_WARN_LEGACY_SYNTAX_CONVERTED);

  EXPECT_EQ(expected_lock_type, sl2->table_list.first->lock_type);
}

}
