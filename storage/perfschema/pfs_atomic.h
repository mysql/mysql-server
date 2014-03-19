/* Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.

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
  /** Atomic load. */
  static inline int32 load_32(int32 *ptr)
  {
    return my_atomic_load32(ptr);
  }

  /** Atomic load. */
  static inline int64 load_64(int64 *ptr)
  {
    return my_atomic_load64(ptr);
  }

  /** Atomic load. */
  static inline uint32 load_u32(uint32 *ptr)
  {
    return (uint32) my_atomic_load32((int32*) ptr);
  }

  /** Atomic load. */
  static inline uint64 load_u64(uint64 *ptr)
  {
    return (uint64) my_atomic_load64((int64*) ptr);
  }

  /** Atomic store. */
  static inline void store_32(int32 *ptr, int32 value)
  {
    my_atomic_store32(ptr, value);
  }

  /** Atomic store. */
  static inline void store_64(int64 *ptr, int64 value)
  {
    my_atomic_store64(ptr, value);
  }

  /** Atomic store. */
  static inline void store_u32(uint32 *ptr, uint32 value)
  {
    my_atomic_store32((int32*) ptr, (int32) value);
  }

  /** Atomic store. */
  static inline void store_u64(uint64 *ptr, uint64 value)
  {
    my_atomic_store64((int64*) ptr, (int64) value);
  }

  /** Atomic add. */
  static inline int32 add_32(int32 *ptr, int32 value)
  {
    return my_atomic_add32(ptr, value);
  }

  /** Atomic add. */
  static inline int64 add_64(int64 *ptr, int64 value)
  {
    return my_atomic_add64(ptr, value);
  }

  /** Atomic add. */
  static inline uint32 add_u32(uint32 *ptr, uint32 value)
  {
    return (uint32) my_atomic_add32((int32*) ptr, (int32) value);
  }

  /** Atomic add. */
  static inline uint64 add_u64(uint64 *ptr, uint64 value)
  {
    return (uint64) my_atomic_add64((int64*) ptr, (int64) value);
  }

  /** Atomic compare and swap. */
  static inline bool cas_32(int32 *ptr, int32 *old_value,
                            int32 new_value)
  {
    return my_atomic_cas32(ptr, old_value, new_value);
  }

  /** Atomic compare and swap. */
  static inline bool cas_64(int64 *ptr, int64 *old_value,
                            int64 new_value)
  {
    return my_atomic_cas64(ptr, old_value, new_value);
  }

  /** Atomic compare and swap. */
  static inline bool cas_u32(uint32 *ptr, uint32 *old_value,
                             uint32 new_value)
  {
    return my_atomic_cas32((int32*) ptr, (int32*) old_value,
                           (uint32) new_value);
  }

  /** Atomic compare and swap. */
  static inline bool cas_u64(uint64 *ptr, uint64 *old_value,
                             uint64 new_value)
  {
    return my_atomic_cas64((int64*) ptr, (int64*) old_value,
                            (uint64) new_value);
  }
};

#endif

