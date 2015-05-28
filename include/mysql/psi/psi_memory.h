/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PSI_MEMORY_H
#define MYSQL_PSI_MEMORY_H

#include "psi_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  @file mysql/psi/psi_memory.h
  Performance schema instrumentation interface.

  @defgroup Instrumentation_interface Instrumentation Interface
  @ingroup Performance_schema
  @{
*/

#ifdef HAVE_PSI_INTERFACE
#ifndef DISABLE_ALL_PSI
#ifndef DISABLE_PSI_MEMORY
#define HAVE_PSI_MEMORY_INTERFACE
#endif /* DISABLE_PSI_MEMORY */
#endif /* DISABLE_ALL_PSI */
#endif /* HAVE_PSI_INTERFACE */

struct PSI_thread;

/**
  Instrumented memory key.
  To instrument memory, a memory key must be obtained using @c register_memory.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_memory_key;

#ifdef HAVE_PSI_1

/**
  @defgroup Group_PSI_v1 Application Binary Interface, version 1
  @ingroup Instrumentation_interface
  @{
*/

/**
  Memory instrument information.
  @since PSI_VERSION_1
  This structure is used to register instrumented memory.
*/
struct PSI_memory_info_v1
{
  /** Pointer to the key assigned to the registered memory. */
  PSI_memory_key *m_key;
  /** The name of the memory instrument to register. */
  const char *m_name;
  /**
    The flags of the socket instrument to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};
typedef struct PSI_memory_info_v1 PSI_memory_info_v1;

/**
  Memory registration API.
  @param category a category name (typically a plugin name)
  @param info an array of memory info to register
  @param count the size of the info array
*/
typedef void (*register_memory_v1_t)
  (const char *category, struct PSI_memory_info_v1 *info, int count);

/**
  Instrument memory allocation.
  @param key the memory instrument key
  @param size the size of memory allocated
  @param[out] owner the memory owner
  @return the effective memory instrument key
*/
typedef PSI_memory_key (*memory_alloc_v1_t)
  (PSI_memory_key key, size_t size, struct PSI_thread ** owner);

/**
  Instrument memory re allocation.
  @param key the memory instrument key
  @param old_size the size of memory previously allocated
  @param new_size the size of memory re allocated
  @param[in, out] owner the memory owner
  @return the effective memory instrument key
*/
typedef PSI_memory_key (*memory_realloc_v1_t)
  (PSI_memory_key key, size_t old_size, size_t new_size, struct PSI_thread ** owner);

/**
  Instrument memory claim.
  @param key the memory instrument key
  @param size the size of memory allocated
  @param[in, out] owner the memory owner
  @return the effective memory instrument key
*/
typedef PSI_memory_key (*memory_claim_v1_t)
  (PSI_memory_key key, size_t size, struct PSI_thread ** owner);

/**
  Instrument memory free.
  @param key the memory instrument key
  @param size the size of memory allocated
  @param owner the memory owner
*/
typedef void (*memory_free_v1_t)
  (PSI_memory_key key, size_t size, struct PSI_thread * owner);

/** @} (end of group Group_PSI_v1) */

#endif /* HAVE_PSI_1 */

#ifdef HAVE_PSI_2
struct PSI_memory_info_v2
{
  int placeholder;
};

#endif /* HAVE_PSI_2 */

#ifdef USE_PSI_1
typedef struct PSI_memory_info_v1 PSI_memory_info;
#endif

#ifdef USE_PSI_2
typedef struct PSI_memory_info_v2 PSI_memory_info;
#endif

/** @} (end of group Instrumentation_interface) */

#ifdef __cplusplus
}
#endif


#endif /* MYSQL_PSI_MEMORY_H */

