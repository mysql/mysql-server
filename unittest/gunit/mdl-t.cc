/* Copyright (c) 2009, 2012, Oracle and/or its affiliates. All rights reserved.

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

/**
   This is a unit test for the 'meta data locking' classes.
   It is written to illustrate how we can use Google Test for unit testing
   of MySQL code.
   For documentation on Google Test, see http://code.google.com/p/googletest/
   and the contained wiki pages GoogleTestPrimer and GoogleTestAdvancedGuide.
   The code below should hopefully be (mostly) self-explanatory.
 */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "mdl.h"
#include <mysqld_error.h>

#include "thr_malloc.h"
#include "thread_utils.h"
#include "test_mdl_context_owner.h"

/*
  Mock thd_wait_begin/end functions
*/

extern "C" void thd_wait_begin(MYSQL_THD thd, int wait_type)
{
}

extern "C" void thd_wait_end(MYSQL_THD thd)
{
}

/*
  A mock error handler.
*/
static uint expected_error= 0;
extern "C" void test_error_handler_hook(uint err, const char *str, myf MyFlags)
{
  EXPECT_EQ(expected_error, err) << str;
}

/*
  Mock away this global function.
  We don't need DEBUG_SYNC functionality in a unit test.
 */
void debug_sync(THD *thd, const char *sync_point_name, size_t name_len)
{
  DBUG_PRINT("debug_sync_point", ("hit: '%s'", sync_point_name));
  FAIL() << "Not yet implemented.";
}

/*
  Putting everything in a namespace prevents any (unintentional)
  name clashes with the code under test.
*/
namespace mdl_unittest {

using thread::Notification;
using thread::Thread;

const char db_name[]= "some_database";
const char table_name1[]= "some_table1";
const char table_name2[]= "some_table2";
const char table_name3[]= "some_table3";
const char table_name4[]= "some_table4";
const ulong zero_timeout= 0;
const ulong long_timeout= (ulong) 3600L*24L*365L;


class MDLTest : public ::testing::Test, public Test_MDL_context_owner
{
protected:
  MDLTest()
  : m_null_ticket(NULL),
    m_null_request(NULL)
  {
  }

  static void SetUpTestCase()
  {
    error_handler_hook= test_error_handler_hook;
    mdl_locks_hash_partitions= MDL_LOCKS_HASH_PARTITIONS_DEFAULT;
  }

  void SetUp()
  {
    expected_error= 0;
    mdl_init();
    m_mdl_context.init(this);
    EXPECT_FALSE(m_mdl_context.has_locks());
    m_global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                          MDL_TRANSACTION);
  }

  void TearDown()
  {
    m_mdl_context.destroy();
    mdl_destroy();
  }

  virtual bool notify_shared_lock(MDL_context_owner *in_use,
                                  bool needs_thr_lock_abort)
  {
    return in_use->notify_shared_lock(NULL, needs_thr_lock_abort);
  }

  // A utility member for testing single lock requests.
  void test_one_simple_shared_lock(enum_mdl_type lock_type);

  const MDL_ticket  *m_null_ticket;
  const MDL_request *m_null_request;
  MDL_context        m_mdl_context;
  MDL_request        m_request;
  MDL_request        m_global_request;
  MDL_request_list   m_request_list;
private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(MDLTest);
};


/*
  Will grab a lock on table_name of given type in the run() function.
  The two notifications are for synchronizing with the main thread.
  Does *not* take ownership of the notifications.
*/
class MDL_thread : public Thread, public Test_MDL_context_owner
{
public:
  MDL_thread(const char   *table_name,
             enum_mdl_type mdl_type,
             Notification *lock_grabbed,
             Notification *release_locks,
             Notification *lock_blocked,
             Notification *lock_released)
  : m_table_name(table_name),
    m_mdl_type(mdl_type),
    m_lock_grabbed(lock_grabbed),
    m_release_locks(release_locks),
    m_lock_blocked(lock_blocked),
    m_lock_released(lock_released),
    m_ignore_notify(false)
  {
    m_mdl_context.init(this);
  }

  ~MDL_thread()
  {
    m_mdl_context.destroy();
  }

  virtual void run();
  void ignore_notify() { m_ignore_notify= true; }

  virtual bool notify_shared_lock(MDL_context_owner *in_use,
                                  bool needs_thr_lock_abort)
  {
    if (in_use)
      return in_use->notify_shared_lock(NULL, needs_thr_lock_abort);

    if (m_ignore_notify)
      return false;
    m_release_locks->notify();
    return true;
  }

  virtual void enter_cond(mysql_cond_t *cond,
                          mysql_mutex_t* mutex,
                          const PSI_stage_info *stage,
                          PSI_stage_info *old_stage,
                          const char *src_function,
                          const char *src_file,
                          int src_line)
  {
    Test_MDL_context_owner::enter_cond(cond, mutex, stage, old_stage,
                                       src_function, src_file, src_line);

    /*
      No extra checks needed here since MDL uses enter_con only when thread
      is blocked.
    */
    if (m_lock_blocked)
      m_lock_blocked->notify();

    return;
  }

  MDL_context& get_mdl_context()
  {
    return m_mdl_context;
  }

private:
  const char    *m_table_name;
  enum_mdl_type  m_mdl_type;
  Notification  *m_lock_grabbed;
  Notification  *m_release_locks;
  Notification  *m_lock_blocked;
  Notification  *m_lock_released;
  bool           m_ignore_notify;
  MDL_context    m_mdl_context;
};


void MDL_thread::run()
{
  MDL_request request;
  MDL_request global_request;
  MDL_request_list request_list;
  global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                      MDL_TRANSACTION);
  request.init(MDL_key::TABLE, db_name, m_table_name, m_mdl_type,
               MDL_TRANSACTION);

  request_list.push_front(&request);
  if (m_mdl_type >= MDL_SHARED_UPGRADABLE)
    request_list.push_front(&global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&request_list, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, m_table_name, m_mdl_type));

  // Tell the main thread that we have grabbed our locks.
  m_lock_grabbed->notify();
  // Hold on to locks until we are told to release them
  m_release_locks->wait_for_notification();

  m_mdl_context.release_transactional_locks();

  // Tell the main thread that grabbed lock is released.
  if (m_lock_released)
    m_lock_released->notify();
}

// Google Test recommends DeathTest suffix for classes use in death tests.
typedef MDLTest MDLDeathTest;


/*
  Verifies that we die with a DBUG_ASSERT if we destry a non-empty MDL_context.
 */
#if GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)
TEST_F(MDLDeathTest, DieWhenMTicketsNonempty)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_DEATH(m_mdl_context.destroy(),
               ".*Assertion.*MDL_TRANSACTION.*is_empty.*");
  m_mdl_context.release_transactional_locks();
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)



/*
  The most basic test: just construct and destruct our test fixture.
 */
TEST_F(MDLTest, ConstructAndDestruct)
{
}


