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

#ifndef MYSQL_MEMORY_H
#define MYSQL_MEMORY_H

/**
  @file mysql/psi/mysql_memory.h
  Instrumentation helpers for memory allocation.
*/

#include "mysql/psi/psi.h"

#ifndef PSI_MEMORY_CALL
#define PSI_MEMORY_CALL(M) PSI_DYNAMIC_CALL(M)
#endif

/**
  @defgroup Memory_instrumentation Memory Instrumentation
  @ingroup Instrumentation_interface
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
  const char *category,
  PSI_memory_info *info,
  int count)
#else
  const char *category MY_ATTRIBUTE((unused)),
  void *info MY_ATTRIBUTE((unused)),
  int count MY_ATTRIBUTE((unused)))
#endif
{
#ifdef HAVE_PSI_MEMORY_INTERFACE
  PSI_MEMORY_CALL(register_memory)(category, info, count);
#endif
}

/** @} (end of group Memory_instrumentation) */

#endif

