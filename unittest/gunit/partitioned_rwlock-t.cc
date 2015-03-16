/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

/* Simple unit tests for thread id partitioned rwlocks. */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include "auth/partitioned_rwlock.h"

#include <gtest/gtest.h>
#include "thread_utils.h"


namespace partitioned_rwlock_unittest {

using thread::Thread;

TEST(PartitionedRwlock, InitDestroy)
{
  Partitioned_rwlock rwlock;

  bool r= rwlock.init(32
#ifdef HAVE_PSI_INTERFACE
                      , PSI_NOT_INSTRUMENTED
#endif
                      );
  EXPECT_FALSE(r);
  rwlock.destroy();

  r= rwlock.init(8
#ifdef HAVE_PSI_INTERFACE
                 , PSI_NOT_INSTRUMENTED
#endif
                 );
  EXPECT_FALSE(r);
  rwlock.destroy();
}


class Reader_thread : public Thread
{
public:
  void init(uint thread_id, Partitioned_rwlock *rwlock, volatile uint *shared_counter)
  {
    m_thread_id= thread_id;
    m_rwlock= rwlock;
    m_shared_counter= shared_counter;
  }
  virtual void run()
  {
    for (uint i=0; i < 1000; ++i)
    {
      Partitioned_rwlock_read_guard lock(m_rwlock, m_thread_id);
      /*
        With correct rwlock implementation readers should not
        observe counter values not divisible by 100.
      */
      EXPECT_EQ(0U, (*m_shared_counter % 100));
    }
  }
private:
  uint m_thread_id;
  Partitioned_rwlock *m_rwlock;
  volatile uint *m_shared_counter;
};

class Writer_thread : public Thread
{
public:
  Writer_thread(Partitioned_rwlock *rwlock, volatile uint *shared_counter)
    : m_rwlock(rwlock), m_shared_counter(shared_counter)
  {
  }
  virtual void run()
  {
    for (uint i=0; i < 1000; ++i)
    {
      Partitioned_rwlock_write_guard lock(m_rwlock);
      /*
        Add 100 to counter value using 100 single increments. We rely
        on counter being "volatile" to prevent compiler optimizations.
      */
      for (uint j=0; j < 100; ++j)
      {
        ++*m_shared_counter;
      }
    }
  }
private:
  Partitioned_rwlock *m_rwlock;
  volatile uint *m_shared_counter;
};

/**
  Concurrent test which easily breaks if rwlock implementation is wrong
  (e.g. if wrlock() operation doesn't lock all partitions).
*/

TEST(PartitionedRwlock, Concurrent)
{
  const uint PARTS_NUM= 32;

  Partitioned_rwlock rwlock;
  volatile uint shared_counter= 0;
  rwlock.init(PARTS_NUM
#ifdef HAVE_PSI_INTERFACE
              , PSI_NOT_INSTRUMENTED
#endif
              );

  Reader_thread readers[PARTS_NUM];
  Writer_thread writer(&rwlock, &shared_counter);
  for (uint i=0; i < PARTS_NUM; ++i)
    readers[i].init(i, &rwlock, &shared_counter);

  writer.start();
  for (uint i=0; i < PARTS_NUM; ++i)
    readers[i].start();

  for (uint i=0; i < PARTS_NUM; ++i)
    readers[i].join();
  writer.join();

  rwlock.destroy();
}

}
