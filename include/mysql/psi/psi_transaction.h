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

#ifndef MYSQL_PSI_TRANSACTION_H
#define MYSQL_PSI_TRANSACTION_H

/**
  @file include/mysql/psi/psi_transaction.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_transaction Transaction Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "psi_base.h"

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_TRANSACTION_VERSION_1
  Performance Schema Transaction Interface number for version 1.
  This version is supported.
*/
#define PSI_TRANSACTION_VERSION_1 1

/**
  @def PSI_TRANSACTION_VERSION_2
  Performance Schema Transaction Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_TRANSACTION_VERSION_2 2

/**
  @def PSI_CURRENT_TRANSACTION_VERSION
  Performance Schema Transaction Interface number for the most recent version.
  The most current version is @c PSI_TRANSACTION_VERSION_1
*/
#define PSI_CURRENT_TRANSACTION_VERSION 1

#ifndef USE_PSI_TRANSACTION_2
#ifndef USE_PSI_TRANSACTION_1
#define USE_PSI_TRANSACTION_1
#endif /* USE_PSI_TRANSACTION_1 */
#endif /* USE_PSI_TRANSACTION_2 */

#ifdef USE_PSI_TRANSACTION_1
#define HAVE_PSI_TRANSACTION_1
#endif /* USE_PSI_TRANSACTION_1 */

#ifdef USE_PSI_TRANSACTION_2
#define HAVE_PSI_TRANSACTION_2
#endif /* USE_PSI_TRANSACTION_2 */

/** Entry point for the performance schema interface. */
struct PSI_transaction_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_TRANSACTION_VERSION_1
    @sa PSI_TRANSACTION_VERSION_2
    @sa PSI_CURRENT_TRANSACTION_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_transaction_bootstrap PSI_transaction_bootstrap;

#ifdef HAVE_PSI_TRANSACTION_1

/**
  Interface for an instrumented transaction.
  This is an opaque structure.
*/
struct PSI_transaction_locker;
typedef struct PSI_transaction_locker PSI_transaction_locker;

/**
  State data storage for @c get_thread_transaction_locker_v1_t,
  @c get_thread_transaction_locker_v1_t.
  This structure provide temporary storage to a transaction locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa get_thread_transaction_locker_v1_t
*/
struct PSI_transaction_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Instrumentation class. */
  void *m_class;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Internal data. */
  void *m_transaction;
  /** True if read-only transaction, false if read-write. */
  bool m_read_only;
  /** True if transaction is autocommit. */
  bool m_autocommit;
  /** Number of statements. */
  ulong m_statement_count;
  /** Total number of savepoints. */
  ulong m_savepoint_count;
  /** Number of rollback_to_savepoint. */
  ulong m_rollback_to_savepoint_count;
  /** Number of release_savepoint. */
  ulong m_release_savepoint_count;
};
typedef struct PSI_transaction_locker_state_v1 PSI_transaction_locker_state_v1;

/**
  Get a transaction instrumentation locker.
  @param state data storage for the locker
  @param xid the xid for this transaction
  @param trxid the InnoDB transaction id
  @param isolation_level isolation level for this transaction
  @param read_only true if transaction access mode is read-only
  @param autocommit true if transaction is autocommit
  @return a transaction locker, or NULL
*/
typedef struct PSI_transaction_locker *(*get_thread_transaction_locker_v1_t)(
  struct PSI_transaction_locker_state_v1 *state,
  const void *xid,
  const ulonglong *trxid,
  int isolation_level,
  bool read_only,
  bool autocommit);

/**
  Start a new transaction event.
  @param locker the transaction locker for this event
  @param src_file source file name
  @param src_line source line number
*/
typedef void (*start_transaction_v1_t)(struct PSI_transaction_locker *locker,
                                       const char *src_file,
                                       uint src_line);

/**
  Set the transaction xid.
  @param locker the transaction locker for this event
  @param xid the id of the XA transaction
  @param xa_state the state of the XA transaction
*/
typedef void (*set_transaction_xid_v1_t)(struct PSI_transaction_locker *locker,
                                         const void *xid,
                                         int xa_state);

