/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

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
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_atomic.cc
  Atomic operations (implementation).
*/

#include <my_global.h>
#include <my_pthread.h>
#include "pfs_atomic.h"

/*
  Using SAFE_MUTEX is impossible, because of recursion.
  - code locks mutex X
    - P_S records the event
      - P_S needs an atomic counter A
        - safe mutex called for m_mutex[hash(A)]
          - safe mutex allocates/free memory
            - safe mutex locks THR_LOCK_malloc
              - P_S records the event
                - P_S needs an atomic counter B
                  - safe mutex called for m_mutex[hash(B)]

  When hash(A) == hash(B), safe_mutex complains rightly that
  the mutex is already locked.
  In some cases, A == B, in particular for events_waits_history_long_index.

  In short, the implementation of PFS_atomic should not cause events
  to be recorded in the performance schema.

  Also, because SAFE_MUTEX redefines pthread_mutex_t, etc,
  this code is not inlined in pfs_atomic.h, but located here in pfs_atomic.cc.

  What is needed is a plain, unmodified, pthread_mutex_t.
  This is provided by my_atomic_rwlock_t.
*/

/**
  Internal rwlock array.
  Using a single rwlock for all atomic operations would be a bottleneck.
  Using a rwlock per performance schema structure would be too costly in
  memory, and use too many rwlock.
  The PFS_atomic implementation computes a hash value from the
  atomic variable, to spread the bottleneck across 256 buckets,
  while still providing --transparently for the caller-- an atomic
  operation.
*/
my_atomic_rwlock_t PFS_atomic::m_rwlock_array[256];

void PFS_atomic::init(void)
{
  uint i;

  for (i=0; i< array_elements(m_rwlock_array); i++)
    my_atomic_rwlock_init(&m_rwlock_array[i]);
}

void PFS_atomic::cleanup(void)
{
  uint i;

  for (i=0; i< array_elements(m_rwlock_array); i++)
    my_atomic_rwlock_destroy(&m_rwlock_array[i]);
}

