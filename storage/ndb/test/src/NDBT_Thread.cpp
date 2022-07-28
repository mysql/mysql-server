/*
   Copyright (c) 2007, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include <ndb_global.h>
#include <NDBT_Thread.hpp>
#include <NdbApi.hpp>

NDBT_Thread::NDBT_Thread()
{
  create(0, -1);
}

NDBT_Thread::NDBT_Thread(NDBT_ThreadSet* thread_set, int thread_no)
{
  create(thread_set, thread_no);
}

void
NDBT_Thread::create(NDBT_ThreadSet* thread_set, int thread_no)
{
  m_magic = NDBT_Thread::Magic;

  m_state = Wait;
  m_thread_set = thread_set;
  m_thread_no = thread_no;
  m_func = 0;
  m_input = 0;
  m_output = 0;
  m_ndb = 0;
  m_err = 0;

  m_mutex = NdbMutex_Create();
  require(m_mutex != 0);
  m_cond = NdbCondition_Create();
  require(m_cond != 0);

  char buf[20];
  sprintf(buf, "NDBT_%04u", (unsigned)thread_no);
  const char* name = strdup(buf);
  require(name != 0);

  unsigned stacksize = 512 * 1024;
  NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_LOW;
  m_thread = NdbThread_Create(NDBT_Thread_run,
                              (void**)this, stacksize, name, prio);
  require(m_thread != 0);
}

NDBT_Thread::~NDBT_Thread()
{
  if (m_thread != 0) {
    NdbThread_Destroy(&m_thread);
    m_thread = 0;
  }
  if (m_cond != 0) {
    NdbCondition_Destroy(m_cond);
    m_cond = 0;
  }
  if (m_mutex != 0) {
    NdbMutex_Destroy(m_mutex);
    m_mutex = 0;
  }

  disconnect();
}

void*
NDBT_Thread_run(void* arg)
{
  require(arg != 0);
  NDBT_Thread& thr = *(NDBT_Thread*)arg;
  require(thr.m_magic == NDBT_Thread::Magic);
  thr.run();
  return 0;
}

void
NDBT_Thread::run()
{
  while (1) {
    lock();
    while (m_state != Start && m_state != Exit) {
      wait();
    }
    if (m_state == Exit) {
      unlock();
      break;
    }
    (*m_func)(*this);
    m_state = Stop;
    signal();
    unlock();
  }
}

// methods for main process

void
NDBT_Thread::start()
{
  lock();
  clear_err();
  m_state = Start;
  signal();
  unlock();
}

void
NDBT_Thread::stop()
{
  lock();
  while (m_state != Stop)
    wait();
  m_state = Wait;
  unlock();
}

void
NDBT_Thread::exit()
{
  lock();
  m_state = Exit;
  signal();
  unlock();
}

void
NDBT_Thread::join()
{
  NdbThread_WaitFor(m_thread, &m_status);
  m_thread = 0;
}

int
NDBT_Thread::connect(class Ndb_cluster_connection* ncc, const char* db)
{
  m_ndb = new Ndb(ncc, db);
  if (m_ndb->init() == -1 ||
      m_ndb->waitUntilReady() == -1) {
    m_err = m_ndb->getNdbError().code;
    return -1;
  }
  return 0;
}

void
NDBT_Thread::disconnect()
{
  delete m_ndb;
  m_ndb = 0;
}

// set of threads

NDBT_ThreadSet::NDBT_ThreadSet(int count)
{
  m_count = count;
  m_thread = new NDBT_Thread* [count];
  for (int n = 0; n < count; n++) {
    m_thread[n] = new NDBT_Thread(this, n);
  }
}

NDBT_ThreadSet::~NDBT_ThreadSet()
{
  for (int n = 0; n < m_count; n++) {
    delete m_thread[n];
    m_thread[n] = 0;
  }
  delete [] m_thread;
}

void
NDBT_ThreadSet::start()
{
  for (int n = 0; n < m_count; n++) {
    NDBT_Thread& thr = *m_thread[n];
    thr.start();
  }
}

void
NDBT_ThreadSet::stop()
{
  for (int n = 0; n < m_count; n++) {
    NDBT_Thread& thr = *m_thread[n];
    thr.stop();
  }
}

void
NDBT_ThreadSet::exit()
{
  for (int n = 0; n < m_count; n++) {
    NDBT_Thread& thr = *m_thread[n];
    thr.exit();
  }
}

void
NDBT_ThreadSet::join()
{
  for (int n = 0; n < m_count; n++) {
    NDBT_Thread& thr = *m_thread[n];
    thr.join();
  }
}

void
NDBT_ThreadSet::set_func(NDBT_ThreadFunc* func)
{
  for (int n = 0; n < m_count; n++) {
    NDBT_Thread& thr = *m_thread[n];
    thr.set_func(func);
  }
}

void
NDBT_ThreadSet::set_input(const void* input)
{
  for (int n = 0; n < m_count; n++) {
    NDBT_Thread& thr = *m_thread[n];
    thr.set_input(input);
  }
}

void
NDBT_ThreadSet::delete_output()
{
  for (int n = 0; n < m_count; n++) {
    if (m_thread[n] != 0) {
      //NDBT_Thread& thr = *m_thread[n];
      //thr.delete_output();
    }
  }
}

int
NDBT_ThreadSet::connect(class Ndb_cluster_connection* ncc, const char* db)
{
  for (int n = 0; n < m_count; n++) {
    require(m_thread[n] != 0);
    NDBT_Thread& thr = *m_thread[n];
    if (thr.connect(ncc, db) == -1)
      return -1;
  }
  return 0;
}

void
NDBT_ThreadSet::disconnect()
{
  for (int n = 0; n < m_count; n++) {
    if (m_thread[n] != 0) {
      NDBT_Thread& thr = *m_thread[n];
      thr.disconnect();
    }
  }
}

int
NDBT_ThreadSet::get_err() const
{
  for (int n = 0; n < m_count; n++) {
    if (m_thread[n] != 0) {
      NDBT_Thread& thr = *m_thread[n];
      int err = thr.get_err();
      if (err != 0)
        return err;
    }
  }
  return 0;
}