void MDLTest::test_one_simple_shared_lock(enum_mdl_type lock_type)
{
  m_request.init(MDL_key::TABLE, db_name, table_name1, lock_type,
                 MDL_TRANSACTION);

  EXPECT_EQ(lock_type, m_request.type);
  EXPECT_EQ(m_null_ticket, m_request.ticket);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_NE(m_null_ticket, m_request.ticket);
  EXPECT_TRUE(m_mdl_context.has_locks());
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, lock_type));

  MDL_request request_2;
  request_2.init(&m_request.key, lock_type, MDL_TRANSACTION);
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_2));
  EXPECT_EQ(m_request.ticket, request_2.ticket);

  m_mdl_context.release_transactional_locks();
  EXPECT_FALSE(m_mdl_context.has_locks());
}


/*
  Acquires one lock of type MDL_SHARED.
 */
TEST_F(MDLTest, OneShared)
{
  test_one_simple_shared_lock(MDL_SHARED);
}


/*
  Acquires one lock of type MDL_SHARED_HIGH_PRIO.
 */
TEST_F(MDLTest, OneSharedHighPrio)
{
  test_one_simple_shared_lock(MDL_SHARED_HIGH_PRIO);
}


/*
  Acquires one lock of type MDL_SHARED_READ.
 */
TEST_F(MDLTest, OneSharedRead)
{
  test_one_simple_shared_lock(MDL_SHARED_READ);
}


/*
  Acquires one lock of type MDL_SHARED_WRITE.
 */
TEST_F(MDLTest, OneSharedWrite)
{
  test_one_simple_shared_lock(MDL_SHARED_WRITE);
}


/*
  Acquires one lock of type MDL_EXCLUSIVE.  
 */
TEST_F(MDLTest, OneExclusive)
{
  const enum_mdl_type lock_type= MDL_EXCLUSIVE;
  m_request.init(MDL_key::TABLE, db_name, table_name1, lock_type,
                 MDL_TRANSACTION);
  EXPECT_EQ(m_null_ticket, m_request.ticket);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));

  EXPECT_NE(m_null_ticket, m_request.ticket);
  EXPECT_NE(m_null_ticket, m_global_request.ticket);
  EXPECT_TRUE(m_mdl_context.has_locks());
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, lock_type));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE));
  EXPECT_TRUE(m_request.ticket->is_upgradable_or_exclusive());

  m_mdl_context.release_transactional_locks();
  EXPECT_FALSE(m_mdl_context.has_locks());
}


/*
  Acquires two locks, on different tables, of type MDL_SHARED.
  Verifies that they are independent.
 */
TEST_F(MDLTest, TwoShared)
{
  MDL_request request_2;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED, MDL_EXPLICIT);
  request_2.init(MDL_key::TABLE, db_name, table_name2, MDL_SHARED, MDL_EXPLICIT);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_2));
  EXPECT_TRUE(m_mdl_context.has_locks());
  ASSERT_NE(m_null_ticket, m_request.ticket);
  ASSERT_NE(m_null_ticket, request_2.ticket);

  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name3, MDL_SHARED));

  m_mdl_context.release_lock(m_request.ticket);
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.has_locks());

  m_mdl_context.release_lock(request_2.ticket);
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.has_locks());
}


/*
  Verifies that two different contexts can acquire a shared lock
  on the same table.
 */
TEST_F(MDLTest, SharedLocksBetweenContexts)
{
  MDL_context  mdl_context2;
  mdl_context2.init(this);
  MDL_request request_2;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  request_2.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_FALSE(mdl_context2.try_acquire_lock(&request_2));

  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(mdl_context2.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));

  m_mdl_context.release_transactional_locks();
  mdl_context2.release_transactional_locks();
}


/*
  Verifies that we can upgrade a shared lock to exclusive.
 */
TEST_F(MDLTest, UpgradeSharedUpgradable)
{
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED_UPGRADABLE,
                 MDL_TRANSACTION);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));
  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock(m_request.ticket, MDL_EXCLUSIVE, long_timeout));
  EXPECT_EQ(MDL_EXCLUSIVE, m_request.ticket->get_type());

  // Another upgrade should be a no-op.
  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock(m_request.ticket, MDL_EXCLUSIVE, long_timeout));
  EXPECT_EQ(MDL_EXCLUSIVE, m_request.ticket->get_type());

  m_mdl_context.release_transactional_locks();
}


/*
  Verifies that only upgradable locks can be upgraded to exclusive.
 */
TEST_F(MDLDeathTest, DieUpgradeShared)
{
  MDL_request request_2;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  request_2.init(MDL_key::TABLE, db_name, table_name2, MDL_SHARED_NO_READ_WRITE,
                 MDL_TRANSACTION);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&request_2);
  m_request_list.push_front(&m_global_request);
  
  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));

#if GTEST_HAS_DEATH_TEST && !defined(DBUG_OFF)
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(m_mdl_context.
                            upgrade_shared_lock(m_request.ticket,
                                                MDL_EXCLUSIVE,
                                                long_timeout),
                            ".*MDL_SHARED_NO_.*");
#endif
  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock(request_2.ticket, MDL_EXCLUSIVE,
                                   long_timeout));
  m_mdl_context.release_transactional_locks();
}


/*
  Verfies that locks are released when we roll back to a savepoint.
 */
TEST_F(MDLTest, SavePoint)
{
  MDL_request request_2;
  MDL_request request_3;
  MDL_request request_4;
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);
  request_2.init(MDL_key::TABLE, db_name, table_name2, MDL_SHARED,
                 MDL_TRANSACTION);
  request_3.init(MDL_key::TABLE, db_name, table_name3, MDL_SHARED,
                 MDL_TRANSACTION);
  request_4.init(MDL_key::TABLE, db_name, table_name4, MDL_SHARED,
                 MDL_TRANSACTION);

  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_2));
  MDL_savepoint savepoint= m_mdl_context.mdl_savepoint();
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_3));
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&request_4));

  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name3, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name4, MDL_SHARED));

  m_mdl_context.rollback_to_savepoint(savepoint);
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name3, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name4, MDL_SHARED));

  m_mdl_context.release_transactional_locks();
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE, db_name, table_name2, MDL_SHARED));
}


/*
  Verifies that we can grab shared locks concurrently, in different threads.
 */
TEST_F(MDLTest, ConcurrentShared)
{
  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_SHARED, &lock_grabbed,
                        &release_locks, NULL, NULL);
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);

  EXPECT_FALSE(m_mdl_context.acquire_lock(&m_request, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE, db_name, table_name1, MDL_SHARED));

  release_locks.notify();
  mdl_thread.join();

  m_mdl_context.release_transactional_locks();
}


/*
  Verifies that we cannot grab an exclusive lock on something which
  is locked with a shared lock in a different thread.
 */
