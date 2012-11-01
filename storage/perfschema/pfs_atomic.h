/* Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_ATOMIC_H
#define PFS_ATOMIC_H

/**
  @file storage/perfschema/pfs_atomic.h
  Atomic operations (declarations).
*/

#include <my_atomic.h>

/** Helper for atomic operations. */
class PFS_atomic
{
public:
  /** Initialise the PFS_atomic component. */
  static void init();
  /** Cleanup the PFS_atomic component. */
  static void cleanup();

  /** Atomic load. */
  static inline int32 load_32(volatile int32 *ptr)
  {
    int32 result;
    rdlock(ptr);
    result= my_atomic_load32(ptr);
    rdunlock(ptr);
    return result;
  }

  /** Atomic load. */
  static inline int64 load_64(volatile int64 *ptr)
  {
    int64 result;
    rdlock(ptr);
    result= my_atomic_load64(ptr);
    rdunlock(ptr);
    return result;
  }

  /** Atomic load. */
  static inline uint32 load_u32(volatile uint32 *ptr)
  {
    uint32 result;
    rdlock(ptr);
    result= (uint32) my_atomic_load32((int32*) ptr);
    rdunlock(ptr);
    return result;
  }

  /** Atomic load. */
  static inline uint64 load_u64(volatile uint64 *ptr)
  {
    uint64 result;
    rdlock(ptr);
    result= (uint64) my_atomic_load64((int64*) ptr);
    rdunlock(ptr);
    return result;
  }

  /** Atomic store. */
  static inline void store_32(volatile int32 *ptr, int32 value)
  {
    wrlock(ptr);
    my_atomic_store32(ptr, value);
    wrunlock(ptr);
  }

  /** Atomic store. */
  static inline void store_64(volatile int64 *ptr, int64 value)
  {
    wrlock(ptr);
    my_atomic_store64(ptr, value);
    wrunlock(ptr);
  }

  /** Atomic store. */
  static inline void store_u32(volatile uint32 *ptr, uint32 value)
  {
    wrlock(ptr);
    my_atomic_store32((int32*) ptr, (int32) value);
    wrunlock(ptr);
  }

  /** Atomic store. */
  static inline void store_u64(volatile uint64 *ptr, uint64 value)
  {
    wrlock(ptr);
    my_atomic_store64((int64*) ptr, (int64) value);
    wrunlock(ptr);
  }

  /** Atomic add. */
  static inline int32 add_32(volatile int32 *ptr, int32 value)
  {
    int32 result;
    wrlock(ptr);
    result= my_atomic_add32(ptr, value);
    wrunlock(ptr);
    return result;
  }

  /** Atomic add. */
  static inline int64 add_64(volatile int64 *ptr, int64 value)
  {
    int64 result;
    wrlock(ptr);
    result= my_atomic_add64(ptr, value);
    wrunlock(ptr);
    return result;
  }

  /** Atomic add. */
  static inline uint32 add_u32(volatile uint32 *ptr, uint32 value)
  {
    uint32 result;
    wrlock(ptr);
    result= (uint32) my_atomic_add32((int32*) ptr, (int32) value);
    wrunlock(ptr);
    return result;
  }

  /** Atomic add. */
  static inline uint64 add_u64(volatile uint64 *ptr, uint64 value)
  {
    uint64 result;
    wrlock(ptr);
    result= (uint64) my_atomic_add64((int64*) ptr, (int64) value);
    wrunlock(ptr);
    return result;
  }

  /** Atomic compare and swap. */
  static inline bool cas_32(volatile int32 *ptr, int32 *old_value,
                            int32 new_value)
  {
    bool result;
    wrlock(ptr);
    result= my_atomic_cas32(ptr, old_value, new_value);
    wrunlock(ptr);
    return result;
  }

  /** Atomic compare and swap. */
  static inline bool cas_64(volatile int64 *ptr, int64 *old_value,
                            int64 new_value)
  {
    bool result;
    wrlock(ptr);
    result= my_atomic_cas64(ptr, old_value, new_value);
    wrunlock(ptr);
    return result;
  }

  /** Atomic compare and swap. */
  static inline bool cas_u32(volatile uint32 *ptr, uint32 *old_value,
                             uint32 new_value)
  {
    bool result;
    wrlock(ptr);
    result= my_atomic_cas32((int32*) ptr, (int32*) old_value,
                            (uint32) new_value);
    wrunlock(ptr);
    return result;
  }

  /** Atomic compare and swap. */
  static inline bool cas_u64(volatile uint64 *ptr, uint64 *old_value,
                             uint64 new_value)
  {
    bool result;
    wrlock(ptr);
    result= my_atomic_cas64((int64*) ptr, (int64*) old_value,
                            (uint64) new_value);
    wrunlock(ptr);
    return result;
  }

private:
  static my_atomic_rwlock_t m_rwlock_array[256];

  /**
    Helper used only with non native atomic implementations.
    @sa MY_ATOMIC_MODE_RWLOCKS
  */
  static inline my_atomic_rwlock_t *get_rwlock(volatile void *ptr)
  {
    /*
      Divide an address by 8 to remove alignment,
      modulo 256 to fall in the array.
    */
    uint index= (((intptr) ptr) >> 3) & 0xFF;
    my_atomic_rwlock_t *result= &m_rwlock_array[index];
    return result;
  }

  /**
    Helper used only with non native atomic implementations.
    @sa MY_ATOMIC_MODE_RWLOCKS
  */
  static inline void rdlock(volatile void *ptr)
  {
    my_atomic_rwlock_rdlock(get_rwlock(ptr));
  }

  /**
    Helper used only with non native atomic implementations.
    @sa MY_ATOMIC_MODE_RWLOCKS
  */
  static inline void wrlock(volatile void *ptr)
  {
    my_atomic_rwlock_wrlock(get_rwlock(ptr));
  }

  /**
    Helper used only with non native atomic implementations.
    @sa MY_ATOMIC_MODE_RWLOCKS
  */
  static inline void rdunlock(volatile void *ptr)
  {
    my_atomic_rwlock_rdunlock(get_rwlock(ptr));
  }

  /**
    Helper used only with non native atomic implementations.
    @sa MY_ATOMIC_MODE_RWLOCKS
  */
  static inline void wrunlock(volatile void *ptr)
  {
    my_atomic_rwlock_wrunlock(get_rwlock(ptr));
  }
};

#endif

