/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_PS_H
#define MYSQL_PS_H

/**
  @file include/mysql/psi/mysql_ps.h
  Instrumentation helpers for prepared statements.
*/

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "mysql/psi/psi_statement.h"

#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
/* PSI_PS_CALL() as direct call. */
#include "pfs_statement_provider.h"  // IWYU pragma: keep
#endif

#ifndef PSI_PS_CALL
#define PSI_PS_CALL(M) psi_statement_service->M
#endif

#ifdef HAVE_PSI_PS_INTERFACE

#define MYSQL_CREATE_PS(IDENTITY, ID, LOCKER, NAME, NAME_LENGTH, SQLTEXT,    \
                        SQLTEXT_LENGTH)                                      \
  inline_mysql_create_prepared_stmt(IDENTITY, ID, LOCKER, NAME, NAME_LENGTH, \
                                    SQLTEXT, SQLTEXT_LENGTH)
#define MYSQL_EXECUTE_PS(LOCKER, PREPARED_STMT) \
  inline_mysql_execute_prepared_stmt(LOCKER, PREPARED_STMT)
#define MYSQL_DESTROY_PS(PREPARED_STMT) \
  inline_mysql_destroy_prepared_stmt(PREPARED_STMT)
#define MYSQL_REPREPARE_PS(PREPARED_STMT) \
  inline_mysql_reprepare_prepared_stmt(PREPARED_STMT)
#define MYSQL_SET_PS_TEXT(PREPARED_STMT, SQLTEXT, SQLTEXT_LENGTH) \
  inline_mysql_set_prepared_stmt_text(PREPARED_STMT, SQLTEXT, SQLTEXT_LENGTH)
#define MYSQL_SET_PS_SECONDARY_ENGINE(PREPARED_STMT, SECONDARY) \
  inline_mysql_set_prepared_stmt_secondary_engine(PREPARED_STMT, SECONDARY)

#else

#define MYSQL_CREATE_PS(IDENTITY, ID, LOCKER, NAME, NAME_LENGTH, SQLTEXT, \
                        SQLTEXT_LENGTH)                                   \
  NULL
#define MYSQL_EXECUTE_PS(LOCKER, PREPARED_STMT) \
  do {                                          \
  } while (0)
#define MYSQL_DESTROY_PS(PREPARED_STMT) \
  do {                                  \
  } while (0)
#define MYSQL_REPREPARE_PS(PREPARED_STMT) \
  do {                                    \
  } while (0)
#define MYSQL_SET_PS_TEXT(PREPARED_STMT, SQLTEXT, SQLTEXT_LENGTH) \
  do {                                                            \
  } while (0)
#define MYSQL_SET_PS_SECONDARY_ENGINE(PREPARED_STMT, SECONDARY) \
  do {                                                          \
  } while (0)

#endif

#ifdef HAVE_PSI_PS_INTERFACE
static inline struct PSI_prepared_stmt *inline_mysql_create_prepared_stmt(
    void *identity, uint stmt_id, PSI_statement_locker *locker,
    const char *stmt_name, size_t stmt_name_length, const char *sqltext,
    size_t sqltext_length) {
  if (locker == nullptr) {
    return nullptr;
  }
  return PSI_PS_CALL(create_prepared_stmt)(identity, stmt_id, locker, stmt_name,
                                           stmt_name_length, sqltext,
                                           sqltext_length);
}

static inline void inline_mysql_execute_prepared_stmt(
    PSI_statement_locker *locker, PSI_prepared_stmt *prepared_stmt) {
  if (prepared_stmt != nullptr && locker != nullptr) {
    PSI_PS_CALL(execute_prepared_stmt)(locker, prepared_stmt);
  }
}

static inline void inline_mysql_destroy_prepared_stmt(
    PSI_prepared_stmt *prepared_stmt) {
  if (prepared_stmt != nullptr) {
    PSI_PS_CALL(destroy_prepared_stmt)(prepared_stmt);
  }
}

static inline void inline_mysql_reprepare_prepared_stmt(
    PSI_prepared_stmt *prepared_stmt) {
  if (prepared_stmt != nullptr) {
    PSI_PS_CALL(reprepare_prepared_stmt)(prepared_stmt);
  }
}

static inline void inline_mysql_set_prepared_stmt_text(
    PSI_prepared_stmt *prepared_stmt, const char *text, uint text_len) {
  if (prepared_stmt != nullptr) {
    PSI_PS_CALL(set_prepared_stmt_text)(prepared_stmt, text, text_len);
  }
}

static inline void inline_mysql_set_prepared_stmt_secondary_engine(
    PSI_prepared_stmt *prepared_stmt, bool secondary) {
  if (prepared_stmt != nullptr) {
    PSI_PS_CALL(set_prepared_stmt_secondary_engine)(prepared_stmt, secondary);
  }
}

#endif

#endif
