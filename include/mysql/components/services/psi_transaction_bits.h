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

#ifndef COMPONENTS_SERVICES_PSI_TRANSACTION_BITS_H
#define COMPONENTS_SERVICES_PSI_TRANSACTION_BITS_H

#include "my_inttypes.h"
#include "my_macros.h"

C_MODE_START

/**
  @file
  Performance schema instrumentation interface.

  @defgroup psi_abi_transaction Transaction Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

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

typedef struct PSI_transaction_locker_state_v1 PSI_transaction_locker_state;

/** @} (end of group psi_abi_transaction) */

C_MODE_END

#endif /* COMPONENTS_SERVICES_PSI_TRANSACTION_BITS_H */
