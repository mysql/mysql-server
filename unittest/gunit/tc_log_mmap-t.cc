/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"
#include <gtest/gtest.h>
#include "log.h"
#include "sql_class.h"
#include "test_utils.h"
#include "thread_utils.h"

using my_testing::Server_initializer;

/*
  Override msync/fsync, saves a *lot* of time during unit testing.
 */
class TC_LOG_MMAP_no_msync : public TC_LOG_MMAP
{
protected:
  virtual int do_msync_and_fsync(int fd, void *addr, size_t len, int flags)
  {
    return 0;
  }
};

/*
  This class is a friend of TC_LOG_MMAP, so it needs to be outside the unittest
  namespace.
*/
class TCLogMMapTest : public ::testing::Test
{
public:
  virtual void SetUp()
  {
    initializer.SetUp();
    total_ha_2pc= 2;
    tc_heuristic_recover= 0;
    EXPECT_EQ(0, tc_log_mmap.open("tc_log_mmap_test"));
  }

  virtual void TearDown()
  {
    tc_log_mmap.close();
    initializer.TearDown();
  }

  THD *thd()
  {
    return initializer.thd();
  }

  void testCommit(ulonglong xid)
  {
    thd()->transaction.xid_state.xid.set(xid);
    EXPECT_EQ(TC_LOG_MMAP::RESULT_SUCCESS, tc_log_mmap.commit(thd(), true));
    thd()->transaction.cleanup();
  }

  ulong testLog(ulonglong xid)
  {
    return tc_log_mmap.log_xid(xid);
  }

  void testUnlog(ulong cookie, ulonglong xid)
  {
    tc_log_mmap.unlog(cookie, xid);
  }

protected:
  TC_LOG_MMAP_no_msync tc_log_mmap;
  Server_initializer initializer;
};

namespace tc_log_mmap_unittest {

TEST_F(TCLogMMapTest, TClogCommit)
{
  // test calling of log/unlog for xid=1
  testCommit(1);
}

class TC_Log_MMap_thread : public thread::Thread
{
public:
  TC_Log_MMap_thread()
  : m_start_xid(0), m_end_xid(0),
    m_tc_log_mmap(NULL)
  {
  }

  void init (ulonglong start_value, ulonglong end_value,
             TCLogMMapTest* tc_log_mmap)
  {
    m_start_xid= start_value;
    m_end_xid= end_value;
    m_tc_log_mmap= tc_log_mmap;
  }

  virtual void run()
  {
    ulonglong xid= m_start_xid;
    while (xid < m_end_xid)
    {
      m_tc_log_mmap->testCommit(xid++);
    }
  }

protected:
  ulonglong m_start_xid, m_end_xid;
  TCLogMMapTest* m_tc_log_mmap;
};

TEST_F(TCLogMMapTest, ConcurrentAccess)
{
  static const unsigned MAX_WORKER_THREADS= 10;
  static const unsigned VALUE_INTERVAL= 100;

  TC_Log_MMap_thread tclog_threads[MAX_WORKER_THREADS];

  ulonglong start_value= 0;
  for (unsigned i=0; i < MAX_WORKER_THREADS; ++i)
  {
    tclog_threads[i].init(start_value, start_value + VALUE_INTERVAL, this);
    tclog_threads[i].start();
    start_value+= VALUE_INTERVAL;
  }

  for (unsigned i=0; i < MAX_WORKER_THREADS; ++i)
    tclog_threads[i].join();
}


TEST_F(TCLogMMapTest, FillAllPagesAndReuse)
{
  /* Get maximum number of XIDs which can be stored in TC log. */
  const uint MAX_XIDS= tc_log_mmap.size();
  ulong cookie;
  /* Fill TC log. */
  for(my_xid xid= 1; xid < MAX_XIDS; ++xid)
    (void)testLog(xid);
  cookie= testLog(MAX_XIDS);
  /*
    Now free one slot and try to reuse it.
    This should work and not crash on assert.
  */
  testUnlog(cookie, MAX_XIDS);
  testLog(MAX_XIDS + 1);
}


TEST_F(TCLogMMapTest, ConcurrentOverflow)
{
  const uint WORKER_THREADS= 10;
  const uint XIDS_TO_REUSE= 100;
  /* Get maximum number of XIDs which can be stored in TC log. */
  const uint MAX_XIDS= tc_log_mmap.size();
  ulong cookies[XIDS_TO_REUSE];

  /* Fill TC log. Remember cookies for last XIDS_TO_REUSE xids. */
  for(my_xid xid= 1; xid <= MAX_XIDS - XIDS_TO_REUSE; ++xid)
    testLog(xid);
  for (uint i= 0; i < XIDS_TO_REUSE; ++i)
    cookies[i]= testLog(MAX_XIDS - XIDS_TO_REUSE + 1 + i);

  /*
    Now create several threads which will try to do commit.
    Since log is full they will have to wait until we free some slots.
  */
  TC_Log_MMap_thread threads[WORKER_THREADS];
  for (uint i= 0; i < WORKER_THREADS; ++i)
  {
    threads[i].init(MAX_XIDS + i * (XIDS_TO_REUSE/WORKER_THREADS),
                    MAX_XIDS + (i + 1) * (XIDS_TO_REUSE/WORKER_THREADS), this);
    threads[i].start();
  }

  /*
    Once started all threads should block since we are out of free slots
    in the log, Resume threads by freeing necessary slots. Resumed thread
    should not hang or assert.
  */
  for (uint i= 0; i < XIDS_TO_REUSE; ++i)
    testUnlog(cookies[i], MAX_XIDS - XIDS_TO_REUSE + 1 + i);

  /* Wait till all threads are done. */
  for (uint i=0; i < WORKER_THREADS; ++i)
    threads[i].join();
}

}
