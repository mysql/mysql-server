/* Copyright (c) 2009, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_LOCK_H
#define PFS_LOCK_H

/**
  @file storage/perfschema/pfs_lock.h
  Performance schema internal locks (declarations).
*/

#include <atomic>

#include "my_dbug.h"

/* to cause bugs, testing */
// #define MEM(X) std::memory_order_relaxed
/* correct code */
#define MEM(X) X

/**
  @addtogroup performance_schema_buffers
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
#define STATE_MASK 0x00000003
#define VERSION_INC 4

struct pfs_optimistic_state {
  uint32 m_version_state;
};

struct pfs_dirty_state {
  uint32 m_version_state;
};

/**
  A 'lock' protecting performance schema internal buffers.
  This lock is used to mark the state of a record.
  Access to the record is not enforced here,
  it's up to the readers and writers to look at the record state
  before making an actual read or write operation.

  The following state diagram shows the general states maintained by the lock.

  @startuml

  state PFS_LOCK_FREE

  state PFS_LOCK_DIRTY

  state PFS_LOCK_ALLOCATED

  [*] -down-> PFS_LOCK_FREE

  PFS_LOCK_FREE -down-> [*]

  PFS_LOCK_FREE -right-> PFS_LOCK_DIRTY : free_to_dirty()
  PFS_LOCK_DIRTY -right-> PFS_LOCK_ALLOCATED : dirty_to_allocated()

  PFS_LOCK_ALLOCATED --> PFS_LOCK_FREE : allocated_to_free()
  PFS_LOCK_ALLOCATED --> PFS_LOCK_DIRTY : allocated_to_dirty()

  @enduml

  An internal version counter is also incremented, to detect each modification
  done to a record.

  The following diagram represent a fragment of all the states reached,
  for an object creation, modification and destruction.

  @startuml

  state "..." as BEFORE

  state "PFS_LOCK_FREE" as FREE_N
  FREE_N : m_version = N

  state "PFS_LOCK_DIRTY" as DIRTY_N
  DIRTY_N : m_version = N

  state "PFS_LOCK_ALLOCATED" as ALLOCATED_N1
  ALLOCATED_N1 : m_version = N + 1

  state "PFS_LOCK_DIRTY" as DIRTY_N1
  DIRTY_N1 : m_version = N + 1

  state "PFS_LOCK_ALLOCATED" as ALLOCATED_N2
  ALLOCATED_N2: m_version = N + 2

  state "PFS_LOCK_FREE" as FREE_N2
  FREE_N2 : m_version = N + 2

  state "..." as AFTER

  BEFORE -down-> FREE_N
  FREE_N -down-> DIRTY_N : free_to_dirty()
  DIRTY_N -down-> ALLOCATED_N1 : dirty_to_allocated()
  ALLOCATED_N1 -down-> DIRTY_N1 : allocated_to_dirty()
  DIRTY_N1 -down-> ALLOCATED_N2 : dirty_to_allocated()
  ALLOCATED_N2 -down-> FREE_N2 : allocated_to_free()
  FREE_N2 -down-> AFTER

  Note left of ALLOCATED_N1 : State after object creation
  Note left of ALLOCATED_N2 : State after object modification
  Note left of FREE_N2 : State after object destruction

  @enduml
*/
struct pfs_lock {
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
  std::atomic<uint32> m_version_state;

  uint32 copy_version_state() {
    uint32 copy;

    copy = m_version_state; /* dirty read */

    return copy;
  }

  /** Returns true if the record is free. */
  bool is_free(void) {
    uint32 copy = m_version_state.load();

    return ((copy & STATE_MASK) == PFS_LOCK_FREE);
  }

  /** Returns true if the record contains values that can be read. */
  bool is_populated(void) {
    uint32 copy = m_version_state.load();

    return ((copy & STATE_MASK) == PFS_LOCK_ALLOCATED);
  }

  /**
    Execute a free to dirty transition.
    This transition is safe to execute concurrently by multiple writers.
    Only one writer will succeed to acquire the record.
    @return true if the operation succeed
  */
  bool free_to_dirty(pfs_dirty_state *copy_ptr) {
    uint32 old_val = m_version_state.load();

    if ((old_val & STATE_MASK) != PFS_LOCK_FREE) {
      return false;
    }

    uint32 new_val = (old_val & VERSION_MASK) + PFS_LOCK_DIRTY;

    bool pass =
        atomic_compare_exchange_strong(&m_version_state, &old_val, new_val);

    if (pass) {
      copy_ptr->m_version_state = new_val;
    }

    return pass;
  }

