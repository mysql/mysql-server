/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifdef __cplusplus
extern "C" {
#endif

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

/**
  Instrumented mutex key.
  To instrument a mutex, a mutex key must be obtained using @c register_mutex.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_mutex_key;

/**
  Instrumented rwlock key.
  To instrument a rwlock, a rwlock key must be obtained
  using @c register_rwlock.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_rwlock_key;

/**
  Instrumented cond key.
  To instrument a condition, a condition key must be obtained
  using @c register_cond.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_cond_key;

/**
  Instrumented thread key.
  To instrument a thread, a thread key must be obtained
  using @c register_thread.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_thread_key;

/**
  Instrumented file key.
  To instrument a file, a file key must be obtained using @c register_file.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_file_key;

/**
  Instrumented stage key.
  To instrument a stage, a stage key must be obtained using @c register_stage.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_stage_key;

/**
  Instrumented statement key.
  To instrument a statement, a statement key must be obtained using @c
  register_statement.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_statement_key;

/**
  Instrumented socket key.
  To instrument a socket, a socket key must be obtained using @c
  register_socket.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_socket_key;

#define PSI_INSTRUMENT_ME 0

#define PSI_NOT_INSTRUMENTED 0

/**
  Global flag.
  This flag indicate that an instrumentation point is a global variable,
  or a singleton.
*/
#define PSI_FLAG_GLOBAL (1 << 0)

/**
  Mutable flag.
  This flag indicate that an instrumentation point is a general placeholder,
  that can mutate into a more specific instrumentation point.
*/
#define PSI_FLAG_MUTABLE (1 << 1)

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
#define PSI_RWLOCK_FLAG_SX (1 << 4)

/**
  Transferable flag.
  This flag indicate that an instrumented object can
  be created by a thread and destroyed by another thread.
*/
#define PSI_FLAG_TRANSFER (1 << 5)

#define PSI_VOLATILITY_UNKNOWN 0
#define PSI_VOLATILITY_PERMANENT 1
#define PSI_VOLATILITY_PROVISIONING 2
#define PSI_VOLATILITY_DDL 3
#define PSI_VOLATILITY_ACCOUNT 4
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

#ifdef __cplusplus
}
#endif

#endif /* MYSQL_PSI_BASE_H */
