/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_SEQLOCK_HPP
#define NDB_SEQLOCK_HPP

#include <ndb_types.h>
#include "mt-asm.h"

#if defined (NDB_HAVE_RMB) && defined(NDB_HAVE_WMB)
struct NdbSeqLock
{
  NdbSeqLock() { m_seq = 0;}
  volatile Uint32 m_seq;

  void write_lock();
  void write_unlock();

  Uint32 read_lock();
  bool read_unlock(Uint32 val) const;
};

inline
void
NdbSeqLock::write_lock()
{
  assert((m_seq & 1) == 0);
  m_seq++;
  wmb();
}

inline
void
NdbSeqLock::write_unlock()
{
  assert((m_seq & 1) == 1);
  wmb();
  m_seq++;
}

inline
Uint32
NdbSeqLock::read_lock()
{
loop:
  Uint32 val = m_seq;
  rmb();
  if (unlikely(val & 1))
  {
#ifdef NDB_HAVE_CPU_PAUSE
    cpu_pause();
#endif
    goto loop;
  }
  return val;
}

inline
bool
NdbSeqLock::read_unlock(Uint32 val) const
{
  rmb();
  return val == m_seq;
}
#else /** ! rmb() or wmb() */
/**
 * Only for ndbd...
 */

struct NdbSeqLock
{
  NdbSeqLock() { }

  void write_lock() {}
  void write_unlock() {}

  Uint32 read_lock() { return 0; }
  bool read_unlock(Uint32 val) const { return true;}
};

#endif

#endif
