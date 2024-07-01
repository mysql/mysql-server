/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef BASE_MOCK_HANDLER_INCLUDED
#define BASE_MOCK_HANDLER_INCLUDED

#include "my_config.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "my_inttypes.h"
#include "sql/handler.h"

/**
  A base mock handler which declares all the pure virtuals. Create subclasses
  mocking additional virtual functions depending on what you want to test.
 */
class Base_mock_HANDLER : public handler {
 public:
  // Declare all the pure-virtuals.
  // Note: Sun Studio needs a little help in resolving uchar.
  MOCK_METHOD(int, close, (), (override));
  MOCK_METHOD(int, create,
              (const char *name, TABLE *form, HA_CREATE_INFO *,
               dd::Table *table_def),
              (override));
  MOCK_METHOD(int, info, (unsigned ha_status_bitmap), (override));
  MOCK_METHOD(int, open,
              (const char *name, int mode, uint test_if_locked,
               const dd::Table *table_def),
              (override));
  MOCK_METHOD(void, position, (const ::uchar *record), (override));
  MOCK_METHOD(int, rnd_init, (bool scan), (override));
  MOCK_METHOD(int, rnd_next, (::uchar * buf), (override));
  MOCK_METHOD(int, rnd_pos, (::uchar * buf, ::uchar *pos), (override));
  MOCK_METHOD(THR_LOCK_DATA **, store_lock,
              (THD *, THR_LOCK_DATA **, thr_lock_type), (override));

  MOCK_METHOD(ulong, index_flags, (::uint idx, ::uint part, bool),
              (const, override));
  MOCK_METHOD(Table_flags, table_flags, (), (const, override));
  MOCK_METHOD(const char *, table_type, (), (const, override));
  MOCK_METHOD(void, print_error, (int error, myf errflag), (override));

  Base_mock_HANDLER(handlerton *ht_arg, TABLE_SHARE *share_arg)
      : handler(ht_arg, share_arg) {}
};

#endif  // BASE_MOCK_HANDLER_INCLUDED
