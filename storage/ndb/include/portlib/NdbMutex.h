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

#ifndef NDB_MUTEX_H
#define NDB_MUTEX_H

#include <ndb_global.h>

#ifdef NDB_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if defined NDB_WIN32
typedef CRITICAL_SECTION NdbMutex;
#else
#include <pthread.h>
typedef pthread_mutex_t NdbMutex;
#endif

/**
 * Create a mutex
 *
 * p_mutex: pointer to the mutex structure
 * returnvalue: pointer to the mutex structure
 */
NdbMutex* NdbMutex_Create(void);

/**
 * Destroy a mutex
 *
 * * p_mutex: pointer to the mutex structure
 * * returnvalue: 0 = succeeded, -1 = failed
 */
int NdbMutex_Destroy(NdbMutex* p_mutex);

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

#endif

#endif
