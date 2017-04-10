/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include "parsertest.h"
#include "test_utils.h"

namespace subquery_syntax_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;
using my_testing::expect_null;

class SubquerySyntaxTest : public ParserTest
{
};

TEST_F(SubquerySyntaxTest, Outer)
{
  SELECT_LEX *term= parse("SET @v = ( SELECT 1 )");
  SELECT_LEX_UNIT *top_union= term->master_unit();
  expect_null(top_union->outer_select());
}

}
