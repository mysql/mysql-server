/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "SafeMutex.hpp"

#define JAM_FILE_ID 265

int SafeMutex::create() {
  int ret;
  if (m_initdone) return err(ErrState, __LINE__);
  ret = native_mutex_init(&m_mutex, 0);
  if (ret != 0) return err(ret, __LINE__);
  ret = native_cond_init(&m_cond);
  if (ret != 0) return err(ret, __LINE__);
  m_initdone = true;
  return 0;
}

int SafeMutex::destroy() {
  int ret;
  if (!m_initdone) return err(ErrState, __LINE__);
  ret = native_cond_destroy(&m_cond);
  if (ret != 0) return err(ret, __LINE__);
  ret = native_mutex_destroy(&m_mutex);
  if (ret != 0) return err(ret, __LINE__);
  m_initdone = false;
  return 0;
}

int SafeMutex::lock() {
  int ret;
  if (m_simple) {
    ret = native_mutex_lock(&m_mutex);
    if (ret != 0) return err(ret, __LINE__);
    return 0;
  }
  ret = native_mutex_lock(&m_mutex);
  if (ret != 0) return err(ret, __LINE__);
  return lock_impl();
}

int SafeMutex::lock_impl() {
  int ret;
  my_thread_t self = my_thread_self();
  assert(self != 0);
  while (1) {
    if (m_level == 0) {
      assert(m_owner == 0);
      m_owner = self;
    } else if (m_owner != self) {
      ret = native_cond_wait(&m_cond, &m_mutex);
      if (ret != 0) return err(ret, __LINE__);
      continue;
    }
    if (!(m_level < m_limit)) return err(ErrLevel, __LINE__);
    m_level++;
    if (m_usage < m_level) m_usage = m_level;
    ret = native_cond_signal(&m_cond);
    if (ret != 0) return err(ret, __LINE__);
    ret = native_mutex_unlock(&m_mutex);
    if (ret != 0) return err(ret, __LINE__);
    break;
  }
  return 0;
}

int SafeMutex::unlock() {
  int ret;
  if (m_simple) {
    ret = native_mutex_unlock(&m_mutex);
    if (ret != 0) return err(ret, __LINE__);
    return 0;
  }
  ret = native_mutex_lock(&m_mutex);
  if (ret != 0) return err(ret, __LINE__);
  return unlock_impl();
}

int SafeMutex::unlock_impl() {
  int ret;
  my_thread_t self = my_thread_self();
  assert(self != 0);
  if (m_owner != self) return err(ErrOwner, __LINE__);
  if (m_level == 0) return err(ErrNolock, __LINE__);
  m_level--;
  if (m_level == 0) {
    m_owner = 0;
    ret = native_cond_signal(&m_cond);
    if (ret != 0) return err(ret, __LINE__);
  }
  ret = native_mutex_unlock(&m_mutex);
  if (ret != 0) return err(ret, __LINE__);
  return 0;
}

int SafeMutex::err(int errcode, int errline) {
  assert(errcode != 0);
  m_errcode = errcode;
  m_errline = errline;
  ndbout << *this << endl;
#ifdef UNIT_TEST
  abort();
#endif
  return errcode;
}

NdbOut &operator<<(NdbOut &out, const SafeMutex &sm) {
  out << sm.m_name << ":";
  out << " level=" << sm.m_level;
  out << " usage=" << sm.m_usage;
  if (sm.m_errcode != 0) {
    out << " errcode=" << sm.m_errcode;
    out << " errline=" << sm.m_errline;
  }
  return out;
}

#ifdef UNIT_TEST

struct sm_thr {
  SafeMutex *sm_ptr;
  uint index;
  uint loops;
  uint limit;
  pthread_t id;
  sm_thr() : sm_ptr(0), index(0), loops(0), limit(0), id(0) {}
  ~sm_thr() {}
};

extern "C" {
static void *sm_run(void *arg);
}

static void *sm_run(void *arg) {
  sm_thr &thr = *(sm_thr *)arg;
  assert(thr.sm_ptr != 0);
  SafeMutex &sm = *thr.sm_ptr;
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
      // ndbout << thr.index << ": lock" << endl;
      int ret = sm.lock();
      assert(ret == 0);
      level++;
    } else if (op == -1) {
      // ndbout << thr.index << ": unlock" << endl;
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
  return 0;
}

int main(int argc, char **argv) {
  const uint max_thr = 128;
  struct sm_thr thr[max_thr];

  // threads - loops - max level - debug
  uint num_thr = argc > 1 ? atoi(argv[1]) : 4;
  assert(num_thr != 0 && num_thr <= max_thr);
  uint loops = argc > 2 ? atoi(argv[2]) : 1000000;
  uint limit = argc > 3 ? atoi(argv[3]) : 10;
  assert(limit != 0);
  bool debug = argc > 4 ? atoi(argv[4]) : true;

  ndbout << "threads=" << num_thr;
  ndbout << " loops=" << loops;
  ndbout << " max level=" << limit << endl;

  SafeMutex sm("test-mutex", limit, debug);
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
    void *value;
    pthread_join(thr[i].id, &value);
    ndbout << "join " << i << " id=" << thr[i].id << endl;
  }

  ret = sm.destroy();
  assert(ret == 0);
  return 0;
}

#endif
