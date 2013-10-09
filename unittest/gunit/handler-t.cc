/* Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "handler-t.h"
#include "test_utils.h"
#include "fake_table.h"
#include "mock_field_datetime.h"

#include "sql_executor.h"

namespace {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

using ::testing::StrictMock;

class HandlerTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};


/**
  Some handler error returns are passed on to report_handler_error()
  which will:
    - ignore errors like END_OF_FILE
    - print most errors to the error log
    - pass the error code back to handler::print_error()
 */
TEST_F(HandlerTest, ReportErrorHandler)
{
  Mock_field_datetime field_datetime;
  Fake_TABLE table(&field_datetime);
  handlerton *hton= NULL;
  StrictMock<Mock_HANDLER> mock_handler(hton, table.get_share());
  table.set_handler(&mock_handler);

  // This error should be ignored.
  EXPECT_EQ(-1, report_handler_error(&table, HA_ERR_END_OF_FILE));

  // This one should not be printed to stderr, but passed on to the handler.
  EXPECT_CALL(mock_handler, print_error(HA_ERR_TABLE_DEF_CHANGED, 0)).Times(1);
  EXPECT_EQ(1, report_handler_error(&table, HA_ERR_TABLE_DEF_CHANGED));
}

}
