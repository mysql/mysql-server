/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_TRANSACTION_H
#define MYSQL_TRANSACTION_H

/**
  @file mysql/psi/mysql_transaction.h
  Instrumentation helpers for transactions.
*/

#include "mysql/psi/psi.h"

#ifndef PSI_TRANSACTION_CALL
#define PSI_TRANSACTION_CALL(M) PSI_DYNAMIC_CALL(M)
#endif

/**
  @defgroup Transaction_instrumentation Transaction Instrumentation
  @ingroup Instrumentation_interface
  @{
*/

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_START_TRANSACTION(STATE, XID, TRXID, ISO, RO, AC) \
    inline_mysql_start_transaction(STATE, XID, TRXID, ISO, RO, AC, __FILE__, __LINE__)
#else
  #define MYSQL_START_TRANSACTION(STATE, XID, TRXID, ISO, RO, AC) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_SET_TRANSACTION_GTID(LOCKER, P1, P2) \
    inline_mysql_set_transaction_gtid(LOCKER, P1, P2)
#else
  #define MYSQL_SET_TRANSACTION_GTID(LOCKER, P1, P2) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_SET_TRANSACTION_XID(LOCKER, P1, P2) \
    inline_mysql_set_transaction_xid(LOCKER, P1, P2)
#else
  #define MYSQL_SET_TRANSACTION_XID(LOCKER, P1, P2) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_SET_TRANSACTION_XA_STATE(LOCKER, P1) \
    inline_mysql_set_transaction_xa_state(LOCKER, P1)
#else
  #define MYSQL_SET_TRANSACTION_XA_STATE(LOCKER, P1) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_SET_TRANSACTION_TRXID(LOCKER, P1) \
    inline_mysql_set_transaction_trxid(LOCKER, P1)
#else
  #define MYSQL_SET_TRANSACTION_TRXID(LOCKER, P1) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_INC_TRANSACTION_SAVEPOINTS(LOCKER, P1) \
    inline_mysql_inc_transaction_savepoints(LOCKER, P1)
#else
  #define MYSQL_INC_TRANSACTION_SAVEPOINTS(LOCKER, P1) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_INC_TRANSACTION_ROLLBACK_TO_SAVEPOINT(LOCKER, P1) \
    inline_mysql_inc_transaction_rollback_to_savepoint(LOCKER, P1)
#else
  #define MYSQL_INC_TRANSACTION_ROLLBACK_TO_SAVEPOINT(LOCKER, P1) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_INC_TRANSACTION_RELEASE_SAVEPOINT(LOCKER, P1) \
    inline_mysql_inc_transaction_release_savepoint(LOCKER, P1)
#else
  #define MYSQL_INC_TRANSACTION_RELEASE_SAVEPOINT(LOCKER, P1) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_ROLLBACK_TRANSACTION(LOCKER) \
    inline_mysql_rollback_transaction(LOCKER)
#else
  #define MYSQL_ROLLBACK_TRANSACTION(LOCKER) \
    NULL
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  #define MYSQL_COMMIT_TRANSACTION(LOCKER) \
    inline_mysql_commit_transaction(LOCKER)
#else
  #define MYSQL_COMMIT_TRANSACTION(LOCKER) \
    NULL
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
static inline struct PSI_transaction_locker *
inline_mysql_start_transaction(PSI_transaction_locker_state *state,
                               const void *xid,
                               const ulonglong *trxid,
                               int isolation_level,
                               my_bool read_only,
                               my_bool autocommit,
                               const char *src_file, int src_line)
{
  PSI_transaction_locker *locker;
  locker= PSI_TRANSACTION_CALL(get_thread_transaction_locker)(state,
                                                              xid, trxid,
                                                              isolation_level,
                                                              read_only,
                                                              autocommit);
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(start_transaction)(locker, src_file, src_line);
  return locker;
}

static inline void
inline_mysql_set_transaction_gtid(PSI_transaction_locker *locker,
                                  const void *sid,
                                  const void *gtid_spec)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(set_transaction_gtid)(locker, sid, gtid_spec);
}

static inline void
inline_mysql_set_transaction_xid(PSI_transaction_locker *locker,
                                 const void *xid,
                                 int xa_state)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(set_transaction_xid)(locker, xid, xa_state);
}

static inline void
inline_mysql_set_transaction_xa_state(PSI_transaction_locker *locker,
                                      int xa_state)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(set_transaction_xa_state)(locker, xa_state);
}

static inline void
inline_mysql_set_transaction_trxid(PSI_transaction_locker *locker,
                                   const ulonglong *trxid)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(set_transaction_trxid)(locker, trxid);
}

static inline void
inline_mysql_inc_transaction_savepoints(PSI_transaction_locker *locker,
                                        ulong count)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(inc_transaction_savepoints)(locker, count);
}

static inline void
inline_mysql_inc_transaction_rollback_to_savepoint(PSI_transaction_locker *locker,
                                                   ulong count)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(inc_transaction_rollback_to_savepoint)(locker, count);
}

static inline void
inline_mysql_inc_transaction_release_savepoint(PSI_transaction_locker *locker,
                                               ulong count)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(inc_transaction_release_savepoint)(locker, count);
}

static inline void
inline_mysql_rollback_transaction(struct PSI_transaction_locker *locker)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(end_transaction)(locker, false);
}

static inline void
inline_mysql_commit_transaction(struct PSI_transaction_locker *locker)
{
  if (likely(locker != NULL))
    PSI_TRANSACTION_CALL(end_transaction)(locker, true);
}
#endif

/** @} (end of group Transaction_instrumentation) */

#endif

