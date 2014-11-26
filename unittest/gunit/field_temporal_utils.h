/* Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef FIELD_TEMPORAL_UTILS_INCLUDED
#define FIELD_TEMPORAL_UTILS_INCLUDED

#include "field.h"
#include <gtest/gtest.h>
#include "test_utils.h"

namespace {

using my_testing::Mock_error_handler;

void store_zero_in_sql_mode(Field_temporal *field,
                            const char *store_value, const int length,
                            const char *expected_result,
                            const type_conversion_status expect_status,
                            const sql_mode_t test_mode,
                            const uint expected_error_code)
{
  THD *thd= field->table->in_use;
  sql_mode_t save_mode= thd->variables.sql_mode;
  thd->variables.sql_mode= test_mode;

  Mock_error_handler error_handler(thd, expected_error_code);
  type_conversion_status err=
    field->store(store_value, length, &my_charset_latin1);

  String unused;
  String str;
  field->val_str(&str, &unused);

  EXPECT_EQ(expect_status, err);
  EXPECT_STREQ(expected_result, str.ptr());
  EXPECT_EQ((expected_error_code == 0 ? 0 : 1),
            error_handler.handle_called());

  thd->variables.sql_mode= save_mode;
}

void test_store_string(Field_temporal *field,
                       const char *store_value, const int length,
                       const char *expected_result,
                       const int expected_error_no,
                       const type_conversion_status expected_status)
{
  THD *thd= field->table->in_use;
  sql_mode_t save_mode= thd->variables.sql_mode;
  thd->variables.sql_mode= MODE_NO_ENGINE_SUBSTITUTION;
  char buff[MAX_FIELD_WIDTH];
  String str(buff, sizeof(buff), &my_charset_bin);
  String unused;

  Mock_error_handler error_handler(field->table->in_use, expected_error_no);
  type_conversion_status err= field->store(store_value, length,
                                           &my_charset_latin1);
  field->val_str(&str, &unused);
  EXPECT_STREQ(expected_result, str.ptr());

  EXPECT_FALSE(field->is_null());
  EXPECT_EQ(expected_status, err);
  EXPECT_EQ((expected_error_no == 0 ? 0 : 1), error_handler.handle_called());
  thd->variables.sql_mode= save_mode;
}


};


#endif  // FIELD_TEMPORAL_UTILS_INCLUDED