  /**
    Execute an allocated to dirty transition.
    This transition should be executed by the writer that owns the record,
    before the record is modified.
  */
  void allocated_to_dirty(pfs_dirty_state *copy_ptr) {
    uint32 copy = copy_version_state();
    /* Make sure the record was ALLOCATED. */
    DBUG_ASSERT((copy & STATE_MASK) == PFS_LOCK_ALLOCATED);
    /* Keep the same version, set the DIRTY state */
    uint32 new_val = (copy & VERSION_MASK) + PFS_LOCK_DIRTY;
    /* We own the record, no need to use compare and swap. */

    m_version_state.store(new_val);

    copy_ptr->m_version_state = new_val;
  }

  /**
    Execute a dirty to allocated transition.
    This transition should be executed by the writer that owns the record,
    after the record is in a state ready to be read.
  */
  void dirty_to_allocated(const pfs_dirty_state *copy) {
    /* Make sure the record was DIRTY. */
    DBUG_ASSERT((copy->m_version_state & STATE_MASK) == PFS_LOCK_DIRTY);
    /* Increment the version, set the ALLOCATED state */
    uint32 new_val = (copy->m_version_state & VERSION_MASK) + VERSION_INC +
                     PFS_LOCK_ALLOCATED;

    m_version_state.store(new_val);
  }

  /**
    Initialize a lock to allocated.
    This transition should be executed by the writer that owns the record and
    the lock,
    after the record is in a state ready to be read.
  */
  void set_allocated(void) {
    /* Do not set the version to 0, read the previous value. */
    uint32 copy = copy_version_state();
    /* Increment the version, set the ALLOCATED state */
    uint32 new_val = (copy & VERSION_MASK) + VERSION_INC + PFS_LOCK_ALLOCATED;

    m_version_state.store(new_val);
  }

  /**
    Initialize a lock to dirty.
  */
  void set_dirty(pfs_dirty_state *copy_ptr) {
    /* Do not set the version to 0, read the previous value. */
    uint32 copy = m_version_state.load();
    /* Increment the version, set the DIRTY state */
    uint32 new_val = (copy & VERSION_MASK) + VERSION_INC + PFS_LOCK_DIRTY;
    m_version_state.store(new_val);

    copy_ptr->m_version_state = new_val;
  }

  /**
    Execute a dirty to free transition.
    This transition should be executed by the writer that owns the record.
  */
  void dirty_to_free(const pfs_dirty_state *copy) {
    /* Make sure the record was DIRTY. */
    DBUG_ASSERT((copy->m_version_state & STATE_MASK) == PFS_LOCK_DIRTY);
    /* Keep the same version, set the FREE state */
    uint32 new_val = (copy->m_version_state & VERSION_MASK) + PFS_LOCK_FREE;

    m_version_state.store(new_val);
  }

  /**
    Execute an allocated to free transition.
    This transition should be executed by the writer that owns the record.
  */
  void allocated_to_free(void) {
    /*
      If this record is not in the ALLOCATED state and the caller is trying
      to free it, this is a bug: the caller is confused,
      and potentially damaging data owned by another thread or object.
    */
    uint32 copy = copy_version_state();
    /* Make sure the record was ALLOCATED. */
    DBUG_ASSERT(((copy & STATE_MASK) == PFS_LOCK_ALLOCATED));
    /* Keep the same version, set the FREE state */
    uint32 new_val = (copy & VERSION_MASK) + PFS_LOCK_FREE;

    m_version_state.store(new_val);
  }

  /**
    Start an optimistic read operation.
    @param [out] copy Saved lock state
    @sa end_optimist_lock.
  */
  void begin_optimistic_lock(pfs_optimistic_state *copy) {
    copy->m_version_state = m_version_state.load();
  }

  /**
    End an optimistic read operation.
    @sa begin_optimist_lock.
    @param copy Saved lock state
    @return true if the data read is safe to use.
  */
  bool end_optimistic_lock(const pfs_optimistic_state *copy) {
    uint32 version_state;

    /* Check there was valid data to look at. */
    if ((copy->m_version_state & STATE_MASK) != PFS_LOCK_ALLOCATED) {
      return false;
    }

    version_state = m_version_state.load();

    /* Check the version + state has not changed. */
    if (copy->m_version_state != version_state) {
      return false;
    }

    return true;
  }

  uint32 get_version() {
    uint32 version_state = m_version_state.load();

    return (version_state & VERSION_MASK);
  }
};

/** @} */
#endif
