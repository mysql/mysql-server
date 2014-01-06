/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved. 

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "thread_utils.h"

#include "my_global.h"
#include "my_dbug.h"

using thread::Notification;
using thread::Thread;

namespace dbug_unittest {

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


#if !defined(DBUG_OFF)
TEST(DebugPrintTest, PrintEval)
{
  int y= 0;

  // This DBUG_PRINT args should never be evaluated.
  DBUG_PRINT("never",("%d",1/y));
}


TEST(DebugPrintDeathTest, PrintEval)
{
  int y= 0;

  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  DBUG_SET("+d,never");
  /*
    The DBUG_PRINT would be evaluated resulting in floating point exception
    killing the server.
  */
  EXPECT_DEATH_IF_SUPPORTED(DBUG_PRINT("never",("%d",1/y)), "");
  DBUG_SET("");
}


TEST(DebugSetTest, DebugKeywordsTest)
{
  char buf[1024];

  /*
    Enable d flag, then enable debug on a keyword. The debug should
    remain set to d and empty list of keywords indicating debug is
    enabled for all.
  */
  DBUG_SET("d");
  DBUG_SET("+d,keyword");
  DBUG_EXPLAIN(buf, sizeof(buf));
  EXPECT_STREQ("d", buf);
  DBUG_SET("");

  /*
    Set debug on a specific keyword. Debug should be enabled
    for the keyword.
  */
  DBUG_SET("+d,keyword");
  DBUG_EXPLAIN(buf, sizeof(buf));
  EXPECT_STREQ("d,keyword", buf);

  /*
    Remove the keyword from debug list. Debug should be
    disabled.
  */
  DBUG_SET("-d,keyword");
  DBUG_EXPLAIN(buf, sizeof(buf));
  EXPECT_STREQ("",buf);
  DBUG_SET("");

  /*
    Enable debug for a keyword. Then enable debug for all
    keywords. Debug should now be enabled for all keywords.
  */
  DBUG_SET("+d,keyword");
  DBUG_SET("+d");
  DBUG_EXPLAIN(buf,sizeof(buf));
  EXPECT_STREQ("d",buf);
  DBUG_SET("");

  // Add multiple debug keywords.
  DBUG_SET("+d,keyword1");
  DBUG_SET("+d,keyword2");
  DBUG_EXPLAIN(buf,sizeof(buf));
  EXPECT_STREQ("d,keyword1,keyword2",buf);
  DBUG_SET("-d,keyword1");
  DBUG_EXPLAIN(buf,sizeof(buf));
  EXPECT_STREQ("d,keyword2",buf);
  DBUG_SET("-d,keyword2");
  DBUG_EXPLAIN(buf,sizeof(buf));
  EXPECT_STREQ("",buf);
  DBUG_SET("");
}
#endif /* DBUG_OFF */
}
