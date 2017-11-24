/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/test_utils.h"

namespace into_syntax_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class IntoSyntaxTest : public ParserTest
{
};

TEST_F(IntoSyntaxTest, Outer)
{
  SELECT_LEX *term= parse("SELECT 1 INTO @v");
  SELECT_LEX_UNIT *top_union= term->master_unit();
  EXPECT_EQ(nullptr, top_union->outer_select());
}

}
