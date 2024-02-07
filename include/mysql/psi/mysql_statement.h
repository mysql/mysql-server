/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_STATEMENT_H
#define MYSQL_STATEMENT_H

/**
  @file include/mysql/psi/mysql_statement.h
  Instrumentation helpers for statements.
*/

#include "my_compiler.h"

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "my_inttypes.h"
#include "mysql/psi/psi_stage.h"
#include "mysql/psi/psi_statement.h"

#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
/* PSI_STATEMENT_CALL() as direct call. */
/* PSI_DIGEST_CALL() as direct call. */
#include "pfs_statement_provider.h"  // IWYU pragma: keep
/* PSI_STAGE_CALL() as direct call. */
#include "pfs_stage_provider.h"  // IWYU pragma: keep
#endif

class Diagnostics_area;
struct CHARSET_INFO;

#ifndef PSI_STATEMENT_CALL
#define PSI_STATEMENT_CALL(M) psi_statement_service->M
#endif

#ifndef PSI_DIGEST_CALL
#define PSI_DIGEST_CALL(M) psi_statement_service->M
#endif

#ifndef PSI_STAGE_CALL
#define PSI_STAGE_CALL(M) psi_stage_service->M
#endif

/**
  @defgroup psi_api_statement Statement Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  @def mysql_statement_register(P1, P2, P3)
  Statement registration.
*/
#define mysql_statement_register(P1, P2, P3) \
  inline_mysql_statement_register(P1, P2, P3)

#ifdef HAVE_PSI_STATEMENT_DIGEST_INTERFACE
#define MYSQL_DIGEST_START(LOCKER) inline_mysql_digest_start(LOCKER)
#else
#define MYSQL_DIGEST_START(LOCKER) NULL
#endif

#ifdef HAVE_PSI_STATEMENT_DIGEST_INTERFACE
#define MYSQL_DIGEST_END(LOCKER, DIGEST) inline_mysql_digest_end(LOCKER, DIGEST)
#else
#define MYSQL_DIGEST_END(LOCKER, DIGEST) \
  do {                                   \
  } while (0)
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_START_STATEMENT(STATE, K, DB, DB_LEN, CS, SPS)            \
  inline_mysql_start_statement(STATE, K, DB, DB_LEN, CS, SPS, __FILE__, \
                               __LINE__)
#else
#define MYSQL_START_STATEMENT(STATE, K, DB, DB_LEN, CS, SPS) NULL
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_REFINE_STATEMENT(LOCKER, K) \
  inline_mysql_refine_statement(LOCKER, K)
#else
#define MYSQL_REFINE_STATEMENT(LOCKER, K) NULL
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_SET_STATEMENT_TEXT(LOCKER, P1, P2) \
  inline_mysql_set_statement_text(LOCKER, P1, P2)
#else
#define MYSQL_SET_STATEMENT_TEXT(LOCKER, P1, P2) \
  do {                                           \
  } while (0)
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_SET_STATEMENT_QUERY_ID(LOCKER, P1) \
  inline_mysql_set_statement_query_id(LOCKER, P1)
#else
#define MYSQL_SET_STATEMENT_QUERY_ID(LOCKER, P1) \
  do {                                           \
  } while (0)
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_SET_STATEMENT_LOCK_TIME(LOCKER, P1) \
  inline_mysql_set_statement_lock_time(LOCKER, P1)
#else
#define MYSQL_SET_STATEMENT_LOCK_TIME(LOCKER, P1) \
  do {                                            \
  } while (0)
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_SET_STATEMENT_ROWS_SENT(LOCKER, P1) \
  inline_mysql_set_statement_rows_sent(LOCKER, P1)
#else
#define MYSQL_SET_STATEMENT_ROWS_SENT(LOCKER, P1) \
  do {                                            \
  } while (0)
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_SET_STATEMENT_ROWS_EXAMINED(LOCKER, P1) \
  inline_mysql_set_statement_rows_examined(LOCKER, P1)
#else
#define MYSQL_SET_STATEMENT_ROWS_EXAMINED(LOCKER, P1) \
  do {                                                \
  } while (0)
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(LOCKER, P1) \
  inline_mysql_notify_statement_query_attributes(LOCKER, P1)
