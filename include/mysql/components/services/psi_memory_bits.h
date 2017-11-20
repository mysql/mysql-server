/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPONENTS_SERVICES_PSI_MEMORY_BITS_H
#define COMPONENTS_SERVICES_PSI_MEMORY_BITS_H

#include "my_inttypes.h"

/**
  @file
  Performance schema instrumentation interface.
*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
  @defgroup psi_abi_memory Memory Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

/**
  Instrumented memory key.
  To instrument memory, a memory key must be obtained using @c register_memory.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_memory_key;

struct PSI_thread;

/**
  Memory instrument information.
  @since PSI_MEMORY_VERSION_1
  This structure is used to register instrumented memory.
*/
struct PSI_memory_info_v1
{
  /** Pointer to the key assigned to the registered memory. */
  PSI_memory_key *m_key;
  /** The name of the memory instrument to register. */
  const char *m_name;
  /**
    The flags of the memory instrument to register.
    @sa PSI_FLAG_ONLY_GLOBAL_STAT
  */
  uint m_flags;
  /** Volatility index. */
  int m_volatility;
  /** Documentation. */
  const char *m_documentation;
};
typedef struct PSI_memory_info_v1 PSI_memory_info_v1;

/**
  Memory registration API.
  @param category a category name (typically a plugin name)
  @param info an array of memory info to register
  @param count the size of the info array
*/
typedef void (*register_memory_v1_t)(const char *category,
                                     struct PSI_memory_info_v1 *info,
                                     int count);

/**
  Instrument memory allocation.
  @param key the memory instrument key
  @param size the size of memory allocated
  @param[out] owner the memory owner
  @return the effective memory instrument key
*/
typedef PSI_memory_key (*memory_alloc_v1_t)(PSI_memory_key key,
                                            size_t size,
                                            struct PSI_thread **owner);

/**
  Instrument memory re allocation.
  @param key the memory instrument key
  @param old_size the size of memory previously allocated
  @param new_size the size of memory re allocated
  @param[in, out] owner the memory owner
  @return the effective memory instrument key
*/
typedef PSI_memory_key (*memory_realloc_v1_t)(PSI_memory_key key,
                                              size_t old_size,
                                              size_t new_size,
                                              struct PSI_thread **owner);

/**
  Instrument memory claim.
  @param key the memory instrument key
  @param size the size of memory allocated
  @param[in, out] owner the memory owner
  @return the effective memory instrument key
*/
typedef PSI_memory_key (*memory_claim_v1_t)(PSI_memory_key key,
                                            size_t size,
                                            struct PSI_thread **owner);

/**
  Instrument memory free.
  @param key the memory instrument key
  @param size the size of memory allocated
  @param owner the memory owner
*/
typedef void (*memory_free_v1_t)(PSI_memory_key key,
                                 size_t size,
                                 struct PSI_thread *owner);

typedef struct PSI_memory_info_v1 PSI_memory_info;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} (end of group psi_abi_memory) */

#endif /* COMPONENTS_SERVICES_PSI_MEMORY_BITS_H */
