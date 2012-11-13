/* Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "thread_utils.h"

#include "my_global.h"
#include "my_dbug.h"

using thread::Notification;
using thread::Thread;

namespace {

#if defined(DBUG_OFF)
TEST(DebugTest, NoSuicide)
{
  DBUG_SUICIDE();
}
#else 
TEST(DebugDeathTest, Suicide)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(DBUG_SUICIDE(), "");
}
#endif


#if !defined(DBUG_OFF) && !defined(__WIN__)
class DbugGcovThread : public Thread
{
public:
  DbugGcovThread(Notification *start_notification)
    : m_start_notification(start_notification)
  {}

  virtual void run()
  {
    m_start_notification->notify();
    _db_flush_gcov_();
  }

private:
  Notification *m_start_notification;
};


TEST(DebugFlushGcov, FlushGcovParallel)
{
  Notification start_notification;
  DbugGcovThread debug_thread(&start_notification);
  debug_thread.start();

  // Wait for the other thread to start, then flush in parallel.
  start_notification.wait_for_notification();
  _db_flush_gcov_();
  debug_thread.join();
}
#endif

}
