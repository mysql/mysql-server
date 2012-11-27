/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "handler.h"

/**
  A mock handler which declares all the pure virtuals.
 */
class Mock_HANDLER : public handler
{
public:
  // Declare all the pure-virtuals.
  // Note: Sun Studio needs a little help in resolving uchar.
  MOCK_METHOD0(close,    int());
  MOCK_METHOD3(create,   int(const char *name, TABLE *form, HA_CREATE_INFO*));
  MOCK_METHOD1(info,     int(unsigned ha_status_bitmap));
  MOCK_METHOD3(open,     int(const char *name, int mode, uint test_if_locked));
  MOCK_METHOD1(position, void(const ::uchar *record));
  MOCK_METHOD1(rnd_init, int(bool scan));
  MOCK_METHOD1(rnd_next, int(::uchar *buf));
  MOCK_METHOD2(rnd_pos,  int(::uchar *buf, ::uchar *pos));
  MOCK_METHOD3(store_lock,
               THR_LOCK_DATA**(THD*, THR_LOCK_DATA**, thr_lock_type));

  MOCK_CONST_METHOD0(bas_ext,     const char**());
  MOCK_CONST_METHOD3(index_flags, ulong(::uint idx, ::uint part, bool));
  MOCK_CONST_METHOD0(table_flags, Table_flags());
  MOCK_CONST_METHOD0(table_type,  const char*());

  // Declare the members we actually want to test.
  MOCK_METHOD2(print_error, void(int error, myf errflag));

  Mock_HANDLER(handlerton *ht_arg, TABLE_SHARE *share_arg)
    : handler(ht_arg, share_arg)
  {}
};