TEST_F(MDLTest, ConcurrentSharedExclusive)
{
  expected_error= ER_LOCK_WAIT_TIMEOUT;

  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_SHARED, &lock_grabbed, &release_locks,
                        NULL, NULL);
  mdl_thread.ignore_notify();
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_EXCLUSIVE,
                 MDL_TRANSACTION);

  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  // We should *not* be able to grab the lock here.
  EXPECT_TRUE(m_mdl_context.acquire_locks(&m_request_list, zero_timeout));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE,
                             db_name, table_name1, MDL_EXCLUSIVE));

  release_locks.notify();
  mdl_thread.join();

  // Now we should be able to grab the lock.
  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, zero_timeout));
  EXPECT_NE(m_null_ticket, m_request.ticket);

  m_mdl_context.release_transactional_locks();
}


/*
  Verifies that we cannot we cannot grab a shared lock on something which
  is locked exlusively in a different thread.
 */
TEST_F(MDLTest, ConcurrentExclusiveShared)
{
  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_EXCLUSIVE,
                        &lock_grabbed, &release_locks, NULL, NULL);
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED,
                 MDL_TRANSACTION);

  // We should *not* be able to grab the lock here.
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_EQ(m_null_ticket, m_request.ticket);

  release_locks.notify();

  // The other thread should eventually release its locks.
  EXPECT_FALSE(m_mdl_context.acquire_lock(&m_request, long_timeout));
  EXPECT_NE(m_null_ticket, m_request.ticket);

  mdl_thread.join();
  m_mdl_context.release_transactional_locks();
}


/*
  Verifies the following scenario:
  Thread 1: grabs a shared upgradable lock.
  Thread 2: grabs a shared lock.
  Thread 1: asks for an upgrade to exclusive (needs to wait for thread 2)
  Thread 2: gets notified, and releases lock.
  Thread 1: gets the exclusive lock.
 */
TEST_F(MDLTest, ConcurrentUpgrade)
{
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED_UPGRADABLE,
                 MDL_TRANSACTION);
  m_request_list.push_front(&m_request);
  m_request_list.push_front(&m_global_request);

  EXPECT_FALSE(m_mdl_context.acquire_locks(&m_request_list, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE,
                            db_name, table_name1, MDL_SHARED_UPGRADABLE));
  EXPECT_FALSE(m_mdl_context.
               is_lock_owner(MDL_key::TABLE,
                             db_name, table_name1, MDL_EXCLUSIVE));

  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_SHARED, &lock_grabbed, &release_locks,
                        NULL, NULL);
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  EXPECT_FALSE(m_mdl_context.
               upgrade_shared_lock(m_request.ticket, MDL_EXCLUSIVE, long_timeout));
  EXPECT_TRUE(m_mdl_context.
              is_lock_owner(MDL_key::TABLE,
                            db_name, table_name1, MDL_EXCLUSIVE));

  mdl_thread.join();
  m_mdl_context.release_transactional_locks();
}


TEST_F(MDLTest, UpgradableConcurrency)
{
  MDL_request request_2;
  MDL_request_list request_list;
  Notification lock_grabbed;
  Notification release_locks;
  MDL_thread mdl_thread(table_name1, MDL_SHARED_UPGRADABLE,
                        &lock_grabbed, &release_locks, NULL, NULL);
  mdl_thread.start();
  lock_grabbed.wait_for_notification();

  // We should be able to take a SW lock.
  m_request.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED_WRITE,
                 MDL_TRANSACTION);
  EXPECT_FALSE(m_mdl_context.try_acquire_lock(&m_request));
  EXPECT_NE(m_null_ticket, m_request.ticket);

  // But SHARED_UPGRADABLE is not compatible with itself
  expected_error= ER_LOCK_WAIT_TIMEOUT;
  request_2.init(MDL_key::TABLE, db_name, table_name1, MDL_SHARED_UPGRADABLE,
                 MDL_TRANSACTION);
  request_list.push_front(&m_global_request);
  request_list.push_front(&request_2);
  EXPECT_TRUE(m_mdl_context.acquire_locks(&request_list, zero_timeout));
  EXPECT_EQ(m_null_ticket, request_2.ticket);

  release_locks.notify();

  mdl_thread.join();
  m_mdl_context.release_transactional_locks();
}


