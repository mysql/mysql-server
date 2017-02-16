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
#define PFS_LOCK_FREE 0
/**
  State of a dirty record.
  Values of a dirty record should not be read by a reader,
  as the record is currently being modified.
  Only one writer, the writer which owns the record, should
  modify the record content.
*/
#define PFS_LOCK_DIRTY 1
/**
  State of an allocated record.
  Values of an allocated record are safe to read by a reader.
  A writer may modify some but not all properties of the record:
  only modifying values that can never cause the reader to crash is allowed.
*/
#define PFS_LOCK_ALLOCATED 2

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
    The record internal state.
    @sa PFS_LOCK_FREE
    @sa PFS_LOCK_DIRTY
    @sa PFS_LOCK_ALLOCATED
  */
  volatile int32 m_state;
  /**
    The record internal version number.
    This version number is to transform the 'ABA' problem
    (see http://en.wikipedia.org/wiki/ABA_problem)
    into an 'A(n)BA(n + 1)' problem, where 'n' is the m_version number.
    When the performance schema instrumentation deletes a record,
    then create a different record reusing the same memory allocation,
    the version number is incremented, so that a reader can detect that
    the record was changed. Note that the version number is never
    reset to zero when a new record is created.
  */
  volatile uint32 m_version;

  /** Returns true if the record is free. */
  bool is_free(void)
  {
    /* This is a dirty read */
    return (m_state == PFS_LOCK_FREE);
  }

  /** Returns true if the record contains values that can be read. */
  bool is_populated(void)
  {
    int32 copy= m_state; /* non volatile copy, and dirty read */
    return (copy == PFS_LOCK_ALLOCATED);
  }

  /**
    Execute a free to dirty transition.
    This transition is safe to execute concurrently by multiple writers.
    Only one writer will succeed to acquire the record.
    @return true if the operation succeed
  */
  bool free_to_dirty(void)
  {
    int32 old_state= PFS_LOCK_FREE;
    int32 new_state= PFS_LOCK_DIRTY;

    return (PFS_atomic::cas_32(&m_state, &old_state, new_state));
  }

  /**
    Execute a dirty to allocated transition.
    This transition should be executed by the writer that owns the record,
    after the record is in a state ready to be read.
  */
  void dirty_to_allocated(void)
  {
    DBUG_ASSERT(m_state == PFS_LOCK_DIRTY);
    PFS_atomic::add_u32(&m_version, 1);
    PFS_atomic::store_32(&m_state, PFS_LOCK_ALLOCATED);
  }

  /**
    Execute a dirty to free transition.
    This transition should be executed by the writer that owns the record.
  */
  void dirty_to_free(void)
  {
    DBUG_ASSERT(m_state == PFS_LOCK_DIRTY);
    PFS_atomic::store_32(&m_state, PFS_LOCK_FREE);
  }

  /**
    Execute an allocated to free transition.
    This transition should be executed by the writer that owns the record.
  */
  void allocated_to_free(void)
  {
    /*
      If this record is not in the ALLOCATED state and the caller is trying
      to free it, this is a bug: the caller is confused,
      and potentially damaging data owned by another thread or object.
      The correct assert to use here to guarantee data integrity is simply:
        DBUG_ASSERT(m_state == PFS_LOCK_ALLOCATED);
    */
    DBUG_ASSERT(m_state == PFS_LOCK_ALLOCATED);
    PFS_atomic::store_32(&m_state, PFS_LOCK_FREE);
  }

  /**
    Start an optimistic read operation.
    @sa end_optimist_lock.
  */
  void begin_optimistic_lock(struct pfs_lock *copy)
  {
    copy->m_version= PFS_atomic::load_u32(&m_version);
    copy->m_state= PFS_atomic::load_32(&m_state);
  }

  /**
    End an optimistic read operation.
    @sa begin_optimist_lock.
    @return true if the data read is safe to use.
  */
  bool end_optimistic_lock(struct pfs_lock *copy)
  {
    /*
      return true if:
      - the version + state has not changed
      - and there was valid data to look at
    */
    return ((copy->m_version == PFS_atomic::load_u32(&m_version)) &&
            (copy->m_state == PFS_atomic::load_32(&m_state)) &&
            (copy->m_state == PFS_LOCK_ALLOCATED));
  }
};


/** @} */
#endif

