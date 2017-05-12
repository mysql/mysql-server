/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <ctime>
#include <gtest/gtest.h>

#include "gcs_xcom_notification.h"
#include "mysql/gcs/gcs_log_system.h"

namespace gcs_xcom_notification_unittest
{
class XcomNotificationTest : public ::testing::Test
{
protected:

  XcomNotificationTest() {};

  virtual void SetUp()
  {
    logger= new Gcs_simple_ext_logger_impl();
    Gcs_logger::initialize(logger);
  }

  virtual void TearDown()
  {
    Gcs_logger::finalize();
    logger->finalize();
    delete logger;
  }

  Gcs_simple_ext_logger_impl *logger;
};

void function(int &val)
{
  val += 1;
}

class Dummy_notification : public Parameterized_notification<false>
{
public:
   Dummy_notification(void (*functor)(int &), int &val)
     : m_functor(functor), m_val(val)
   {
   }

   ~Dummy_notification()
   {
   }

   void (*m_functor)(int &);
   int &m_val;

private:
   void do_execute()
   {
     (*m_functor)(m_val);
   }
};

static int var= 0;
static void cleanup()
{
  var += 1;
}

TEST_F(XcomNotificationTest, ProcessDummyNotification)
{
  int val= 0;
  Gcs_xcom_engine *engine= new Gcs_xcom_engine();

  ASSERT_EQ(val, 0);

  engine->initialize(NULL);
  engine->push(new Dummy_notification(&function, val));
  engine->finalize(NULL);
  delete engine;

  ASSERT_EQ(val, 1);
}

TEST_F(XcomNotificationTest, ProcessFinalizeNotification)
{
  Gcs_xcom_engine *engine= new Gcs_xcom_engine();

  ASSERT_EQ(var, 0);

  engine->initialize(NULL);
  engine->finalize(cleanup);
  delete engine;

  ASSERT_EQ(var, 1);
}
}