/*
  Verifies following scenario,
  Low priority lock requests starvation. Lock is granted to high priority
  lock request in wait queue always as max_write_lock_count is a large value.
  - max_write_lock_count == default value i.e ~(ulong)0L
  - THREAD 1: Acquires X lock on the table.
  - THREAD 2: Requests for SR lock on the table.
  - THREAD 3: Requests for SW lock on the table.
  - THREAD 4: Requests for SNRW on the table.
  - THREAD 1: Releases X lock.
  - THREAD 5: Requests for SNRW lock on the table.
  - THREAD 4: Releases SNRW lock.
  - THREAD 2,3: Check whether THREADs got lock on the table.
  Though, THREAD 2,3 requested lock before THREAD 4's SNRW lock and
  THREAD 5's SNRW lock, lock is granted for THREAD 4 and 5.
*/
TEST_F(MDLTest, HogLockTest1)
{
  Notification thd_lock_grabbed[5];
  Notification thd_release_locks[5];
  Notification thd_lock_blocked[5];
  Notification thd_lock_released[5];

  /* Locks taken by the threads */
  enum {THD1_X, THD2_SR, THD3_SW, THD4_SNRW, THD5_SNRW};

  /*
    THREAD1:  Acquiring X lock on table.
    Lock Wait Queue: <empty>
    Lock granted: <empty>
  */
  MDL_thread mdl_thread1(table_name1, MDL_EXCLUSIVE, &thd_lock_grabbed[THD1_X],
                         &thd_release_locks[THD1_X], &thd_lock_blocked[THD1_X],
                         &thd_lock_released[THD1_X]);
  mdl_thread1.start();
  thd_lock_grabbed[THD1_X].wait_for_notification();

  /*
    THREAD2:  Requesting SR lock on table.
    Lock Wait Queue: SR
    Lock granted: X
  */
  MDL_thread mdl_thread2(table_name1, MDL_SHARED_READ,
                         &thd_lock_grabbed[THD2_SR],
                         &thd_release_locks[THD2_SR],
                         &thd_lock_blocked[THD2_SR],
                         &thd_lock_released[THD2_SR]);
  mdl_thread2.start();
  thd_lock_blocked[THD2_SR].wait_for_notification();

  /*
    THREAD3:  Requesting SW lock on table.
    Lock Wait Queue: SR<--SW
    Lock granted: X
  */
  MDL_thread mdl_thread3(table_name1, MDL_SHARED_WRITE,
                         &thd_lock_grabbed[THD3_SW],
                         &thd_release_locks[THD3_SW],
                         &thd_lock_blocked[THD3_SW],
                         &thd_lock_released[THD3_SW]);
  mdl_thread3.start();
  thd_lock_blocked[THD3_SW].wait_for_notification();

  /*
    THREAD4:  Requesting SNRW lock on table.
    Lock Wait Queue: SR<--SW<--SNRW
    Lock granted: X
  */
  MDL_thread mdl_thread4(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD4_SNRW],
                         &thd_release_locks[THD4_SNRW],
                         &thd_lock_blocked[THD4_SNRW],
                         &thd_lock_released[THD4_SNRW]);
  mdl_thread4.start();
  thd_lock_blocked[THD4_SNRW].wait_for_notification();

  /* THREAD 1: Release X lock. */
  thd_release_locks[THD1_X].notify();
  thd_lock_released[THD1_X].wait_for_notification();

  /*
    Lock Wait Queue: SR<--SW
    Lock granted: SNRW
  */
  thd_lock_grabbed[THD4_SNRW].wait_for_notification();

  /*
    THREAD 5: Requests SNRW lock on the table.
    Lock Wait Queue: SR<--SW<--SNRW
    Lock granted: SNRW
  */
  MDL_thread mdl_thread5(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD5_SNRW],
                         &thd_release_locks[THD5_SNRW],
                         &thd_lock_blocked[THD5_SNRW],
                         &thd_lock_released[THD5_SNRW]);
  mdl_thread5.start();
  thd_lock_blocked[THD5_SNRW].wait_for_notification();

  /* THREAD 4: Release SNRW lock */
  thd_release_locks[THD4_SNRW].notify();
  thd_lock_released[THD4_SNRW].wait_for_notification();

  /* THREAD 2: Is Lock granted to me? */
  EXPECT_FALSE((mdl_thread2.get_mdl_context()).
               is_lock_owner(MDL_key::TABLE, db_name, table_name1,
                             MDL_SHARED_READ));
  /* THREAD 3: Is Lock granted to me? */
  EXPECT_FALSE((mdl_thread3.get_mdl_context()).
               is_lock_owner(MDL_key::TABLE, db_name, table_name1,
                             MDL_SHARED_WRITE));
  /*
    THREAD 5: Lock is granted to THREAD 5 as priority is higher.
    Lock Wait Queue: SR<--SW
    Lock granted: SNRW
  */
  thd_lock_grabbed[THD5_SNRW].wait_for_notification();
  thd_release_locks[THD5_SNRW].notify();
  thd_lock_released[THD5_SNRW].wait_for_notification();

  /*CLEANUP*/
  thd_lock_grabbed[THD2_SR].wait_for_notification();
  thd_release_locks[THD2_SR].notify();
  thd_lock_released[THD2_SR].wait_for_notification();

  thd_lock_grabbed[THD3_SW].wait_for_notification();
  thd_release_locks[THD3_SW].notify();
  thd_lock_released[THD3_SW].wait_for_notification();

  mdl_thread1.join();
  mdl_thread2.join();
  mdl_thread3.join();
  mdl_thread4.join();
  mdl_thread5.join();
}


/*
  Verifies following scenario,
  After granting max_write_lock_count(=1) number of times for high priority
  lock request, lock is granted to starving low priority lock request
  in wait queue.
  - max_write_lock_count= 1
  - THREAD 1: Acquires X lock on the table.
  - THREAD 2: Requests for SR lock on the table.
  - THREAD 3: Requests for SW lock on the table.
  - THREAD 4: Requests for SNRW on the table.
  - THREAD 1: Releases X lock. m_hog_lock_count= 1
  - THREAD 5: Requests for SNRW lock on the table.
  - THREAD 4: Releases SNRW lock.
  - THREAD 2,3: Release lock.
  While releasing X held by THREAD-1, m_hog_lock_count becomes 1 and while
  releasing SNRW lock in THREAD 4, lock is granted to starving low priority
  locks as m_hog_lock_count == max_write_lock_count.
  So THREAD 2, 3 gets lock here instead of THREAD 5.
*/
TEST_F(MDLTest, HogLockTest2)
{
  Notification thd_lock_grabbed[5];
  Notification thd_release_locks[5];
  Notification thd_lock_blocked[5];
  Notification thd_lock_released[5];
  const ulong org_max_write_lock_count= max_write_lock_count;

  /* Locks taken by the threads */
  enum {THD1_X, THD2_SR, THD3_SW, THD4_SNRW, THD5_SNRW};

  max_write_lock_count= 1;

  /*
    THREAD1:  Acquiring X lock on table.
    Lock Wait Queue: <empty>
    Lock Granted: <empty>
  */
  MDL_thread mdl_thread1(table_name1, MDL_EXCLUSIVE,
                         &thd_lock_grabbed[THD1_X],
                         &thd_release_locks[THD1_X],
                         &thd_lock_blocked[THD1_X],
                         &thd_lock_released[THD1_X]);
  mdl_thread1.start();
  thd_lock_grabbed[THD1_X].wait_for_notification();

  /*
    THREAD2:  Requesting SR lock on table.
    Lock Wait Queue: SR
    Lock Granted: X
  */
  MDL_thread mdl_thread2(table_name1, MDL_SHARED_READ,
                         &thd_lock_grabbed[THD2_SR],
                         &thd_release_locks[THD2_SR],
                         &thd_lock_blocked[THD2_SR],
                         &thd_lock_released[THD2_SR]);
  mdl_thread2.start();
  thd_lock_blocked[THD2_SR].wait_for_notification();

  /*
    THREAD3:  Requesting SW lock on table.
    Lock Wait Queue: SR<--SW
    Lock Granted: X
  */
  MDL_thread mdl_thread3(table_name1, MDL_SHARED_WRITE,
                         &thd_lock_grabbed[THD3_SW],
                         &thd_release_locks[THD3_SW],
                         &thd_lock_blocked[THD3_SW],
                         &thd_lock_released[THD3_SW]);
  mdl_thread3.start();
  thd_lock_blocked[THD3_SW].wait_for_notification();

  /*
    THREAD4:  Requesting SNRW lock on table.
    Lock Wait Queue: SR<--SW<--SNRW
    Lock Granted: X
  */
  MDL_thread mdl_thread4(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD4_SNRW],
                         &thd_release_locks[THD4_SNRW],
                         &thd_lock_blocked[THD4_SNRW],
                         &thd_lock_released[THD4_SNRW]);
  mdl_thread4.start();
  thd_lock_blocked[THD4_SNRW].wait_for_notification();

  /*
     THREAD 1: Release X lock.
     Lock Wait Queue: SR<--SW
     Lock Granted: SNRW
     m_hog_lock_count= 1
  */
  thd_release_locks[THD1_X].notify();
  thd_lock_released[THD1_X].wait_for_notification();

  /* Lock is granted to THREAD 4 */
  thd_lock_grabbed[THD4_SNRW].wait_for_notification();

  /*
    THREAD 5: Requests SNRW lock on the table.
    Lock Wait Queue: SR<--SW<--SNRW
    Lock Granted: SNRW
  */
  MDL_thread mdl_thread5(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD5_SNRW], 
                         &thd_release_locks[THD5_SNRW],
                         &thd_lock_blocked[THD5_SNRW],
                         &thd_lock_released[THD5_SNRW]);
  mdl_thread5.start();
  thd_lock_blocked[THD5_SNRW].wait_for_notification();

  /* THREAD 4: Release SNRW lock */
  thd_release_locks[THD4_SNRW].notify();
  thd_lock_released[THD4_SNRW].wait_for_notification();

  /*
    THREAD 2: Since max_write_lock_count == m_hog_lock_count, Lock is granted to
              THREAD 2 and 3 instead of THREAD 5.
    Lock Wait Queue: SNRW
    Lock Granted: SR, SW
  */
  thd_lock_grabbed[THD2_SR].wait_for_notification();
  thd_lock_grabbed[THD3_SW].wait_for_notification();

  thd_release_locks[THD2_SR].notify();
  thd_lock_released[THD2_SR].wait_for_notification();

  thd_release_locks[THD3_SW].notify();
  thd_lock_released[THD3_SW].wait_for_notification();

  /* Cleanup */
  thd_lock_grabbed[THD5_SNRW].wait_for_notification();
  thd_release_locks[THD5_SNRW].notify();
  thd_lock_released[THD5_SNRW].wait_for_notification();

  mdl_thread1.join();
  mdl_thread2.join();
  mdl_thread3.join();
  mdl_thread4.join();
  mdl_thread5.join();

  max_write_lock_count= org_max_write_lock_count;
}


