/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_MUTEX_H
#define NDB_MUTEX_H

#include <ndb_global.h>
#include "thr_mutex.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined NDB_MUTEX_STAT && !defined NDB_MUTEX_DEADLOCK_DETECTOR
typedef native_mutex_t NdbMutex;
#else
typedef struct {
  native_mutex_t mutex;
#ifdef NDB_MUTEX_STAT
  unsigned cnt_lock;
  unsigned cnt_lock_contention;
  unsigned cnt_trylock_ok;
  unsigned cnt_trylock_nok;
  unsigned long long min_lock_wait_time_ns;
  unsigned long long sum_lock_wait_time_ns;
  unsigned long long max_lock_wait_time_ns;
  unsigned long long min_hold_time_ns;
  unsigned long long sum_hold_time_ns;
  unsigned long long max_hold_time_ns;
  unsigned long long lock_start_time_ns;
  char name[32];
#endif
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  struct ndb_mutex_state * m_mutex_state;
#endif
} NdbMutex;
#endif

/**
 * Create a mutex
 *  - the allocated mutex should be released by calling
 *    NdbMutex_Destroy 
 *
 * returnvalue: pointer to the mutex structure
 */
NdbMutex* NdbMutex_Create(void);
NdbMutex* NdbMutex_CreateWithName(const char * name);

/**
 * Initialize a mutex created with file-storage or on the stack
 *
 * * p_mutex: pointer to the mutex structure
 * * returnvalue: 0 = succeeded, -1 = failed
 */
int NdbMutex_Init(NdbMutex* p_mutex);
int NdbMutex_InitWithName(NdbMutex* p_mutex, const char * name);

/**
 * Destroy a mutex
 *
 * * p_mutex: pointer to the mutex structure
 * * returnvalue: 0 = succeeded, -1 = failed
 */
int NdbMutex_Destroy(NdbMutex* p_mutex);
int NdbMutex_Deinit(NdbMutex* p_mutex);

/**
 * Lock a mutex
 *
 * * p_mutex: pointer to the mutex structure
 * * returnvalue: 0 = succeeded, -1 = failed
 */
int NdbMutex_Lock(NdbMutex* p_mutex);

/**
 * Unlock a mutex
 *
 * * p_mutex: pointer to the mutex structure
 * * returnvalue: 0 = succeeded, -1 = failed
 */
int NdbMutex_Unlock(NdbMutex* p_mutex);

/**
 * Try to lock a mutex
 *
 * * p_mutex: pointer to the mutex structure
 * * returnvalue: 0 = succeeded, -1 = failed
 */
int NdbMutex_Trylock(NdbMutex* p_mutex);

#ifdef	__cplusplus
}
#endif

#ifdef __cplusplus
class NdbLockable {
  friend class Guard;
  friend class Guard2;
public:
  NdbLockable() { m_mutex = NdbMutex_Create(); }
  ~NdbLockable() { NdbMutex_Destroy(m_mutex); }
  
  void lock() { NdbMutex_Lock(m_mutex); }
  void unlock(){ NdbMutex_Unlock(m_mutex);}
  bool tryLock(){ return NdbMutex_Trylock(m_mutex) == 0;}
  
  NdbMutex* getMutex() {return m_mutex;};

protected:
  NdbMutex * m_mutex;
};

class Guard {
public:
  Guard(NdbMutex *mtx) : m_mtx(mtx) { NdbMutex_Lock(m_mtx); };
  Guard(NdbLockable & l) : m_mtx(l.m_mutex) { NdbMutex_Lock(m_mtx); }; 
  ~Guard() { NdbMutex_Unlock(m_mtx); };
private:
  NdbMutex *m_mtx;
};

class Guard2
{
public:
  Guard2(NdbMutex *mtx) : m_mtx(mtx) { if (m_mtx) NdbMutex_Lock(m_mtx);};
  Guard2(NdbLockable & l) : m_mtx(l.m_mutex) { if(m_mtx)NdbMutex_Lock(m_mtx);};
  ~Guard2() { if (m_mtx) NdbMutex_Unlock(m_mtx); };
private:
  NdbMutex *m_mtx;
};

#endif

#endif
