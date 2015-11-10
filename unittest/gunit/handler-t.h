/* Copyright (c) 2012, 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef HANDLER_T_INCLUDED
#define HANDLER_T_INCLUDED

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "base_mock_handler.h"

/**
  A mock handler extending Base_mock_HANDLER
 */
class Mock_HANDLER : public Base_mock_HANDLER
{
public:
  // Declare the members we actually want to test.
  MOCK_METHOD2(print_error, void(int error, myf errflag));

  Mock_HANDLER(handlerton *ht_arg, TABLE_SHARE *share_arg)
    : Base_mock_HANDLER(ht_arg, share_arg)
  {}
};


/**
  A mock for the handlerton struct
*/
class Fake_handlerton : public handlerton
{
public:
  /// Minimal initialization of the handlerton
  Fake_handlerton()
  {
    slot= 0;
  }
};

#endif  // HANDLER_T_INCLUDED