/*
  Verifies locks priorities,
  X has priority over--> S, SR, SW, SU, (SNW, SNRW)
  SNRW has priority over--> SR, SW
  SNW has priority over--> SW

  - max_write_lock_count contains default value i.e ~(ulong)0L
  - THREAD 1: Acquires X lock on the table.
  - THREAD 2: Requests for S lock on the table.
  - THREAD 3: Requests for SR lock on the table.
  - THREAD 4: Requests for SW lock on the table.
  - THREAD 5: Requests for SU lock on the table.
  - THREAD 6: Requests for SNRW on the table.
  - THREAD 1: Releases X lock.
              Lock is granted THREAD 2, THREAD 5.
  - THREAD 5: RELEASE SU lock.
              Lock is granted to THREAD 6.
  - THREAD 7: Requests for SNW lock on the table.
  - THREAD 6: Releases SNRW lock.
              Lock is granted to THREAD 4 & THREAD 7.
  - THREAD 4: Check whether THREAD got lock on the table.
  At each locks release, locks of equal priorities are granted.
  At the end only SW will be in wait queue as lock is granted to SNW
  lock request.
 */
TEST_F(MDLTest, LockPriorityTest)
{
  Notification thd_lock_grabbed[7];
  Notification thd_release_locks[7];
  Notification thd_lock_blocked[7];
  Notification thd_lock_released[7];

  /* Locks taken by the threads */
  enum {THD1_X, THD2_S, THD3_SR, THD4_SW, THD5_SU, THD6_SNRW, THD7_SNW};

  /*THREAD1:  Acquiring X lock on table */
  MDL_thread mdl_thread1(table_name1, MDL_EXCLUSIVE, &thd_lock_grabbed[THD1_X],
                         &thd_release_locks[THD1_X], &thd_lock_blocked[THD1_X],
                         &thd_lock_released[THD1_X]);
  mdl_thread1.start();
  thd_lock_grabbed[THD1_X].wait_for_notification();

  /*
    THREAD2:  Requesting S lock on table.
    Lock Wait Queue: S
    Lock Granted: X
  */
  MDL_thread mdl_thread2(table_name1, MDL_SHARED, &thd_lock_grabbed[THD2_S],
                         &thd_release_locks[THD2_S], &thd_lock_blocked[THD2_S],
                         &thd_lock_released[THD2_S]);
  mdl_thread2.start();
  thd_lock_blocked[THD2_S].wait_for_notification();

  /*
    THREAD3:  Requesting SR lock on table.
    Lock Wait Queue: S<--SR
    Lock Granted: X
  */
  MDL_thread mdl_thread3(table_name1, MDL_SHARED_READ, &thd_lock_grabbed[THD3_SR],
                         &thd_release_locks[THD3_SR], &thd_lock_blocked[THD3_SR],
                         &thd_lock_released[THD3_SR]);
  mdl_thread3.start();
  thd_lock_blocked[THD3_SR].wait_for_notification();

  /*
    THREAD4:  Requesting SW lock on table.
    Lock Wait Queue: S<--SR<--SW
    Lock Granted: X
  */
  MDL_thread mdl_thread4(table_name1, MDL_SHARED_WRITE,
                         &thd_lock_grabbed[THD4_SW],
                         &thd_release_locks[THD4_SW],
                         &thd_lock_blocked[THD4_SW],
                         &thd_lock_released[THD4_SW]);
  mdl_thread4.start();
  thd_lock_blocked[THD4_SW].wait_for_notification();

  /*
    THREAD5:  Requesting SU lock on table
    Lock Wait Queue: S<--SR<--SW<--SU
    Lock Granted: X
  */
  MDL_thread mdl_thread5(table_name1, MDL_SHARED_UPGRADABLE,
                         &thd_lock_grabbed[THD5_SU],
                         &thd_release_locks[THD5_SU],
                         &thd_lock_blocked[THD5_SU],
                         &thd_lock_released[THD5_SU]);
  mdl_thread5.start();
  thd_lock_blocked[THD5_SU].wait_for_notification();

  /*
    THREAD6:  Requesting SNRW lock on table
    Lock Wait Queue: S<--SR<--SW<--SU<--SNRW
    Lock Granted: X
  */
  MDL_thread mdl_thread6(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD6_SNRW],
                         &thd_release_locks[THD6_SNRW],
                         &thd_lock_blocked[THD6_SNRW],
                         &thd_lock_released[THD6_SNRW]);
  mdl_thread6.start();
  thd_lock_blocked[THD6_SNRW].wait_for_notification();

  /*
    Lock wait Queue status: S<--SR<--SW<--SU<--SNRW
    THREAD 1: Release X lock.
  */
  thd_release_locks[THD1_X].notify();
  thd_lock_released[THD1_X].wait_for_notification();

  /*
    THREAD 5: Verify and Release lock.
    Lock wait Queue status: SR<--SW<--SNRW
    Lock Granted: S, SU
  */
  thd_lock_grabbed[THD2_S].wait_for_notification();
  thd_release_locks[THD2_S].notify();
  thd_lock_released[THD2_S].wait_for_notification();

  thd_lock_grabbed[THD5_SU].wait_for_notification();
  thd_release_locks[THD5_SU].notify();
  thd_lock_released[THD5_SU].wait_for_notification();

  /* Now Lock Granted to THREAD 6 SNRW lock type request*/
  thd_lock_grabbed[THD6_SNRW].wait_for_notification();

  /*
    THREAD 7: Requests SNW lock on the table.
    Lock wait Queue status: SR<--SW<--SNW
    Lock Granted: SNRW
  */
  MDL_thread mdl_thread7(table_name1, MDL_SHARED_NO_WRITE,
                         &thd_lock_grabbed[THD7_SNW],
                         &thd_release_locks[THD7_SNW],
                         &thd_lock_blocked[THD7_SNW],
                         &thd_lock_released[THD7_SNW]);
  mdl_thread7.start();
  thd_lock_blocked[THD7_SNW].wait_for_notification();

  /* THREAD 6: Release SNRW lock */
  thd_release_locks[THD6_SNRW].notify();
  thd_lock_released[THD6_SNRW].wait_for_notification();

  /* Now lock is granted to THREAD 3 & 7 */
  thd_lock_grabbed[THD7_SNW].wait_for_notification();
  thd_lock_grabbed[THD3_SR].wait_for_notification();

  /*
    THREAD 3: Release SR lock
    Lock wait Queue status: SW
    Lock Granted: SR, SNW
  */
  thd_release_locks[THD3_SR].notify();
  thd_lock_released[THD3_SR].wait_for_notification();

  /* THREAD 4: Verify whether lock is granted or not*/
  EXPECT_FALSE((mdl_thread4.get_mdl_context()).
               is_lock_owner(MDL_key::TABLE, db_name, table_name1,
                             MDL_SHARED_WRITE));

  /*CLEANUP*/
  thd_release_locks[THD7_SNW].notify();
  thd_lock_released[THD7_SNW].wait_for_notification();

  thd_lock_grabbed[THD4_SW].wait_for_notification();
  thd_release_locks[THD4_SW].notify();
  thd_lock_released[THD4_SW].wait_for_notification();

  mdl_thread1.join();
  mdl_thread2.join();
  mdl_thread3.join();
  mdl_thread4.join();
  mdl_thread5.join();
  mdl_thread6.join();
  mdl_thread7.join();
}


