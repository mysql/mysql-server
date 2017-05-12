/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_MEMORY_PROVIDER_H
#define PFS_MEMORY_PROVIDER_H

#include "my_psi_config.h"

/**
  @file include/pfs_memory_provider.h
  Performance schema instrumentation (declarations).
*/

#ifdef HAVE_PSI_MEMORY_INTERFACE
#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN

#include <stddef.h>

#include "my_macros.h"
#include "mysql/psi/psi_memory.h"

struct PSI_thread;

#define PSI_MEMORY_CALL(M) pfs_ ## M ## _v1

C_MODE_START

void pfs_register_memory_v1
    (const char *category, struct PSI_memory_info_v1 *info, int count);

PSI_memory_key
pfs_memory_alloc_v1
  (PSI_memory_key key, size_t size, PSI_thread **owner);

PSI_memory_key
pfs_memory_realloc_v1
  (PSI_memory_key key, size_t old_size, size_t new_size, PSI_thread **owner);

void pfs_memory_free_v1
  (PSI_memory_key key, size_t size, PSI_thread *owner);

C_MODE_END

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_MEMORY_INTERFACE */

#endif

