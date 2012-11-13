/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_LOCK_H
#define PFS_LOCK_H

/**
  @file storage/perfschema/pfs_lock.h
  Performance schema internal locks (declarations).
*/

#include "pfs_atomic.h"

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/**
  State of a free record.
  Values of a free record should not be read by a reader.
  Writers can concurrently attempt to allocate a free record.
*/
#define PFS_LOCK_FREE 0x00
/**
  State of a dirty record.
  Values of a dirty record should not be read by a reader,
  as the record is currently being modified.
  Only one writer, the writer which owns the record, should
  modify the record content.
*/
#define PFS_LOCK_DIRTY 0x01
/**
  State of an allocated record.
  Values of an allocated record are safe to read by a reader.
  A writer may modify some but not all properties of the record:
  only modifying values that can never cause the reader to crash is allowed.
*/
#define PFS_LOCK_ALLOCATED 0x02

#define VERSION_MASK 0xFFFFFFFC
#define STATE_MASK   0x00000003
#define VERSION_INC  4

/**
  A 'lock' protecting performance schema internal buffers.
  This lock is used to mark the state of a record.
  Access to the record is not enforced here,
  it's up to the readers and writers to look at the record state
  before making an actual read or write operation.
*/
struct pfs_lock
{
  /**
    The record internal version and state
    @sa PFS_LOCK_FREE
    @sa PFS_LOCK_DIRTY
    @sa PFS_LOCK_ALLOCATED
    The version number is to transform the 'ABA' problem
    (see http://en.wikipedia.org/wiki/ABA_problem)
    into an 'A(n)BA(n + 1)' problem, where 'n' is the m_version number.
    When the performance schema instrumentation deletes a record,
    then create a different record reusing the same memory allocation,
    the version number is incremented, so that a reader can detect that
    the record was changed. Note that the version number is never
    reset to zero when a new record is created.
    The version number is stored in the high 30 bits.
    The state is stored in the low 2 bits.
  */
  volatile uint32 m_version_state;

  /** Returns true if the record is free. */
  bool is_free(void)
  {
    uint32 copy= m_version_state; /* non volatile copy, and dirty read */
    return ((copy & STATE_MASK) == PFS_LOCK_FREE);
  }

  /** Returns true if the record contains values that can be read. */
  bool is_populated(void)
  {
    uint32 copy= m_version_state; /* non volatile copy, and dirty read */
    return ((copy & STATE_MASK) == PFS_LOCK_ALLOCATED);
  }

  /**
    Execute a free to dirty transition.
    This transition is safe to execute concurrently by multiple writers.
    Only one writer will succeed to acquire the record.
    @return true if the operation succeed
  */
  bool free_to_dirty(void)
  {
    uint32 copy= m_version_state; /* non volatile copy, and dirty read */
    uint32 old_val= (copy & VERSION_MASK) + PFS_LOCK_FREE;
    uint32 new_val= (copy & VERSION_MASK) + PFS_LOCK_DIRTY;

    return (PFS_atomic::cas_u32(&m_version_state, &old_val, new_val));
  }

  /**
    Execute an allocated to dirty transition.
    This transition should be executed by the writer that owns the record,
    before the record is modified.
  */
  void allocated_to_dirty(void)
  {
    uint32 copy= PFS_atomic::load_u32(&m_version_state);
    /* Make sure the record was ALLOCATED. */
    DBUG_ASSERT((copy & STATE_MASK) == PFS_LOCK_ALLOCATED);
    /* Keep the same version, set the DIRTY state */
    uint32 new_val= (copy & VERSION_MASK) + PFS_LOCK_DIRTY;
    /* We own the record, no need to use compare and swap. */
    PFS_atomic::store_u32(&m_version_state, new_val);
  }