/*
  Verifies locks priorities when max_write_lock_count= 1
  X has priority over--> S, SR, SW, SU, (SNW, SNRW)
  SNRW has priority over--> SR, SW
  SNW has priority over--> SW

  - max_write_lock_count= 1 
  - THREAD 1: Acquires X lock on the table.
  - THREAD 2: Requests for S lock on the table.
  - THREAD 3: Requests for SR lock on the table.
  - THREAD 4: Requests for SW lock on the table.
  - THREAD 5: Requests for SU lock on the table.
  - THREAD 6: Requests for X on the table.
  - THREAD 1: Releases X lock.
              Lock is granted THREAD 6.
  - THREAD 7: Requests SNRW lock.
  - THREAD 6: Releases X lock.
              Lock is granted to THREAD 2,3,4,5.
  - THREAD 7: Check Whether lock is granted or not.
 */
TEST_F(MDLTest, HogLockTest3)
{
  Notification thd_lock_grabbed[7];
  Notification thd_release_locks[7];
  Notification thd_lock_blocked[7];
  Notification thd_lock_released[7];
  const ulong org_max_write_lock_count= max_write_lock_count;

  enum {THD1_X, THD2_S, THD3_SR, THD4_SW, THD5_SU, THD6_X, THD7_SNRW};

  max_write_lock_count= 1;

  /* THREAD1: Acquiring X lock on table. */
  MDL_thread mdl_thread1(table_name1, MDL_EXCLUSIVE, &thd_lock_grabbed[THD1_X],
                         &thd_release_locks[THD1_X], &thd_lock_blocked[THD1_X],
                         &thd_lock_released[THD1_X]);
  mdl_thread1.start();
  thd_lock_grabbed[THD1_X].wait_for_notification();

  /*
    THREAD2: Requesting S lock on table.
    Lock Wait Queue: S
    Lock Granted: X
  */
  MDL_thread mdl_thread2(table_name1, MDL_SHARED, &thd_lock_grabbed[THD2_S],
                         &thd_release_locks[THD2_S], &thd_lock_blocked[THD2_S],
                         &thd_lock_released[THD2_S]);
  mdl_thread2.start();
  thd_lock_blocked[THD2_S].wait_for_notification();

  /*
    THREAD3: Requesting SR lock on table.
    Lock Wait Queue: S<--SR
    Lock Granted: X
  */
  MDL_thread mdl_thread3(table_name1, MDL_SHARED_READ, &thd_lock_grabbed[THD3_SR],
                         &thd_release_locks[THD3_SR], &thd_lock_blocked[THD3_SR],
                         &thd_lock_released[THD3_SR]);
  mdl_thread3.start();
  thd_lock_blocked[THD3_SR].wait_for_notification();

  /*
    THREAD4: Requesting SW lock on table.
    Lock Wait Queue: S<--SR<--SW.
    Lock Granted: X
  */
  MDL_thread mdl_thread4(table_name1, MDL_SHARED_WRITE,
                         &thd_lock_grabbed[THD4_SW],
                         &thd_release_locks[THD4_SW],
                         &thd_lock_blocked[THD4_SW],
                         &thd_lock_released[THD4_SW]);
  mdl_thread4.start();
  thd_lock_blocked[THD4_SW].wait_for_notification();

  /*
    THREAD5: Requesting SU lock on table.
    Lock Wait Queue: S<--SR<--SW<--SU
    Lock Granted: X
  */
  MDL_thread mdl_thread5(table_name1, MDL_SHARED_UPGRADABLE,
                         &thd_lock_grabbed[THD5_SU],
                         &thd_release_locks[THD5_SU],
                         &thd_lock_blocked[THD5_SU],
                         &thd_lock_released[THD5_SU]);
  mdl_thread5.start();
  thd_lock_blocked[THD5_SU].wait_for_notification();

  /*
    THREAD6: Requesting X lock on table
    Lock Wait Queue: S<--SR<--SW<--SU<--X
    Lock Granted: X
  */
  MDL_thread mdl_thread6(table_name1, MDL_EXCLUSIVE, &thd_lock_grabbed[THD6_X],
                         &thd_release_locks[THD6_X], &thd_lock_blocked[THD6_X],
                         &thd_lock_released[THD6_X]);
  mdl_thread6.start();
  thd_lock_blocked[THD6_X].wait_for_notification();

  /*
    Lock wait Queue status: S<--SR<--SW<--SU<--X
    Lock Granted: X
    THREAD 1: Release X lock.
  */
  thd_release_locks[THD1_X].notify();
  thd_lock_released[THD1_X].wait_for_notification();

  /* Lock is granted to THREAD 6*/
  thd_lock_grabbed[THD6_X].wait_for_notification();

  /*
    THREAD7:  Requesting SNRW lock on table
    Lock wait Queue status: S<--SR<--SW<--SU
    Lock Granted: X
  */
  MDL_thread mdl_thread7(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD7_SNRW],
                         &thd_release_locks[THD7_SNRW],
                         &thd_lock_blocked[THD7_SNRW],
                         &thd_lock_released[THD7_SNRW]);
  mdl_thread7.start();
  thd_lock_blocked[THD7_SNRW].wait_for_notification();

  /* THREAD 6: Release X lock. */
  thd_release_locks[THD6_X].notify();
  thd_lock_released[THD6_X].wait_for_notification();

  /* Lock is granted to THREAD 2, 3, 4, 5*/
  thd_lock_grabbed[THD2_S].wait_for_notification();
  thd_lock_grabbed[THD3_SR].wait_for_notification();
  thd_lock_grabbed[THD4_SW].wait_for_notification();
  thd_lock_grabbed[THD5_SU].wait_for_notification();

  /*
    Lock wait Queue status: <empty>
    Lock Granted: <empty>
    THREAD 7: high priority SNRW lock is still waiting.
  */
  EXPECT_FALSE((mdl_thread7.get_mdl_context()).
               is_lock_owner(MDL_key::TABLE, db_name, table_name1,
                             MDL_SHARED_NO_READ_WRITE));

  /* CLEAN UP */
  thd_release_locks[THD2_S].notify();
  thd_lock_released[THD2_S].wait_for_notification();

  thd_release_locks[THD3_SR].notify();
  thd_lock_released[THD3_SR].wait_for_notification();

  thd_release_locks[THD4_SW].notify();
  thd_lock_released[THD4_SW].wait_for_notification();

  thd_release_locks[THD5_SU].notify();
  thd_lock_released[THD5_SU].wait_for_notification();

  thd_lock_grabbed[THD7_SNRW].wait_for_notification();
  thd_release_locks[THD7_SNRW].notify();
  thd_lock_released[THD7_SNRW].wait_for_notification();

  mdl_thread1.join();
  mdl_thread2.join();
  mdl_thread3.join();
  mdl_thread4.join();
  mdl_thread5.join();
  mdl_thread6.join();
  mdl_thread7.join();

  max_write_lock_count= org_max_write_lock_count;
}