#else
#define MYSQL_NOTIFY_STATEMENT_QUERY_ATTRIBUTES(LOCKER, P1) \
  do {                                                      \
  } while (0)
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
#define MYSQL_END_STATEMENT(LOCKER, DA) inline_mysql_end_statement(LOCKER, DA)
#else
#define MYSQL_END_STATEMENT(LOCKER, DA) \
  do {                                  \
  } while (0)
#endif

static inline void inline_mysql_statement_register(
#ifdef HAVE_PSI_STATEMENT_INTERFACE
    const char *category, PSI_statement_info *info, int count
#else
    const char *category [[maybe_unused]], void *info [[maybe_unused]],
    int count [[maybe_unused]]
#endif
) {
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(register_statement)(category, info, count);
#endif
}

#ifdef HAVE_PSI_STATEMENT_DIGEST_INTERFACE
static inline struct PSI_digest_locker *inline_mysql_digest_start(
    PSI_statement_locker *locker) {
  PSI_digest_locker *digest_locker = nullptr;

  if (likely(locker != nullptr)) {
    digest_locker = PSI_DIGEST_CALL(digest_start)(locker);
  }
  return digest_locker;
}
#endif

#ifdef HAVE_PSI_STATEMENT_DIGEST_INTERFACE
static inline void inline_mysql_digest_end(PSI_digest_locker *locker,
                                           const sql_digest_storage *digest) {
  if (likely(locker != nullptr)) {
    PSI_DIGEST_CALL(digest_end)(locker, digest);
  }
}
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
static inline struct PSI_statement_locker *inline_mysql_start_statement(
    PSI_statement_locker_state *state, PSI_statement_key key, const char *db,
    uint db_len, const CHARSET_INFO *charset, PSI_sp_share *sp_share,
    const char *src_file, int src_line) {
  PSI_statement_locker *locker;
  locker = PSI_STATEMENT_CALL(get_thread_statement_locker)(state, key, charset,
                                                           sp_share);
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(start_statement)(locker, db, db_len, src_file, src_line);
  }
  return locker;
}

static inline struct PSI_statement_locker *inline_mysql_refine_statement(
    PSI_statement_locker *locker, PSI_statement_key key) {
  if (likely(locker != nullptr)) {
    locker = PSI_STATEMENT_CALL(refine_statement)(locker, key);
  }
  return locker;
}

static inline void inline_mysql_set_statement_text(PSI_statement_locker *locker,
                                                   const char *text,
                                                   uint text_len) {
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(set_statement_text)(locker, text, text_len);
  }
}

static inline void inline_mysql_set_statement_query_id(
    PSI_statement_locker *locker, ulonglong id) {
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(set_statement_query_id)(locker, id);
  }
}

static inline void inline_mysql_set_statement_lock_time(
    PSI_statement_locker *locker, ulonglong count) {
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(set_statement_lock_time)(locker, count);
  }
}

static inline void inline_mysql_set_statement_rows_sent(
    PSI_statement_locker *locker, ulonglong count) {
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(set_statement_rows_sent)(locker, count);
  }
}

static inline void inline_mysql_set_statement_rows_examined(
    PSI_statement_locker *locker, ulonglong count) {
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(set_statement_rows_examined)(locker, count);
  }
}

static inline void inline_mysql_notify_statement_query_attributes(
    PSI_statement_locker *locker, bool with_query_attributes) {
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(notify_statement_query_attributes)
    (locker, with_query_attributes);
  }
}

static inline void inline_mysql_end_statement(
    struct PSI_statement_locker *locker, Diagnostics_area *stmt_da) {
#ifdef HAVE_PSI_STAGE_INTERFACE
  PSI_STAGE_CALL(end_stage)();
#endif /* HAVE_PSI_STAGE_INTERFACE */
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(end_statement)(locker, stmt_da);
  }
}
#endif /* HAVE_PSI_STATEMENT_INTERFACE */

static inline void mysql_statement_set_secondary_engine(
    PSI_statement_locker *locker [[maybe_unused]],
    bool secondary [[maybe_unused]]) {
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  if (likely(locker != nullptr)) {
    PSI_STATEMENT_CALL(set_statement_secondary_engine)(locker, secondary);
  }
#endif /* HAVE_PSI_STATEMENT_INTERFACE */
}

/** @} (end of group psi_api_statement) */

#endif
