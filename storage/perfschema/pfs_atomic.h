/*
  Copyright (c) 2009, 2010 Sun Microsystems, Inc.
  Use is subject to license terms.

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
  static void init();
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
  static inline uint32 load_u32(volatile uint32 *ptr)
  {
    uint32 result;
    rdlock(ptr);
    result= (uint32) my_atomic_load32((int32*) ptr);
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
  static inline void store_u32(volatile uint32 *ptr, uint32 value)
  {
    wrlock(ptr);
    my_atomic_store32((int32*) ptr, (int32) value);
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
  static inline uint32 add_u32(volatile uint32 *ptr, uint32 value)
  {
    uint32 result;
    wrlock(ptr);
    result= (uint32) my_atomic_add32((int32*) ptr, (int32) value);
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

private:
  static my_atomic_rwlock_t m_rwlock_array[256];

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

  static inline void rdlock(volatile void *ptr)
  {
    my_atomic_rwlock_rdlock(get_rwlock(ptr));
  }

  static inline void wrlock(volatile void *ptr)
  {
    my_atomic_rwlock_wrlock(get_rwlock(ptr));
  }

  static inline void rdunlock(volatile void *ptr)
  {
    my_atomic_rwlock_rdunlock(get_rwlock(ptr));
  }

  static inline void wrunlock(volatile void *ptr)
  {
    my_atomic_rwlock_wrunlock(get_rwlock(ptr));
  }
};

#endif

