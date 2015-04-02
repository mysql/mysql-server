/* Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef MT_LOCK_HPP
#define MT_LOCK_HPP

#include <ndb_global.h>
#include "mt-asm.h"
#include <NdbMutex.h>

#define JAM_FILE_ID 323


struct mt_lock_stat
{
  const void * m_ptr;
  char * m_name;
  Uint32 m_contended_count;
  Uint32 m_spin_count;
};

static void register_lock(const void * ptr, const char * name);

/**
 * We will disable use of spinlocks since it doesn't work properly
 * with realtime settings. Will also provide more stable results in
 * some environments at the expense of a minor optimisation. If
 * desirable to have optimal performance without usage of realtime
 * and always ensuring that each thread runs in its own processor,
 * then enable spinlocks again by removing comment on
 * #ifdef NDB_HAVE_XCNG
 */
#if defined(NDB_HAVE_XCNG) && defined(NDB_USE_SPINLOCK)
static mt_lock_stat * lookup_lock(const void * ptr);
struct thr_spin_lock
{
  thr_spin_lock(const char * name = 0)
  {
    m_lock = 0;
    register_lock(this, name);
  }

  volatile Uint32 m_lock;
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

static
inline
void
lock(struct thr_spin_lock* sl)
{
  volatile unsigned* val = &sl->m_lock;
  if (likely(xcng(val, 1) == 0))
    return;

  lock_slow(sl, val);
}

static
inline
void
unlock(struct thr_spin_lock* sl)
{
  /**
   * Memory barrier here, to make sure all of our stores are visible before
   * the lock release is.
   *
   * NOTE: Bug#13870457 UNNECESSARY STRONG MEMORY BARRIER ...
   *       Suggest that a 'wmb' may have been sufficient here.
   *       However, as spinlocks are not used anymore, 
   *       (see fix for bug#16961971) this will not be fixed.
   */
  mb();
  sl->m_lock = 0;
}

static
inline
int
trylock(struct thr_spin_lock* sl)
{
  volatile unsigned* val = &sl->m_lock;
  return xcng(val, 1);
}
#else
#define thr_spin_lock thr_mutex
#endif

struct thr_mutex
{
  thr_mutex(const char * name = 0) {
    NdbMutex_Init(&m_mutex);
    register_lock(this, name);
  }

  NdbMutex m_mutex;
};

static
inline
void
lock(struct thr_mutex* sl)
{
  NdbMutex_Lock(&sl->m_mutex);
}

static
inline
void
unlock(struct thr_mutex* sl)
{
  NdbMutex_Unlock(&sl->m_mutex);
}

static
inline
int
trylock(struct thr_mutex* sl)
{
  return NdbMutex_Trylock(&sl->m_mutex);
}


#undef JAM_FILE_ID

#endif
