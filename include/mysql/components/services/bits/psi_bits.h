/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_BITS_PSI_BITS_H
#define COMPONENTS_SERVICES_BITS_PSI_BITS_H

/**
  @file mysql/components/services/bits/psi_bits.h
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

static constexpr unsigned PSI_INSTRUMENT_ME = 0;

#define PSI_DOCUMENT_ME ""

// Remove 'constexpr' in order to pick the right overload for
//    Prealloced_array CTOR.
// See https://bugs.llvm.org/show_bug.cgi?id=51582
// and TEST_F(PreallocedArrayTest, CorrectOverloadIsChosen)
#if defined(__clang__) && defined(_WIN32)
static unsigned PSI_NOT_INSTRUMENTED = 0;
#else
static constexpr unsigned PSI_NOT_INSTRUMENTED = 0;
#endif

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

/**
  Priority lock flag.
  Indicates that rwlock support the priority lock scheduling.
*/
#define PSI_FLAG_RWLOCK_PR (1 << 8)

/**
  System thread flag.
  Indicates that the instrumented object exists on a system thread.
*/
#define PSI_FLAG_THREAD_SYSTEM (1 << 9)

/**
  Automatic sequence number flag.
  Generate thread instances names automatically.
*/
#define PSI_FLAG_AUTO_SEQNUM (1 << 10)

/**
  No sequence number flag.
  Use thread instances names without sequence numbers.
*/
#define PSI_FLAG_NO_SEQNUM (1 << 11)

/**
  Enable collecting the memory consumed by threads.
*/
#define PSI_FLAG_MEM_COLLECT (1 << 12)

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

struct PSI_placeholder {
  int m_placeholder;
};

/**
  Instrumented artifact.
  The object instrumented can be enabled or disabled.
  The @c m_enabled member is visible and public,
  and must be tested by the instrumented code
  before making low level api calls to
  the actual instrumentation implementation.
*/
struct PSI_instr {
  /**
    Instrumentation is enabled.
    This flag must be checked before making calls to:
    - @c PSI_MUTEX_CALL(start_mutex_wait)
    - @c PSI_MUTEX_CALL(unlock_mutex)
    - @c PSI_RWLOCK_CALL(start_rwlock_rdwait)
    - @c PSI_RWLOCK_CALL(start_rwlock_wrwait)
    - @c PSI_RWLOCK_CALL(unlock_rwlock)
    - @c PSI_COND_CALL(start_cond_wait)
    - @c PSI_COND_CALL(signal_cond)
    - @c PSI_COND_CALL(broadcast_cond)
    - @c PSI_SOCKET_CALL(start_socket_wait)
  */
  bool m_enabled;
};

/**
    @} (end of group psi_abi)
  @} (end of group instrumentation_interface)
*/

#endif /* COMPONENTS_SERVICES_BITS_PSI_BITS_H */
