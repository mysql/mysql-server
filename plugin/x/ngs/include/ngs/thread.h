/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_THREAD_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_THREAD_H_

#ifdef NGS_STANDALONE
#include <pthread.h>
#else
#include "mutex_lock.h"
#include "my_thread.h"
#include "plugin/x/src/xpl_performance_schema.h"
#include "thr_cond.h"
#include "thr_mutex.h"
#endif

#include <deque>

#include "plugin/x/ngs/include/ngs_common/bind.h"

namespace ngs {
#ifdef NGS_STANDALONE
#else
typedef my_thread_handle Thread_t;
typedef my_thread_attr_t Thread_attr_t;
typedef my_start_routine Start_routine_t;
#endif

void thread_create(PSI_thread_key key, Thread_t *hread, Start_routine_t func,
                   void *arg);
int thread_join(Thread_t *thread, void **ret);

class Mutex {
 public:
  friend class Cond;

  Mutex(PSI_mutex_key key = PSI_NOT_INSTRUMENTED);
  ~Mutex();

  operator mysql_mutex_t *();

  void lock() { mysql_mutex_lock(&m_mutex); }

  bool try_lock() { return mysql_mutex_trylock(&m_mutex); }

  void unlock() { mysql_mutex_unlock(&m_mutex); }

 private:
  Mutex(const Mutex &);
  Mutex &operator=(const Mutex &);

  mysql_mutex_t m_mutex;
};

class RWLock {
 public:
  RWLock(PSI_rwlock_key key = PSI_NOT_INSTRUMENTED);
  ~RWLock();

  operator mysql_rwlock_t *() { return &m_rwlock; }

  void rlock() { mysql_rwlock_rdlock(&m_rwlock); }

  void wlock() { mysql_rwlock_wrlock(&m_rwlock); }

  bool try_wlock() { return mysql_rwlock_trywrlock(&m_rwlock) == 0; }

  void unlock() { mysql_rwlock_unlock(&m_rwlock); }

 private:
  RWLock(const RWLock &);
  RWLock &operator=(const RWLock &);

  mysql_rwlock_t m_rwlock;
};

class RWLock_readlock {
 public:
  RWLock_readlock(RWLock &lock) : m_lock(lock) { m_lock.rlock(); }

  ~RWLock_readlock() { m_lock.unlock(); }

 private:
  RWLock_readlock(const RWLock_readlock &);
  RWLock_readlock &operator=(RWLock_readlock &);
  RWLock &m_lock;
};

class RWLock_writelock {
 public:
  RWLock_writelock(RWLock &lock, bool try_ = false) : m_lock(lock) {
    if (try_)
      m_locked = m_lock.try_wlock() == 0;
    else {
      m_lock.wlock();
      m_locked = true;
    }
  }

  ~RWLock_writelock() { m_lock.unlock(); }

  bool locked() const { return m_locked; }

 private:
  RWLock_writelock(const RWLock_writelock &);
  RWLock_writelock &operator=(RWLock_writelock &);
  RWLock &m_lock;
  bool m_locked;
};

class Cond {
 public:
  Cond(PSI_cond_key key = PSI_NOT_INSTRUMENTED);
  ~Cond();

  void wait(Mutex &mutex);
  int timed_wait(Mutex &mutex, unsigned long long nanoseconds);
  void signal();
  void signal(Mutex &mutex);
  void broadcast();
  void broadcast(Mutex &mutex);

 private:
  Cond(const Cond &);
  Cond &operator=(const Cond &);

  mysql_cond_t m_cond;
};

template <typename Container, typename Locker, typename Lock>
class Locked_container {
 public:
  Locked_container(Container &container, Lock &lock)
      : m_lock(lock), m_ref(container) {}

  Container &operator*() { return m_ref; }

  Container *operator->() { return &m_ref; }

  Container *container() { return &m_ref; }

 private:
  Locked_container(const Locked_container &);
  Locked_container &operator=(const Locked_container &);

  Locker m_lock;
  Container &m_ref;
};

template <typename Variable_type>
class Sync_variable {
 public:
  Sync_variable(const Variable_type value) : m_value(value) {}

