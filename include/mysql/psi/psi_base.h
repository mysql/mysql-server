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

#ifndef MYSQL_PSI_BASE_H
#define MYSQL_PSI_BASE_H

#include "my_psi_config.h"

/**
  @file include/mysql/psi/psi_base.h
  Performance schema instrumentation interface.

  @defgroup instrumentation_interface Instrumentation Interface
  @ingroup performance_schema
  @{

    @defgroup psi_api Instrumentation Programming Interface
    @{
    @}

    @defgroup psi_abi Instrumentation Binary Interface
    @{
*/

#define PSI_INSTRUMENT_ME 0

#define PSI_DOCUMENT_ME ""

#define PSI_NOT_INSTRUMENTED 0

/**
  Singleton flag.
  This flag indicate that an instrumentation point is a singleton.
*/
#define PSI_FLAG_SINGLETON (1 << 0)

/**
  Mutable flag.
  This flag indicate that an instrumentation point is a general placeholder,
  that can mutate into a more specific instrumentation point.
*/
#define PSI_FLAG_MUTABLE (1 << 1)

/**
  Per Thread flag.
  This flag indicates the instrumented object is per thread.
  Reserved for future use.
*/
#define PSI_FLAG_THREAD (1 << 2)

/**
  Stage progress flag.
  This flag apply to the stage instruments only.
  It indicates the instrumentation provides progress data.
*/
#define PSI_FLAG_STAGE_PROGRESS (1 << 3)

/**
  Shared Exclusive flag.
  Indicates that rwlock support the shared exclusive state.
*/
#define PSI_FLAG_RWLOCK_SX (1 << 4)

/**
  Transferable flag.
  This flag indicate that an instrumented object can
  be created by a thread and destroyed by another thread.
*/
#define PSI_FLAG_TRANSFER (1 << 5)

/**
  User flag.
  This flag indicate that an instrumented object exists on a
  user or foreground thread. If not set, then the object
  exists on a system or background thread.
*/
#define PSI_FLAG_USER (1 << 6)

/**
  Global stat only flag.
  This flag indicates statistics for the instrument
  are aggregated globally only.
  No per thread / account / user / host aggregations
  are available.
*/
#define PSI_FLAG_ONLY_GLOBAL_STAT (1 << 7)

#define PSI_VOLATILITY_UNKNOWN 0
#define PSI_VOLATILITY_PERMANENT 1
#define PSI_VOLATILITY_PROVISIONING 2
#define PSI_VOLATILITY_DDL 3
#define PSI_VOLATILITY_CACHE 4
#define PSI_VOLATILITY_SESSION 5
#define PSI_VOLATILITY_TRANSACTION 6
#define PSI_VOLATILITY_QUERY 7
#define PSI_VOLATILITY_INTRA_QUERY 8

#define PSI_COUNT_VOLATILITY 9

struct PSI_placeholder
{
  int m_placeholder;
};

/**
    @} (end of group psi_abi)
  @} (end of group instrumentation_interface)
*/

#endif /* MYSQL_PSI_BASE_H */
