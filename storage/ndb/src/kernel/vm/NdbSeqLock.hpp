/* Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_SEQLOCK_HPP
#define NDB_SEQLOCK_HPP

#include <ndb_types.h>
#include "portlib/mt-asm.h"

#define JAM_FILE_ID 251


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


#undef JAM_FILE_ID

#endif
