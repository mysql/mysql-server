/* Copyright 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef NDB_SAFE_MUTEX_HPP
#define NDB_SAFE_MUTEX_HPP

#ifdef __WIN__
#include <ndb_global.h>
#include <my_pthread.h>
#else
#include <pthread.h>
#endif
#include <assert.h>
#include <ndb_types.h>
#include <NdbOut.hpp>

/*
 * Recursive mutex with recursion limit >= 1.  Intended for debugging.
 * One should rewrite caller code until limit 1 works.
 *
 * The implementation uses a default mutex.  If limit is > 1 or debug
 * is specified then a recursive mutex is simulated.  Operating system
 * recursive mutex (if any) is not used.  The simulation is several
 * times slower.  There is a unit test testSafeMutex.
 *
 * The caller currently is multi-threaded disk data.  Here it is easy
 * to verify that the mutex is released within a time-slice.
 */

class SafeMutex {
  const char* const m_name;
  const Uint32 m_limit; // error if usage exceeds this
  const bool m_debug;   // use recursive implementation even for limit 1
  const bool m_simple;
  pthread_mutex_t m_mutex;
  pthread_cond_t m_cond;
  pthread_t m_owner;
  bool m_initdone;
  Uint32 m_level;
  Uint32 m_usage;       // max level used so far
  int m_errcode;
  int m_errline;
  int err(int errcode, int errline);
  friend class NdbOut& operator<<(NdbOut&, const SafeMutex&);

public:
  SafeMutex(const char* name, Uint32 limit, bool debug) :
    m_name(name),
    m_limit(limit),
    m_debug(debug),
    m_simple(!(limit > 1 || debug))
  {
    assert(m_limit >= 1),
    m_owner = 0;        // wl4391_todo assuming numeric non-zero
    m_initdone = false;
    m_level = 0;
    m_usage = 0;
    m_errcode = 0;
    m_errline = 0;
  };
  ~SafeMutex() {
    if (m_initdone)
      (void)destroy();
  }

  enum {
    // caller must crash on any error - recovery is not possible
    ErrState = -101,    // user error
    ErrLevel = -102,    // level exceeded limit
    ErrOwner = -103,    // unlock when not owner
    ErrNolock = -104    // unlock when no lock
  };
  int create();
  int destroy();
  int lock();
  int unlock();

private:
  int lock_impl();
  int unlock_impl();
};

#endif
