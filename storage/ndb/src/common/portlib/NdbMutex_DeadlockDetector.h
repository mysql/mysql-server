/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MUTEX_DEADLOCK_DETECTOR_H
#define NDB_MUTEX_DEADLOCK_DETECTOR_H

#include <NdbMutex.h>

struct nmdd_mask
{
  unsigned char * m_mask;
  unsigned m_len;
};

struct nmdd_mutex_array
{
  NdbMutex ** m_array;
  unsigned m_used;
  unsigned m_array_len;
};

struct ndb_mutex_state
{
  struct nmdd_mask m_locked_before_mask; /* mutexes held when locking this mutex */
  struct nmdd_mutex_array m_locked_before_list; /* mutexes held when locking this mutex */

  struct nmdd_mutex_array m_locked_after_list; /* mutexes locked when holding this mutex*/
  struct nmdd_mask m_locked_after_mask;        /* mask (for quick check) */

  unsigned m_no; /* my mutex "id" (for access in masks) */
};

struct ndb_mutex_thr_state
{
  struct nmdd_mutex_array m_mutexes_locked;
};

#ifdef	__cplusplus
extern "C" {
#endif

  void NdbMutex_DeadlockDetectorInit();
  void NdbMutex_DeadlockDetectorEnd();

  void ndb_mutex_created(NdbMutex*);
  void ndb_mutex_destoyed(NdbMutex*);
  void ndb_mutex_locked(NdbMutex*);
  void ndb_mutex_unlocked(NdbMutex*);
  void ndb_mutex_try_locked(NdbMutex*);

  void ndb_mutex_thread_init(struct ndb_mutex_thr_state*);
  void ndb_mutex_thread_exit();

#ifdef	__cplusplus
}
#endif


#endif