/*
  Verifies whether m_hog_lock_count is resets or not,
  when there are no low priority lock request.

  - max_write_lock_count= 1
  - THREAD 1: Acquires X lock on the table.
  - THREAD 2: Requests for SU lock on the table.
  - THREAD 3: Requests for X lock on the table.
  - THREAD 1: Releases X lock.
              Lock is granted to THREAD 3
              m_hog_lock_count= 1;
  - THREAD 3: Releases X lock.
              Lock is granted to THRED 2.
              m_hog_lock_count= 0;
  - THREAD 4: Requests for SNRW lock.
  - THREAD 5: Requests for R lock.
  - THREAD 2: Releases SU lock.
              Lock is granted to THREAD 4.
 */
TEST_F(MDLTest, HogLockTest4)
{
  Notification thd_lock_grabbed[5];
  Notification thd_release_locks[5];
  Notification thd_lock_blocked[5];
  Notification thd_lock_released[5];
  const ulong org_max_write_lock_count= max_write_lock_count;

  /* Locks taken by the threads */
  enum {THD1_X, THD2_SU, THD3_X, THD4_SNRW, THD5_SR};

  max_write_lock_count= 1;

  /* THREAD1:  Acquiring X lock on table */
  MDL_thread mdl_thread1(table_name1, MDL_EXCLUSIVE, &thd_lock_grabbed[THD1_X],
                         &thd_release_locks[THD1_X], &thd_lock_blocked[THD1_X],
                         &thd_lock_released[THD1_X]);
  mdl_thread1.start();
  thd_lock_grabbed[THD1_X].wait_for_notification();

  /* THREAD2:  Requesting SU lock on table */
  MDL_thread mdl_thread2(table_name1, MDL_SHARED_UPGRADABLE,
                         &thd_lock_grabbed[THD2_SU],
                         &thd_release_locks[THD2_SU],
                         &thd_lock_blocked[THD2_SU],
                         &thd_lock_released[THD2_SU]);
  mdl_thread2.start();
  thd_lock_blocked[THD2_SU].wait_for_notification();

  /* THREAD3:  Requesting X lock on table */
  MDL_thread mdl_thread3(table_name1, MDL_EXCLUSIVE, &thd_lock_grabbed[THD3_X],
                         &thd_release_locks[THD3_X], &thd_lock_blocked[THD3_X],
                         &thd_lock_released[THD3_X]);
  mdl_thread3.start();
  thd_lock_blocked[THD3_X].wait_for_notification();

  /*
    THREAD1: Release X lock.
    Lock Request Queue: SU<--X
    Lock Grant: X
    m_hog_lock_count= 1
  */
  thd_release_locks[THD1_X].notify();
  thd_lock_released[THD1_X].wait_for_notification();
  /* Lock is granted to THREAD 3 */
  thd_lock_grabbed[THD3_X].wait_for_notification();

  /*
    THREAD3: Release X lock.
    Lock Request Queue: <empty>
    Lock Grant: SU
    m_hog_lock_count= 0
  */
  thd_release_locks[THD3_X].notify();
  thd_lock_released[THD3_X].wait_for_notification();
  /*Lock is granted to THREAD 2 */
  thd_lock_grabbed[THD2_SU].wait_for_notification();

  /*
    THREAD4: Requesting SNRW lock on table.
    Lock Request Queue: SNRW
    Lock Grant: SU
  */
  MDL_thread mdl_thread4(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD4_SNRW],
                         &thd_release_locks[THD4_SNRW],
                         &thd_lock_blocked[THD4_SNRW],
                         &thd_lock_released[THD4_SNRW]);
  mdl_thread4.start();
  thd_lock_blocked[THD4_SNRW].wait_for_notification();

  /*
    THREAD5: Requesting SR lock on table.
    Lock Request Queue: SNRW<--SR
    Lock Grant: SU
  */
  MDL_thread mdl_thread5(table_name1, MDL_SHARED_READ,
                         &thd_lock_grabbed[THD5_SR],
                         &thd_release_locks[THD5_SR],
                         &thd_lock_blocked[THD5_SR],
                         &thd_lock_released[THD5_SR]);
  mdl_thread5.start();
  thd_lock_blocked[THD5_SR].wait_for_notification();

  /* THREAD 2: Release lock. */
  thd_release_locks[THD2_SU].notify();
  thd_lock_released[THD2_SU].wait_for_notification();

  /*
    Lock Request Queue: SR
    Lock Grant: SNRW
    Lock is granted to THREAD 5 if m_hog_lock_count is not reset.
  */
  thd_lock_grabbed[THD4_SNRW].wait_for_notification();

  /* THREAD5: Lock is not granted */
  EXPECT_FALSE((mdl_thread5.get_mdl_context()).
               is_lock_owner(MDL_key::TABLE, db_name, table_name1,
                             MDL_SHARED_READ));
  
  /* CLEAN UP */
  thd_release_locks[THD4_SNRW].notify();
  thd_lock_released[THD4_SNRW].wait_for_notification();

  thd_lock_grabbed[THD5_SR].wait_for_notification();
  thd_release_locks[THD5_SR].notify();
  thd_lock_released[THD5_SR].wait_for_notification();

  mdl_thread1.join();
  mdl_thread2.join();
  mdl_thread3.join();
  mdl_thread4.join();
  mdl_thread5.join();

  max_write_lock_count= org_max_write_lock_count;
}


