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

#ifndef MYSQL_PSI_MEMORY_H
#define MYSQL_PSI_MEMORY_H

#ifndef MYSQL_ABI_CHECK
#include <sys/types.h>
#endif

#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"

/*
  MAINTAINER:
  Note that this file is part of the public API,
  because mysql.h exports
    struct st_mem_root
  See
    - PSI_memory_key st_mem_root::m_psi_key
    - include/mysql.h.pp
*/

/**
  @file include/mysql/psi/psi_memory.h
  Performance schema instrumentation interface.
*/

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

#ifdef HAVE_PSI_INTERFACE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
  @def PSI_MEMORY_VERSION_1
  Performance Schema Memory Interface number for version 1.
  This version is supported.
*/
#define PSI_MEMORY_VERSION_1 1

/**
  @def PSI_MEMORY_VERSION_2
  Performance Schema Memory Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_MEMORY_VERSION_2 2

/**
  @def PSI_CURRENT_MEMORY_VERSION
  Performance Schema Memory Interface number for the most recent version.
  The most current version is @c PSI_MEMORY_VERSION_1
*/
#define PSI_CURRENT_MEMORY_VERSION 1

#ifndef USE_PSI_MEMORY_2
#ifndef USE_PSI_MEMORY_1
#define USE_PSI_MEMORY_1
#endif /* USE_PSI_MEMORY_1 */
#endif /* USE_PSI_MEMORY_2 */

#ifdef USE_PSI_MEMORY_1
#define HAVE_PSI_MEMORY_1
#endif /* USE_PSI_MEMORY_1 */

#ifdef USE_PSI_MEMORY_2
#define HAVE_PSI_MEMORY_2
#endif

struct PSI_thread;

/** Entry point for the performance schema interface. */
struct PSI_memory_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_MEMORY_VERSION_1
    @sa PSI_MEMORY_VERSION_2
    @sa PSI_CURRENT_MEMORY_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_memory_bootstrap PSI_memory_bootstrap;

#ifdef HAVE_PSI_MEMORY_1

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

/**
  Performance Schema Memory Interface, version 1.
  @since PSI_MEMORY_VERSION_1
*/
struct PSI_memory_service_v1
{
  /** @sa register_memory_v1_t. */
  register_memory_v1_t register_memory;
  /** @sa memory_alloc_v1_t. */
  memory_alloc_v1_t memory_alloc;
  /** @sa memory_realloc_v1_t. */
  memory_realloc_v1_t memory_realloc;
  /** @sa memory_claim_v1_t. */
  memory_claim_v1_t memory_claim;
  /** @sa memory_free_v1_t. */
  memory_free_v1_t memory_free;
};

#endif /* HAVE_PSI_MEMORY_1 */

#ifdef USE_PSI_MEMORY_1
typedef struct PSI_memory_service_v1 PSI_memory_service_t;
typedef struct PSI_memory_info_v1 PSI_memory_info;
#else
typedef struct PSI_placeholder PSI_memory_service_t;
typedef struct PSI_placeholder PSI_memory_info;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_memory_service_t *psi_memory_service;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} (end of group psi_abi_memory) */

#endif /* HAVE_PSI_MEMORY_INTERFACE */

#endif /* MYSQL_PSI_MEMORY_H */
