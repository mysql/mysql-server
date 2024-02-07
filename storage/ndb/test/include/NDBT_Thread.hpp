/*
   Copyright (c) 2007, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_THREAD_HPP
#define NDB_THREAD_HPP

#include <NdbCondition.h>
#include <NdbMutex.h>
#include <NdbThread.h>
#include "util/require.h"

// NDBT_Thread ctor -> NDBT_Thread_run -> thr.run()
extern "C" {
void *NDBT_Thread_run(void *arg);
}

// Function to run in a thread.

typedef void NDBT_ThreadFunc(class NDBT_Thread &);

/*
 * NDBT_Thread
 *
 * Represents a thread.  The thread pauses at startup.
 * Main process sets a function to run.  When the function
 * returns, the thread pauses again to wait for a command.
 * This allows main process to sync with the thread and
 * exchange data with it.
 *
 * Input to thread is typically options.  The input area
 * is read-only in the thread.  Output from thread is
 * results such as statistics.  Error code is handled
 * separately.
 *
 * Pointer to Ndb object and method to create it are
 * provided for convenience.
 */

class NDBT_ThreadSet;

class NDBT_Thread {
 public:
  NDBT_Thread();
  NDBT_Thread(NDBT_ThreadSet *thread_set, int thread_no);
  void create(NDBT_ThreadSet *thread_set, int thread_no);
  ~NDBT_Thread();

  // if part of a set
  inline NDBT_ThreadSet &get_thread_set() const {
    require(m_thread_set != 0);
    return *m_thread_set;
  }
  inline int get_thread_no() const { return m_thread_no; }

  // { Wait -> Start -> Stop }+ -> Exit
  enum State {
    Wait = 1,  // wait for command
    Start,     // run current function
    Stop,      // stopped (paused) when current function done
    Exit       // exit thread
  };

  // tell thread to start running current function
  void start();
  // wait for thread to stop when function is done
  void stop();
  // tell thread to exit
  void exit();
  // collect thread after exit
  void join();

  // set function to run
  inline void set_func(NDBT_ThreadFunc *func) { m_func = func; }

  // input area
  inline void set_input(const void *input) { m_input = input; }
  inline const void *get_input() const { return m_input; }

  // output area
  inline void set_output(void *output) { m_output = output; }
  inline void *get_output() const { return m_output; }
  template <class T>
  inline void set_output() {
    set_output(new T);
  }
#if 0
  inline void delete_output() {
    delete m_output;
    m_output = 0;
  }
#endif

  // thread-specific Ndb object
  inline class Ndb *get_ndb() const { return m_ndb; }
  int connect(class Ndb_cluster_connection *, const char *db = "TEST_DB");
  void disconnect();

  // error code (OS, Ndb, other)
  void clear_err() { m_err = 0; }
  void set_err(int err) { m_err = err; }
  int get_err() const { return m_err; }

 private:
  friend class NDBT_ThreadSet;
  friend void *NDBT_Thread_run(void *arg);

  enum { Magic = 0xabacadae };
  Uint32 m_magic;

  State m_state;
  NDBT_ThreadSet *m_thread_set;
  int m_thread_no;

  NDBT_ThreadFunc *m_func;
  const void *m_input;
  void *m_output;
  class Ndb *m_ndb;
  int m_err;

  // run the thread
  void run();

  void lock() { NdbMutex_Lock(m_mutex); }
  void unlock() { NdbMutex_Unlock(m_mutex); }

  void wait() { NdbCondition_Wait(m_cond, m_mutex); }
  void signal() { NdbCondition_Signal(m_cond); }

  NdbMutex *m_mutex;
  NdbCondition *m_cond;
  NdbThread *m_thread;
  void *m_status;
};

/*
 * A set of threads, indexed from 0 to count-1.  Methods
 * are applied to each thread (serially).  Input area is
 * common to all threads.  Output areas are allocated
 * separately according to a template class.
 */

class NDBT_ThreadSet {
 public:
  NDBT_ThreadSet(int count);
  ~NDBT_ThreadSet();

  inline int get_count() const { return m_count; }
  inline NDBT_Thread &get_thread(int n) {
    require(n < m_count && m_thread[n] != 0);
    return *m_thread[n];
  }

  // tell each thread to start running
  void start();
  // wait for each thread to stop
  void stop();
  // tell each thread to exit
  void exit();
  // collect each thread after exit
  void join();

  // set function to run in each thread
  void set_func(NDBT_ThreadFunc *func);

  // set input area (same instance in each thread)
  void set_input(const void *input);

  // set output areas
  template <class T>
  inline void set_output() {
    for (int n = 0; n < m_count; n++) {
      NDBT_Thread &thr = *m_thread[n];
      thr.set_output<T>();
    }
  }
  void delete_output();

  // thread-specific Ndb objects
  int connect(class Ndb_cluster_connection *, const char *db = "TEST_DB");
  void disconnect();

  int get_err() const;

 private:
  int m_count;
  NDBT_Thread **m_thread;
};

#endif