/*
  Verifies resetting of m_hog_lock_count when only few of
  the waiting low priority locks are granted and queue has
  some more low priority lock requests in queue.
  m_hog_lock_count should not be reset to 0 when few low priority
  lock requests are granted.

  - max_write_lock_count= 1
  - THREAD 1: Acquires X lock on the table.
  - THREAD 2: Requests for SNW lock on the table.
  - THREAD 3: Requests for SR lock on the table.
  - THREAD 4: Requests for SW lock on the table.
  - THREAD 5: Requests for SU lock on the table.
  - THREAD 1: Releases X lock.
              Lock is granted THREAD 2, 3 as they are of same priority.
  - THREAD 6: Requests for SNRW lock.
  - THREAD 2: Releases SNW lock.
              Lock shoule be granted to THREAD 4, 5 as
              m_hog_lock_count == max_write_lock_count.
  - THREAD 3: Check Whether lock is granted or not.
 */
TEST_F(MDLTest, HogLockTest5)
{
  Notification thd_lock_grabbed[6];
  Notification thd_release_locks[6];
  Notification thd_lock_blocked[6];
  Notification thd_lock_released[6];
  const ulong org_max_write_lock_count= max_write_lock_count;

  /* Locks taken by the threads */
  enum {THD1_X, THD2_SNW, THD3_SR, THD4_SW, THD5_SU, THD6_SNRW};
  max_write_lock_count= 1;

  /* THREAD1:  Acquiring X lock on table. */
  MDL_thread mdl_thread1(table_name1, MDL_EXCLUSIVE, &thd_lock_grabbed[THD1_X],
                         &thd_release_locks[THD1_X], &thd_lock_blocked[THD1_X],
                         &thd_lock_released[THD1_X]);
  mdl_thread1.start();
  thd_lock_grabbed[THD1_X].wait_for_notification();

  /* THREAD2:  Requesting SNW lock on table. */
  MDL_thread mdl_thread2(table_name1, MDL_SHARED_NO_WRITE,
                         &thd_lock_grabbed[THD2_SNW],
                         &thd_release_locks[THD2_SNW],
                         &thd_lock_blocked[THD2_SNW],
                         &thd_lock_released[THD2_SNW]);
  mdl_thread2.start();
  thd_lock_blocked[THD2_SNW].wait_for_notification();

  /* THREAD3:  Requesting SR lock on table. */
  MDL_thread mdl_thread3(table_name1, MDL_SHARED_READ,
                         &thd_lock_grabbed[THD3_SR],
                         &thd_release_locks[THD3_SR],
                         &thd_lock_blocked[THD3_SR],
                         &thd_lock_released[THD3_SR]);
  mdl_thread3.start();
  thd_lock_blocked[THD3_SR].wait_for_notification();

  /* THREAD4:  Requesting SW lock on table. */
  MDL_thread mdl_thread4(table_name1, MDL_SHARED_WRITE,
                         &thd_lock_grabbed[THD4_SW],
                         &thd_release_locks[THD4_SW],
                         &thd_lock_blocked[THD4_SW],
                         &thd_lock_released[THD4_SW]);
  mdl_thread4.start();
  thd_lock_blocked[THD4_SW].wait_for_notification();

  /* THREAD5:  Requesting SNW lock on table. */
  MDL_thread mdl_thread5(table_name1, MDL_SHARED_UPGRADABLE,
                         &thd_lock_grabbed[THD5_SU],
                         &thd_release_locks[THD5_SU],
                         &thd_lock_blocked[THD5_SU],
                         &thd_lock_released[THD5_SU]);
  mdl_thread5.start();
  thd_lock_blocked[THD5_SU].wait_for_notification();

  /*
    Lock wait Queue status: SNW<--SR<--SW<--SU
    Lock Granted: X
    THREAD 1: Release X lock.
  */
  thd_release_locks[THD1_X].notify();
  thd_lock_released[THD1_X].wait_for_notification();

  /*
    Lock wait Queue status: SW<--SU
    Lock Granted: SR, SNW
    Lock is granted for Thread 2, 3
  */
  thd_lock_grabbed[THD2_SNW].wait_for_notification();
  thd_lock_grabbed[THD3_SR].wait_for_notification();

  /*
    THREAD5:  Requesting SNRW lock on table.
    Lock wait Queue status: SW<--SU<--SNRW
    Lock Granted: SR, SNW
  */
  MDL_thread mdl_thread6(table_name1, MDL_SHARED_NO_READ_WRITE,
                         &thd_lock_grabbed[THD6_SNRW],
                         &thd_release_locks[THD6_SNRW],
                         &thd_lock_blocked[THD6_SNRW],
                         &thd_lock_released[THD6_SNRW]);
  mdl_thread6.start();
  thd_lock_blocked[THD6_SNRW].wait_for_notification();


  /* Thread 2: Release SNW lock */
  thd_release_locks[THD2_SNW].notify();
  thd_lock_released[THD2_SNW].wait_for_notification();

  /*
    Lock wait Queue status: SNRW
    Lock Granted: SR, SW, SU
    Lock is granted to Thread 4,5 instead of Thread 6
    THREAD6: Lock is not granted
  */
  EXPECT_FALSE((mdl_thread6.get_mdl_context()).
               is_lock_owner(MDL_key::TABLE, db_name, table_name1,
                             MDL_SHARED_NO_READ_WRITE));

  thd_lock_grabbed[THD4_SW].wait_for_notification();
  thd_release_locks[THD4_SW].notify();
  thd_lock_released[THD4_SW].wait_for_notification();

  thd_lock_grabbed[THD5_SU].wait_for_notification();
  thd_release_locks[THD5_SU].notify();
  thd_lock_released[THD5_SU].wait_for_notification();

  /* CLEANUP */
  thd_lock_grabbed[THD6_SNRW].wait_for_notification();
  thd_release_locks[THD6_SNRW].notify();
  thd_lock_released[THD6_SNRW].wait_for_notification();

  mdl_thread1.join();
  mdl_thread2.join();
  mdl_thread3.join();
  mdl_thread4.join();
  mdl_thread5.join();

  max_write_lock_count= org_max_write_lock_count;
}

}  // namespace
