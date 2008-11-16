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

#ifndef NDB_SAFE_MUTEX_HPP
#define NDB_SAFE_MUTEX_HPP

#include <pthread.h>
#include <assert.h>
#include <ndb_types.h>
#include <NdbOut.hpp>

/*
 * Recursive mutex with a recursion limit >= 1.  Can be useful for
 * debugging.  If a recursive mutex is not wanted, one must rewrite
 * caller code until limit 1 works.
 *
 * Implementation for limit > 1 uses a real OS recursive mutex.  Should
 * work on linux and solaris 10.  There is a unit test testSafeMutex.
 *
 * The caller currently is multi-threaded disk data.  Here it is easy
 * to verify that the mutex is released within a time-slice.
 */

class SafeMutex {
  pthread_mutex_t m_mutex;
  pthread_t m_owner;
  bool m_init;
  Uint32 m_level;
  Uint32 m_usage;       // max level used so far
  const Uint32 m_limit; // error if usage exceeds this
  const bool m_debug;   // use recursive mutex even for limit 1
  friend class NdbOut& operator<<(NdbOut&, const SafeMutex&);

public:
  SafeMutex(Uint32 limit, bool debug) :
    m_limit(limit),
    m_debug(debug)
  {
    assert(m_limit >= 1),
    m_owner = 0;        // wl4391_todo assuming numeric non-zero
    m_init = false;
    m_level = 0;
    m_usage = 0;
  };
  ~SafeMutex() {
    (void)destroy();
  }

  enum {
    // caller must crash on any error
    ErrUnsupp = -101,   // limit > 1 or debug, and not supported by OS
    ErrState = -102,    // user error
    ErrLevel = -103,    // level exceeded limit
    ErrOwner1 = -104,   // owner not 0 at first lock (OS error)
    ErrOwner2 = -105,   // owner not self at recursive lock (OS error)
    ErrOwner3 = -106    // owner not self at unlock (OS error)
  };
  int create();
  int destroy();
  int lock();
  int unlock();
};

#endif
