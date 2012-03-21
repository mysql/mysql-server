/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MT_LOCK_HPP
#define MT_LOCK_HPP

#include <ndb_global.h>
#include "mt-asm.h"
#include <NdbMutex.h>

struct mt_lock_stat
{
  const void * m_ptr;
  char * m_name;
  Uint32 m_contended_count;
  Uint32 m_spin_count;
};

static void register_lock(const void * ptr, const char * name);
static mt_lock_stat * lookup_lock(const void * ptr);

#ifdef NDB_HAVE_XCNG
template <unsigned SZ>
struct thr_spin_lock
{
  thr_spin_lock(const char * name = 0)
  {
    m_lock = 0;
    register_lock(this, name);
  }

  union {
    volatile Uint32 m_lock;
    char pad[SZ];
  };
};

static
ATTRIBUTE_NOINLINE
void
lock_slow(void * sl, volatile unsigned * val)
{
  mt_lock_stat* s = lookup_lock(sl); // lookup before owning lock

loop:
  Uint32 spins = 0;
  do {
    spins++;
    cpu_pause();
  } while (* val == 1);

  if (unlikely(xcng(val, 1) != 0))
    goto loop;

  if (s)
  {
    s->m_spin_count += spins;
    Uint32 count = ++s->m_contended_count;
    Uint32 freq = (count > 10000 ? 5000 : (count > 20 ? 200 : 1));

    if ((count % freq) == 0)
      printf("%s waiting for lock, contentions: %u spins: %u\n",
             s->m_name, count, s->m_spin_count);
  }
}

template <unsigned SZ>
static
inline
void
lock(struct thr_spin_lock<SZ>* sl)
{
  volatile unsigned* val = &sl->m_lock;
  if (likely(xcng(val, 1) == 0))
    return;

  lock_slow(sl, val);
}

template <unsigned SZ>
static
inline
void
unlock(struct thr_spin_lock<SZ>* sl)
{
  /**
   * Memory barrier here, to make sure all of our stores are visible before
   * the lock release is.
   */
  mb();
  sl->m_lock = 0;
}

template <unsigned SZ>
static
inline
int
trylock(struct thr_spin_lock<SZ>* sl)
{
  volatile unsigned* val = &sl->m_lock;
  return xcng(val, 1);
}
#else
#define thr_spin_lock thr_mutex
#endif

template <unsigned SZ>
struct thr_mutex
{
  thr_mutex(const char * name = 0) {
    NdbMutex_Init(&m_mutex);
    register_lock(this, name);
  }

  union {
    NdbMutex m_mutex;
    char pad[SZ];
  };
};

template <unsigned SZ>
static
inline
void
lock(struct thr_mutex<SZ>* sl)
{
  NdbMutex_Lock(&sl->m_mutex);
}

template <unsigned SZ>
static
inline
void
unlock(struct thr_mutex<SZ>* sl)
{
  NdbMutex_Unlock(&sl->m_mutex);
}

template <unsigned SZ>
static
inline
int
trylock(struct thr_mutex<SZ> * sl)
{
  return NdbMutex_Trylock(&sl->m_mutex);
}

#endif
