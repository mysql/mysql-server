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

#ifdef __WIN__
#include<Windows.h>
#else
#include <pthread.h>
#endif

#include "runtime.hpp"
#include "yassl_int.hpp"

#include "thread_utils.h"

namespace {

using thread::Notification;
using thread::Thread;

class Yassl_thread : public Thread
{
public:
  Yassl_thread(Notification *go, Notification *done)
    : m_sessions_instance(NULL), m_go(go), m_done(done)
  {}
  virtual void run()
  {
    // Wait until my creator tells me to go.
    m_go->wait_for_notification();
    yaSSL::Sessions &sessions= yaSSL::GetSessions();
    m_sessions_instance= &sessions;
    // Tell my creator I'm done.
    m_done->notify();
  }
  yaSSL::Sessions *m_sessions_instance;
private:
  Notification *m_go;
  Notification *m_done;
};


/**
  Verify that yaSSL::sessionsInstance is indeed a singleton.
  If any of the EXPECT_EQ below reports an error, it is not.
  We can also run 'valgrind ./yassl-t'. If there are errors,
  valgrind will report a multiple of
     sizeof(yaSSL::Sessions) == 80
  bytes lost.
 */
TEST(YasslTest, ManySessions)
{
  Notification go[5];
  Notification done[5];
  Yassl_thread t0(&go[0], &done[0]);
  Yassl_thread t1(&go[1], &done[1]);
  Yassl_thread t2(&go[2], &done[2]);
  Yassl_thread t3(&go[3], &done[3]);
  Yassl_thread t4(&go[4], &done[4]);

  t0.start();
  t1.start();
  t2.start();
  t3.start();
  t4.start();

  for (int ix= 0; ix < 5; ++ix)
    go[ix].notify();

  for (int ix= 0; ix < 5; ++ix)
    done[ix].wait_for_notification();

  // These are most likely to fail unless we use pthread_once.
  EXPECT_EQ(t0.m_sessions_instance, t1.m_sessions_instance);
  EXPECT_EQ(t0.m_sessions_instance, t2.m_sessions_instance);
  EXPECT_EQ(t0.m_sessions_instance, t3.m_sessions_instance);
  EXPECT_EQ(t0.m_sessions_instance, t4.m_sessions_instance);

  // These rarely fail. If they do, we have more than two instances.
  EXPECT_EQ(t1.m_sessions_instance, t2.m_sessions_instance);
  EXPECT_EQ(t1.m_sessions_instance, t3.m_sessions_instance);
  EXPECT_EQ(t1.m_sessions_instance, t4.m_sessions_instance);

  EXPECT_EQ(t2.m_sessions_instance, t3.m_sessions_instance);
  EXPECT_EQ(t2.m_sessions_instance, t4.m_sessions_instance);

  EXPECT_EQ(t3.m_sessions_instance, t4.m_sessions_instance);

  t0.join();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  yaSSL_CleanUp();
}

}
