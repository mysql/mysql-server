/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_THREAD_INCLUDED
#define SQL_THREAD_INCLUDED

#include <my_global.h>
#include <my_pthread.h>

namespace thread {

class Thread_start_arg;

/*
  An abstract class for creating/running/joining threads.
  Thread::start() will create a new pthread, and execute the run() function.
*/
class Thread
{
public:
  Thread() : m_thread_id(0)
#ifdef __WIN__
    , m_thread_handle(NULL)
#endif
  {}
  virtual ~Thread();

  /*
    Will create a new pthread, and invoke run();
    Returns the value from pthread_create().
  */
  int start();

  /*
    You may invoke this to wait for the thread to finish.
    You should probalbly join() a thread before deleting it.
  */
  void join();

  // The id of the thread (valid only if it is actually running).
  pthread_t thread_id() const { return m_thread_id; }

  /*
    A wrapper for the run() function.
    Users should *not* call this function directly, they should rather
    invoke the start() function.
  */
  static void run_wrapper(Thread_start_arg*);

protected:
  /*
    Define this function in derived classes.
    Users should *not* call this function directly, they should rather
    invoke the start() function.
  */
  virtual void run() = 0;

private:
  pthread_t m_thread_id;
#ifdef __WIN__
  // We need an open handle to the thread in order to join() it.
  HANDLE m_thread_handle;
#endif

  Thread(const Thread&);                        /* Not copyable. */
  void operator=(const Thread&);                /* Not assignable. */
};


// A simple wrapper around a mutex:
// Grabs the mutex in the CTOR, releases it in the DTOR.
class Mutex_lock
{
public:
  Mutex_lock(mysql_mutex_t *mutex);
  ~Mutex_lock();
private:
  mysql_mutex_t *m_mutex;

  Mutex_lock(const Mutex_lock&);                /* Not copyable. */
  void operator=(const Mutex_lock&);            /* Not assignable. */
};


// A barrier which can be used for one-time synchronization between threads.
class Notification
{
public:
  Notification();
  ~Notification();

  bool has_been_notified();
  void wait_for_notification();
  void notify();
private:
  bool            m_notified;
  mysql_cond_t    m_cond;
  mysql_mutex_t   m_mutex;

  Notification(const Notification&);            /* Not copyable. */
  void operator=(const Notification&);          /* Not assignable. */
};

}  // namespace thread

#endif  // SQL_THREAD_INCLUDED
