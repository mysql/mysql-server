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

#ifndef COMPONENTS_SERVICES_MYSQL_COND_BITS_H
#define COMPONENTS_SERVICES_MYSQL_COND_BITS_H

/**
  @file
  Instrumentation helpers for conditions.
*/

#include "mysql/components/services/thr_cond_bits.h"

/**
  @defgroup psi_api_cond Cond Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  An instrumented cond structure.
  @c mysql_cond_t is a drop-in replacement for @c native_cond_t.
  @sa mysql_cond_init
  @sa mysql_cond_wait
  @sa mysql_cond_timedwait
  @sa mysql_cond_signal
  @sa mysql_cond_broadcast
  @sa mysql_cond_destroy
*/
struct mysql_cond_t
{
  /** The real condition */
  native_cond_t m_cond;
  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c mysql_cond_t interface.
  */
  struct PSI_cond *m_psi;
};

/** @} (end of group psi_api_cond) */

#endif /* COMPONENTS_SERVICES_MYSQL_COND_BITS_H */
