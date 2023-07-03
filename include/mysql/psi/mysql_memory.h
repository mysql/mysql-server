/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_MEMORY_H
#define MYSQL_MEMORY_H

/**
  @file include/mysql/psi/mysql_memory.h
  Instrumentation helpers for memory allocation.
*/

#include "my_compiler.h"
#include "my_inttypes.h"

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "mysql/psi/psi_memory.h"

#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
/* PSI_MEMORY_CALL() as direct call. */
#include "pfs_memory_provider.h"  // IWYU pragma: keep
#endif

#ifndef PSI_MEMORY_CALL
#define PSI_MEMORY_CALL(M) psi_memory_service->M
#endif

/**
  @defgroup psi_api_memory Memory Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  @def mysql_memory_register(P1, P2, P3)
  Memory registration.
*/
#define mysql_memory_register(P1, P2, P3) \
  inline_mysql_memory_register(P1, P2, P3)

static inline void inline_mysql_memory_register(
#ifdef HAVE_PSI_MEMORY_INTERFACE
    const char *category, PSI_memory_info *info, int count)
#else
    const char *category [[maybe_unused]], void *info [[maybe_unused]],
    int count [[maybe_unused]])
#endif
{
#ifdef HAVE_PSI_MEMORY_INTERFACE
  PSI_MEMORY_CALL(register_memory)(category, info, count);
#endif
}

#ifdef HAVE_PSI_MEMORY_INTERFACE

struct my_memory_header {
  PSI_memory_key m_key;
  uint m_magic;
  size_t m_size;
  PSI_thread *m_owner;
};
typedef struct my_memory_header my_memory_header;

#define PSI_HEADER_SIZE 32

#define PSI_MEMORY_MAGIC 1234

#define PSI_MEM_CNT_BIT ((uint)1 << 31)
#define PSI_REAL_MEM_KEY(P) ((PSI_memory_key)((P) & ~PSI_MEM_CNT_BIT))

#define USER_TO_HEADER(P) ((my_memory_header *)(((char *)P) - PSI_HEADER_SIZE))
#define HEADER_TO_USER(P) (((char *)P) + PSI_HEADER_SIZE)

#define USER_TO_HEADER_UINT8_T(P) \
  (((static_cast<uint8_t *>(P)) - PSI_HEADER_SIZE))

#endif

/** @} (end of group psi_api_memory) */

#endif
