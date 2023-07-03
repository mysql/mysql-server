/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_EVENTS_TRANSACTIONS_H
#define PFS_EVENTS_TRANSACTIONS_H

/**
  @file storage/perfschema/pfs_events_transactions.h
  Events transactions data structures (declarations).
*/

#include <sys/types.h>
#include <atomic>

#include "my_inttypes.h"
#include "sql/rpl_gtid.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_events.h"
#include "storage/perfschema/pfs_global.h"

// Define XIDDATASIZE manually here to avoid pulling in all of mysql/plugin.h
// (which is big) just for MYSQL_XIDDATASIZE; this file is included from a lot
// of places. We have a static_assert in the .cc file to check that they are
// in sync. See sql/xa.h, which does the same thing.
#define XIDDATASIZE 128

struct PFS_thread;
struct PFS_account;
struct PFS_user;
struct PFS_host;

/**
  struct PSI_xid is binary compatible with the XID structure as
  in the X/Open CAE Specification, Distributed Transaction Processing:
  The XA Specification, X/Open Company Ltd., 1991.
  http://www.opengroup.org/bookstore/catalog/c193.htm

  A value of -1 in formatID means that the XID is null.
  Max length for bqual and gtrid is 64 bytes each.

  @see XID in sql/handler.h
  @see MYSQL_XID in mysql/plugin.h
*/
struct PSI_xid {
  /** Format identifier. */
  long formatID;
  /** GTRID length, value 1-64. */
  long gtrid_length;
  /** BQUAL length, value 1-64. */
  long bqual_length;
  /** XID raw data, not \0-terminated */
  char data[XIDDATASIZE];

  PSI_xid() { null(); }
  bool is_null() { return formatID == -1; }
  void null() {
    formatID = -1;
    gtrid_length = 0;
    bqual_length = 0;
  }
};
typedef struct PSI_xid PSI_xid;

/** A transaction record. */
struct PFS_events_transactions : public PFS_events {
  /** Source identifier, mapped from internal format. */
  rpl_sid m_sid;
  /** InnoDB transaction ID. */
  ulonglong m_trxid;
  /** Status */
  enum_transaction_state m_state;
  /** Global Transaction ID specifier. */
  Gtid_specification m_gtid_spec;
  /** True if XA transaction. */
  bool m_xa;
  /** XA transaction ID. */
  PSI_xid m_xid;
  /** XA status */
  enum_xa_transaction_state m_xa_state;
  /** Transaction isolation level. */
  enum_isolation_level m_isolation_level;
  /** True if read-only transaction, otherwise read-write. */
  bool m_read_only;
  /** True if autocommit transaction. */
  bool m_autocommit;
  /** Total number of savepoints. */
  ulonglong m_savepoint_count;
  /** Number of rollback_to_savepoint. */
  ulonglong m_rollback_to_savepoint_count;
  /** Number of release_savepoint. */
  ulonglong m_release_savepoint_count;
};

bool xid_printable(PSI_xid *xid, size_t offset, size_t length);

void insert_events_transactions_history(PFS_thread *thread,
                                        PFS_events_transactions *transaction);
void insert_events_transactions_history_long(
    PFS_events_transactions *transaction);

extern bool flag_events_transactions_current;
extern bool flag_events_transactions_history;
extern bool flag_events_transactions_history_long;

extern bool events_transactions_history_long_full;
extern PFS_cacheline_atomic_uint32 events_transactions_history_long_index;
extern PFS_events_transactions *events_transactions_history_long_array;
extern ulong events_transactions_history_long_size;

int init_events_transactions_history_long(
    uint events_transactions_history_long_sizing);
void cleanup_events_transactions_history_long();

void reset_events_transactions_current();
void reset_events_transactions_history();
void reset_events_transactions_history_long();
void reset_events_transactions_by_thread();
void reset_events_transactions_by_account();
void reset_events_transactions_by_user();
void reset_events_transactions_by_host();
void reset_events_transactions_global();
void aggregate_account_transactions(PFS_account *account);
void aggregate_user_transactions(PFS_user *user);
void aggregate_host_transactions(PFS_host *host);

#endif
