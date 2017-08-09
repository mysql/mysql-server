/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gcs_basic_logging.h"

using ::testing::Return;
using ::testing::WithArgs;
using ::testing::Invoke;
using ::testing::_;
using ::testing::Eq;
using ::testing::DoAll;
using ::testing::ByRef;
using ::testing::SetArgReferee;
using ::testing::SaveArg;
using ::testing::ContainsRegex;
using ::testing::AnyNumber;

/**
  Class that defines basic common testing infra-structure to be used
  in all test cases and should be the default choice whenever a new
  testing class is created.

  Note that any global change to the test classes should be made
  here. Currently, it only defines a simple logging object.
*/
class GcsBaseTest : public ::testing::Test
{
public:
  GcsBaseTest() {};

  virtual ~GcsBaseTest() {};

  /**
    Simple logging object that can be used in the test case.
  */
  Gcs_basic_logging logging;
};


/**
  Class that defines basic common testing infra-structure to be used
  in al test cases whenever they need to create its own logging
  objects.
*/
class GcsBaseTestNoLogging : public ::testing::Test
{
public:
  GcsBaseTestNoLogging() {};

  virtual ~GcsBaseTestNoLogging() {};
};