  bool is(const Variable_type value_to_check) {
    MUTEX_LOCK(lock, m_mutex);

    return value_to_check == m_value;
  }

  template <std::size_t NUM_OF_ELEMENTS>
  bool is(const Variable_type (&expected_value)[NUM_OF_ELEMENTS]) {
    MUTEX_LOCK(lock, m_mutex);

    const Variable_type *begin_element = expected_value;
    const Variable_type *end_element = expected_value + NUM_OF_ELEMENTS;

    return find(begin_element, end_element, m_value);
  }

  bool exchange(const Variable_type expected_value,
                const Variable_type new_value) {
    MUTEX_LOCK(lock, m_mutex);

    bool result = false;

    if (expected_value == m_value) {
      m_value = new_value;

      m_cond.signal();
      result = true;
    }

    return result;
  }

  void set(const Variable_type new_value) {
    MUTEX_LOCK(lock, m_mutex);

    m_value = new_value;

    m_cond.signal();
  }

  Variable_type set_and_return_old(const Variable_type new_value) {
    MUTEX_LOCK(lock, m_mutex);

    Variable_type old_value = m_value;
    m_value = new_value;

    m_cond.signal();

    return old_value;
  }

  void wait_for(const Variable_type expected_value) {
    MUTEX_LOCK(lock, m_mutex);

    while (m_value != expected_value) {
      m_cond.wait(m_mutex);
    }
  }

  template <std::size_t NUM_OF_ELEMENTS>
  void wait_for(const Variable_type (&expected_value)[NUM_OF_ELEMENTS]) {
    MUTEX_LOCK(lock, m_mutex);

    const Variable_type *begin_element = expected_value;
    const Variable_type *end_element = expected_value + NUM_OF_ELEMENTS;
    while (!find(begin_element, end_element,
                 m_value))  // std::find doesn't work with (const int*)
    {
      m_cond.wait(m_mutex);
    }
  }

  template <std::size_t NUM_OF_ELEMENTS>
  void wait_for_and_set(const Variable_type (&expected_value)[NUM_OF_ELEMENTS],
                        const Variable_type change_to) {
    MUTEX_LOCK(lock, m_mutex);

    const Variable_type *begin_element = expected_value;
    const Variable_type *end_element = expected_value + NUM_OF_ELEMENTS;
    while (!find(begin_element, end_element,
                 m_value))  // std::find doesn't work with (const int*)
    {
      m_cond.wait(m_mutex);
    }

    if (change_to != m_value) {
      m_value = change_to;
      m_cond.signal();
    }
  }

 protected:
  bool find(const Variable_type *begin, const Variable_type *end,
            const Variable_type to_find) {
    const Variable_type *iterator = begin;
    while (iterator < end) {
      if (to_find == *iterator) return true;

      ++iterator;
    }

    return false;
  }

  Variable_type m_value;
  Mutex m_mutex;
  Cond m_cond;
};

class Wait_for_signal {
 public:
  Wait_for_signal() {
    m_mutex_signal.lock();
    m_mutex_execution.lock();
  }

  ~Wait_for_signal() { m_mutex_signal.unlock(); }

  void wait() {
    m_mutex_execution.unlock();
    m_cond.wait(m_mutex_signal);
  }

  class Signal_when_done {
   public:
    typedef ngs::function<void()> Callback;
    Signal_when_done(Wait_for_signal &signal_variable, Callback callback)
        : m_signal_variable(signal_variable), m_callback(callback) {}

    ~Signal_when_done() { m_signal_variable.signal(); }

    void execute() {
      m_signal_variable.begin_execution_ready();
      m_callback();
      Callback().swap(m_callback);
      m_signal_variable.end_execution_ready();
    }

   private:
    Wait_for_signal &m_signal_variable;
    Callback m_callback;
  };

 protected:
  void begin_execution_ready() { m_mutex_execution.lock(); }

  void end_execution_ready() { m_mutex_execution.unlock(); }

  void signal() { m_cond.signal(m_mutex_signal); }

 private:
  Mutex m_mutex_signal;
  Mutex m_mutex_execution;
  Cond m_cond;
};
}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_THREAD_H_