  /**
    Execute a dirty to allocated transition.
    This transition should be executed by the writer that owns the record,
    after the record is in a state ready to be read.
  */
  void dirty_to_allocated(void)
  {
    uint32 copy= PFS_atomic::load_u32(&m_version_state);
    /* Make sure the record was DIRTY. */
    DBUG_ASSERT((copy & STATE_MASK) == PFS_LOCK_DIRTY);
    /* Increment the version, set the ALLOCATED state */
    uint32 new_val= (copy & VERSION_MASK) + VERSION_INC + PFS_LOCK_ALLOCATED;
    PFS_atomic::store_u32(&m_version_state, new_val);
  }

  /**
    Initialize a lock to allocated.
    This transition should be executed by the writer that owns the record and the lock,
    after the record is in a state ready to be read.
  */
  void set_allocated(void)
  {
    /* Do not set the version to 0, read the previous value. */
    uint32 copy= PFS_atomic::load_u32(&m_version_state);
    /* Increment the version, set the ALLOCATED state */
    uint32 new_val= (copy & VERSION_MASK) + VERSION_INC + PFS_LOCK_ALLOCATED;
    PFS_atomic::store_u32(&m_version_state, new_val);
  }

  /**
    Execute a dirty to free transition.
    This transition should be executed by the writer that owns the record.
  */
  void dirty_to_free(void)
  {
    uint32 copy= PFS_atomic::load_u32(&m_version_state);
    /* Make sure the record was DIRTY. */
    DBUG_ASSERT((copy & STATE_MASK) == PFS_LOCK_DIRTY);
    /* Keep the same version, set the FREE state */
    uint32 new_val= (copy & VERSION_MASK) + PFS_LOCK_FREE;
    PFS_atomic::store_u32(&m_version_state, new_val);
  }

  /**
    Execute an allocated to free transition.
    This transition should be executed by the writer that owns the record.
  */
  void allocated_to_free(void)
  {
#ifndef DBUG_OFF
    extern volatile bool ready_to_exit;
#endif

    /*
      If this record is not in the ALLOCATED state and the caller is trying
      to free it, this is a bug: the caller is confused,
      and potentially damaging data owned by another thread or object.
      The correct assert to use here to guarantee data integrity is simply:
        DBUG_ASSERT(m_state == PFS_LOCK_ALLOCATED);
      Now, because of Bug#56666 (Race condition between the server main thread
      and the kill server thread), this assert actually fails during shutdown,
      and the failure is legitimate, on concurrent calls to mysql_*_destroy(),
      when destroying the instrumentation of an object ... twice.
      During shutdown this has no consequences for the performance schema,
      so the assert is relaxed with the "|| ready_to_exit" condition as a work
      around until Bug#56666 is fixed.
    */
    uint32 copy= PFS_atomic::load_u32(&m_version_state);
    /* Make sure the record was ALLOCATED. */
    DBUG_ASSERT(((copy & STATE_MASK) == PFS_LOCK_ALLOCATED) || ready_to_exit);
    /* Keep the same version, set the FREE state */
    uint32 new_val= (copy & VERSION_MASK) + PFS_LOCK_FREE;
    PFS_atomic::store_u32(&m_version_state, new_val);
  }

  /**
    Start an optimistic read operation.
    @sa end_optimist_lock.
  */
  void begin_optimistic_lock(struct pfs_lock *copy)
  {
    copy->m_version_state= PFS_atomic::load_u32(&m_version_state);
  }

  /**
    End an optimistic read operation.
    @sa begin_optimist_lock.
    @return true if the data read is safe to use.
  */
  bool end_optimistic_lock(struct pfs_lock *copy)
  {
    /* Check there was valid data to look at. */
    if ((copy->m_version_state & STATE_MASK) != PFS_LOCK_ALLOCATED)
      return false;

    /* Check the version + state has not changed. */
    if (copy->m_version_state != PFS_atomic::load_u32(&m_version_state))
      return false;

    return true;
  }

  uint32 get_version()
  {
    return (PFS_atomic::load_u32(&m_version_state) & VERSION_MASK);
  }
};


/** @} */
#endif

