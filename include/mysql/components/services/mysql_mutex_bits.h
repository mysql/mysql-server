/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPONENTS_SERVICES_MYSQL_MUTEX_BITS_H
#define COMPONENTS_SERVICES_MYSQL_MUTEX_BITS_H

/**
  @file
  ABI for instrumented mutexes.
*/

#include <mysql/components/services/thr_mutex_bits.h>

/**
  @defgroup psi_api_mutex Mutex Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  An instrumented mutex structure.
  @sa mysql_mutex_t
*/
struct st_mysql_mutex
{
  /** The real mutex. */
  my_mutex_t m_mutex;
  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c mysql_mutex_t interface.
  */
  struct PSI_mutex *m_psi;
};

/**
  Type of an instrumented mutex.
  @c mysql_mutex_t is a drop-in replacement for @c my_mutex_t.
  @sa mysql_mutex_assert_owner
  @sa mysql_mutex_assert_not_owner
  @sa mysql_mutex_init
  @sa mysql_mutex_lock
  @sa mysql_mutex_unlock
  @sa mysql_mutex_destroy
*/
typedef struct st_mysql_mutex mysql_mutex_t;

/** @} (end of group psi_api_mutex) */

#endif /* COMPONENTS_SERVICES_MYSQL_MUTEX_BITS_H */