/**
  Set the state of the XA transaction.
  @param locker the transaction locker for this event
  @param xa_state the new state of the xa transaction
*/
typedef void (*set_transaction_xa_state_v1_t)(
  struct PSI_transaction_locker *locker, int xa_state);

/**
  Set the transaction gtid.
  @param locker the transaction locker for this event
  @param sid the source id for the transaction, mapped from sidno
  @param gtid_spec the gtid specifier for the transaction
*/
typedef void (*set_transaction_gtid_v1_t)(struct PSI_transaction_locker *locker,
                                          const void *sid,
                                          const void *gtid_spec);

/**
  Set the transaction trx_id.
  @param locker the transaction locker for this event
  @param trxid the storage engine transaction ID
*/
typedef void (*set_transaction_trxid_v1_t)(
  struct PSI_transaction_locker *locker, const ulonglong *trxid);

/**
  Increment a transaction event savepoint count.
  @param locker the transaction locker
  @param count the increment value
*/
typedef void (*inc_transaction_savepoints_v1_t)(
  struct PSI_transaction_locker *locker, ulong count);

/**
  Increment a transaction event rollback to savepoint count.
  @param locker the transaction locker
  @param count the increment value
*/
typedef void (*inc_transaction_rollback_to_savepoint_v1_t)(
  struct PSI_transaction_locker *locker, ulong count);

/**
  Increment a transaction event release savepoint count.
  @param locker the transaction locker
  @param count the increment value
*/
typedef void (*inc_transaction_release_savepoint_v1_t)(
  struct PSI_transaction_locker *locker, ulong count);

/**
  Commit or rollback the transaction.
  @param locker the transaction locker for this event
  @param commit true if transaction was committed, false if rolled back
*/
typedef void (*end_transaction_v1_t)(struct PSI_transaction_locker *locker,
                                     bool commit);

/**
  Performance Schema Transaction Interface, version 1.
  @since PSI_TRANSACTION_VERSION_1
*/
struct PSI_transaction_service_v1
{
  /** @sa get_thread_transaction_locker_v1_t. */
  get_thread_transaction_locker_v1_t get_thread_transaction_locker;
  /** @sa start_transaction_v1_t. */
  start_transaction_v1_t start_transaction;
  /** @sa set_transaction_xid_v1_t. */
  set_transaction_xid_v1_t set_transaction_xid;
  /** @sa set_transaction_xa_state_v1_t. */
  set_transaction_xa_state_v1_t set_transaction_xa_state;
  /** @sa set_transaction_gtid_v1_t. */
  set_transaction_gtid_v1_t set_transaction_gtid;
  /** @sa set_transaction_trxid_v1_t. */
  set_transaction_trxid_v1_t set_transaction_trxid;
  /** @sa inc_transaction_savepoints_v1_t. */
  inc_transaction_savepoints_v1_t inc_transaction_savepoints;
  /** @sa inc_transaction_rollback_to_savepoint_v1_t. */
  inc_transaction_rollback_to_savepoint_v1_t
    inc_transaction_rollback_to_savepoint;
  /** @sa inc_transaction_release_savepoint_v1_t. */
  inc_transaction_release_savepoint_v1_t inc_transaction_release_savepoint;
  /** @sa end_transaction_v1_t. */
  end_transaction_v1_t end_transaction;
};

#endif /* HAVE_PSI_TRANSACTION_1 */

/* Export the required version */
#ifdef USE_PSI_TRANSACTION_1
typedef struct PSI_transaction_service_v1 PSI_transaction_service_t;
typedef struct PSI_transaction_locker_state_v1 PSI_transaction_locker_state;
#else
typedef struct PSI_placeholder PSI_transaction_service_t;
typedef struct PSI_placeholder PSI_transaction_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_transaction_service_t *psi_transaction_service;

/** @} (end of group psi_abi_transaction) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_TRANSACTION_H */
