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

#include "handler.h"

/**
  A fake handler which implements all the pure virtuals.
  This can be used as a base class for mock handlers.
 */
class Fake_HANDLER : public handler
{
public:
  virtual int rnd_next(uchar*)
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual int rnd_pos(uchar*, uchar*)
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual void position(const uchar*)
  { ADD_FAILURE() << "Unexpected call."; return; }
  virtual int info(uint)
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual const char* table_type() const
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual const char** bas_ext() const
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual ulong index_flags(uint, uint, bool) const
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual THR_LOCK_DATA** store_lock(THD*, THR_LOCK_DATA**, thr_lock_type)
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual int open(const char*, int, uint)
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual int close()
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual int rnd_init(bool)
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual Table_flags table_flags() const
  { ADD_FAILURE() << "Unexpected call."; return 0; }
  virtual int create(const char*, TABLE*, HA_CREATE_INFO*)
  { ADD_FAILURE() << "Unexpected call."; return 0; }

  Fake_HANDLER(handlerton *ht_arg, TABLE_SHARE *share_arg)
    : handler(ht_arg, share_arg)
  {}
};

/**
  A mock handler for testing print_error().
  TODO: Use gmock instead of this class.
 */
class Mock_HANDLER : public Fake_HANDLER
{
public:
  Mock_HANDLER(handlerton *ht_arg, TABLE_SHARE *share_arg)
    : Fake_HANDLER(ht_arg, share_arg),
      m_print_error_called(0),
      m_expected_error(0)
  {}
  virtual void print_error(int error, myf errflag)
  {
    EXPECT_EQ(m_expected_error, error);
    ++m_print_error_called;
  }
  void expect_error(int val)
  {
    m_expected_error= val;
  }
  int print_error_called() const { return m_print_error_called; }
private:
  int m_print_error_called;
  int m_expected_error;
};
