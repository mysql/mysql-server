/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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

#ifndef HANDLER_T_INCLUDED
#define HANDLER_T_INCLUDED

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "my_inttypes.h"
#include "sql/handler.h"
#include "unittest/gunit/base_mock_handler.h"

/**
  A mock handler extending Base_mock_HANDLER
 */
class Mock_HANDLER : public Base_mock_HANDLER {
 public:
  // Declare the members we actually want to test.
  MOCK_METHOD(void, print_error, (int error, myf errflag), (override));
  MOCK_METHOD(bool, primary_key_is_clustered, (), (const override));
  MOCK_METHOD(ha_rows, records_in_range,
              (unsigned index, key_range *min_key, key_range *max_key),
              (override));
  MOCK_METHOD(Cost_estimate, index_scan_cost,
              (unsigned index, double ranges, double rows), (override));

  Mock_HANDLER(handlerton *ht_arg, TABLE_SHARE *share_arg)
      : Base_mock_HANDLER(ht_arg, share_arg) {
    // By default, estimate all ranges to have 10 records, just like the default
    // implementation in handler.
    ON_CALL(*this, records_in_range).WillByDefault(testing::Return(10));
  }

  void set_ha_table_flags(Table_flags flags) { cached_table_flags = flags; }
};

/**
  A mock handler for testing the sampling handler.
*/
class Mock_SAMPLING_HANDLER : public Base_mock_HANDLER {
 public:
  /*
    Declare the members we actually want to test. These are the members that
    should be called by the "default" sampling implementation.
  */
  MOCK_METHOD1(rnd_init, int(bool scan));
  MOCK_METHOD1(rnd_next, int(::uchar *buf));
  MOCK_METHOD0(rnd_end, int());

  Mock_SAMPLING_HANDLER(handlerton *ht_arg, TABLE *table_arg,
                        TABLE_SHARE *share)
      : Base_mock_HANDLER(ht_arg, share) {
    table = table_arg;
  }
};

/**
  A mock for the handlerton struct
*/
class Fake_handlerton : public handlerton {
 public:
  /// Minimal initialization of the handlerton
  Fake_handlerton() {
    slot = 0;
    db_type = DB_TYPE_UNKNOWN;
  }
};

#endif  // HANDLER_T_INCLUDED
