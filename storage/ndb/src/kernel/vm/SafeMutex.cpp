/* Copyright (C) 2003 MySQL AB

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

#include "SafeMutex.hpp"

//wl4391_todo
#define HAVE_PTHREAD_MUTEX_RECURSIVE 1

NdbOut&
operator<<(NdbOut& out, const SafeMutex& dm)
{
  out << "level=" << dm.m_level << "," << "usage=" << dm.m_usage;
  return out;
}

int
SafeMutex::create()
{
  if (m_init)
    return ErrState;
  int ret = -1;
#ifdef HAVE_PTHREAD_MUTEX_RECURSIVE
  if (m_limit > 1 || m_debug) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    ret = pthread_mutex_init(&m_mutex, &attr);
  } else {
    // error-check mutex does not work right on my linux, skip it
    ret = pthread_mutex_init(&m_mutex, 0);
  }
#else
  if (m_limit > 1 || m_debug)
    return ErrUnsupp;
  ret = pthread_mutex_init(&m_mutex, 0);
#endif
  if (ret != 0)
    return ret;
  m_init = true;
  return 0;
}

int
SafeMutex::destroy()
{
  if (!m_init)
    return ErrState;
  int ret = pthread_mutex_destroy(&m_mutex);
  if (ret != 0)
    return ret;
  m_init = false;
  return 0;
}

int
SafeMutex::lock()
{
  pthread_t self = pthread_self();
  int ret = pthread_mutex_lock(&m_mutex);
  /* have mutex */
  if (ret != 0)
    return ret;
  if (!(m_level < m_limit))
    return ErrLevel;
  m_level++;
  if (m_level > m_usage)
    m_usage = m_level;
  if (m_level == 1 && m_owner != 0)
    return ErrOwner1;
  if (m_level >= 2 && m_owner != self)
    return ErrOwner2;
  m_owner = self;
  return 0;
}

int
SafeMutex::unlock()
{
  pthread_t self = pthread_self();
  if (!(m_level > 0))
    return ErrState;
  if (m_owner != self)
    return ErrOwner3;
  if (m_level == 1)
    m_owner = 0;
  m_level--;
  int ret = pthread_mutex_unlock(&m_mutex);
  /* lose mutex */
  if (ret != 0)
    return ret;
  return 0;
}

#ifdef UNIT_TEST

struct sm_thr {
  SafeMutex* sm_ptr;
  uint index;
  uint loops;
  uint limit;
  pthread_t id;
  sm_thr() : sm_ptr(0), index(0), loops(0), limit(0), id(0) {}
  ~sm_thr() {}
};

extern "C" { static void* sm_run(void* arg); }

static void*
sm_run(void* arg)
{
  sm_thr& thr = *(sm_thr*)arg;
  assert(thr.sm_ptr != 0);
  SafeMutex& sm = *thr.sm_ptr;
  uint level = 0;
  int dir = 0;
  uint i;
  for (i = 0; i < thr.loops; i++) {
    int op = 0;
    uint sel = uint(random()) % 10;
    if (level == 0) {
      dir = +1;
      op = +1;
    } else if (level == thr.limit) {
      dir = -1;
      op = -1;
    } else if (dir == +1) {
      op = sel != 0 ? +1 : -1;
    } else if (dir == -1) {
      op = sel != 0 ? -1 : +1;
    } else {
      assert(false);
    }
    if (op == +1) {
      assert(level < thr.limit);
      int ret = sm.lock();
      assert(ret == 0);
      level++;
    } else if (op == -1) {
      int ret = sm.unlock();
      assert(ret == 0);
      assert(level != 0);
      level--;
    } else {
      assert(false);
    }
  }
  while (level > 0) {
    int ret = sm.unlock();
    assert(ret == 0);
    level--;
  }
}

int
main(int argc, char** argv)
{
  const uint max_thr = 128;
  struct sm_thr thr[max_thr];

  // threads - loops - max level
  uint num_thr = argc > 1 ? atoi(argv[1]) : 4;
  assert(num_thr != 0 && num_thr <= max_thr);
  uint loops = argc > 2 ? atoi(argv[2]) : 1000000;
  uint limit = argc > 3 ? atoi(argv[3]) : 10;
  assert(limit != 0);

  ndbout << "threads=" << num_thr;
  ndbout << " loops=" << loops;
  ndbout << " max level=" << limit << endl;

  SafeMutex sm(limit, true);
  int ret;
  ret = sm.create();
  assert(ret == 0);

  uint i;
  for (i = 0; i < num_thr; i++) {
    thr[i].sm_ptr = &sm;
    thr[i].index = i;
    thr[i].loops = loops;
    thr[i].limit = limit;
    pthread_create(&thr[i].id, 0, &sm_run, &thr[i]);
    ndbout << "create " << i << " id=" << thr[i].id << endl;
  }
  for (i = 0; i < num_thr; i++) {
    void* value;
    pthread_join(thr[i].id, &value);
    ndbout << "join " << i << " id=" << thr[i].id << endl;
  }

  ret = sm.destroy();
  assert(ret == 0);
  return 0;
}

#endif
