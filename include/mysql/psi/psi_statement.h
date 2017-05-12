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

#ifndef MYSQL_PSI_STATEMENT_H
#define MYSQL_PSI_STATEMENT_H

/**
  @file include/mysql/psi/psi_statement.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_statement Statement Instrumentation (ABI)
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
  @def PSI_STATEMENT_VERSION_1
  Performance Schema Statement Interface number for version 1.
  This version is supported.
*/
#define PSI_STATEMENT_VERSION_1 1

/**
  @def PSI_STATEMENT_VERSION_2
  Performance Schema Statement Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_STATEMENT_VERSION_2 2

/**
  @def PSI_CURRENT_STATEMENT_VERSION
  Performance Schema Statement Interface number for the most recent version.
  The most current version is @c PSI_STATEMENT_VERSION_1
*/
#define PSI_CURRENT_STATEMENT_VERSION 1

#ifndef USE_PSI_STATEMENT_2
#ifndef USE_PSI_STATEMENT_1
#define USE_PSI_STATEMENT_1
#endif /* USE_PSI_STATEMENT_1 */
#endif /* USE_PSI_STATEMENT_2 */

#ifdef USE_PSI_STATEMENT_1
#define HAVE_PSI_STATEMENT_1
#endif /* USE_PSI_STATEMENT_1 */

#ifdef USE_PSI_STATEMENT_2
#define HAVE_PSI_STATEMENT_2
#endif /* USE_PSI_STATEMENT_2 */

/** Entry point for the performance schema interface. */
struct PSI_statement_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_STATEMENT_VERSION_1
    @sa PSI_STATEMENT_VERSION_2
    @sa PSI_CURRENT_STATEMENT_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_statement_bootstrap PSI_statement_bootstrap;

#ifdef HAVE_PSI_STATEMENT_1

/**
  Interface for an instrumented statement.
  This is an opaque structure.
*/
struct PSI_statement_locker;
typedef struct PSI_statement_locker PSI_statement_locker;

/**
  Interface for an instrumented prepared statement.
  This is an opaque structure.
*/
struct PSI_prepared_stmt;
typedef struct PSI_prepared_stmt PSI_prepared_stmt;

/**
  Interface for an instrumented statement digest operation.
  This is an opaque structure.
*/
struct PSI_digest_locker;
typedef struct PSI_digest_locker PSI_digest_locker;

/**
  Interface for an instrumented stored procedure share.
  This is an opaque structure.
*/
struct PSI_sp_share;
typedef struct PSI_sp_share PSI_sp_share;

/**
  Interface for an instrumented stored program.
  This is an opaque structure.
*/
struct PSI_sp_locker;
typedef struct PSI_sp_locker PSI_sp_locker;

/**
  Statement instrument information.
  @since PSI_STATEMENT_VERSION_1
  This structure is used to register an instrumented statement.
*/
struct PSI_statement_info_v1
{
  /** The registered statement key. */
  PSI_statement_key m_key;
  /** The name of the statement instrument to register. */
  const char *m_name;
  /** The flags of the statement instrument to register. */
  int m_flags;
};
typedef struct PSI_statement_info_v1 PSI_statement_info_v1;

/* Duplicate of NAME_LEN, to avoid dependency on mysql_com.h */
#define PSI_SCHEMA_NAME_LEN (64 * 3)

/**
  State data storage for @c get_thread_statement_locker_v1_t,
  @c get_thread_statement_locker_v1_t.
  This structure provide temporary storage to a statement locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa get_thread_statement_locker_v1_t
*/
struct PSI_statement_locker_state_v1
{
  /** Discarded flag. */
  bool m_discarded;
  /** In prepare flag. */
  bool m_in_prepare;
  /** Metric, no index used flag. */
  uchar m_no_index_used;
  /** Metric, no good index used flag. */
  uchar m_no_good_index_used;
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
  void *m_statement;
  /** Locked time. */
  ulonglong m_lock_time;
  /** Rows sent. */
  ulonglong m_rows_sent;
  /** Rows examined. */
  ulonglong m_rows_examined;
  /** Metric, temporary tables created on disk. */
  ulong m_created_tmp_disk_tables;
  /** Metric, temporary tables created. */
  ulong m_created_tmp_tables;
  /** Metric, number of select full join. */
  ulong m_select_full_join;
  /** Metric, number of select full range join. */
  ulong m_select_full_range_join;
  /** Metric, number of select range. */
  ulong m_select_range;
  /** Metric, number of select range check. */
  ulong m_select_range_check;
  /** Metric, number of select scan. */
  ulong m_select_scan;
  /** Metric, number of sort merge passes. */
  ulong m_sort_merge_passes;
  /** Metric, number of sort merge. */
  ulong m_sort_range;
  /** Metric, number of sort rows. */
  ulong m_sort_rows;
  /** Metric, number of sort scans. */
  ulong m_sort_scan;
  /** Statement digest. */
  const struct sql_digest_storage *m_digest;
  /** Current schema name. */
  char m_schema_name[PSI_SCHEMA_NAME_LEN];
  /** Length in bytes of @c m_schema_name. */
  uint m_schema_name_length;
  /** Statement character set number. */
  uint m_cs_number;
  PSI_sp_share *m_parent_sp_share;
  PSI_prepared_stmt *m_parent_prepared_stmt;
};
typedef struct PSI_statement_locker_state_v1 PSI_statement_locker_state_v1;

struct PSI_sp_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Stored Procedure share. */
  PSI_sp_share *m_sp_share;
};
typedef struct PSI_sp_locker_state_v1 PSI_sp_locker_state_v1;

/**
  Statement registration API.
  @param category a category name
  @param info an array of statement info to register
  @param count the size of the info array
*/
typedef void (*register_statement_v1_t)(const char *category,
                                        struct PSI_statement_info_v1 *info,
                                        int count);

/**
  Get a statement instrumentation locker.
  @param state data storage for the locker
  @param key the statement instrumentation key
  @param charset client character set
  @return a statement locker, or NULL
*/
typedef struct PSI_statement_locker *(*get_thread_statement_locker_v1_t)(
  struct PSI_statement_locker_state_v1 *state,
  PSI_statement_key key,
  const void *charset,
  PSI_sp_share *sp_share);

/**
  Refine a statement locker to a more specific key.
  Note that only events declared mutable can be refined.
  @param locker the statement locker for the current event
  @param key the new key for the event
  @sa PSI_FLAG_MUTABLE
*/
typedef struct PSI_statement_locker *(*refine_statement_v1_t)(
  struct PSI_statement_locker *locker, PSI_statement_key key);

/**
  Start a new statement event.
  @param locker the statement locker for this event
  @param db the active database name for this statement
  @param db_length the active database name length for this statement
  @param src_file source file name
  @param src_line source line number
*/
typedef void (*start_statement_v1_t)(struct PSI_statement_locker *locker,
                                     const char *db,
                                     uint db_length,
                                     const char *src_file,
                                     uint src_line);

/**
  Set the statement text for a statement event.
  @param locker the current statement locker
  @param text the statement text
  @param text_len the statement text length
*/
typedef void (*set_statement_text_v1_t)(struct PSI_statement_locker *locker,
                                        const char *text,
                                        uint text_len);

/**
  Set a statement event lock time.
  @param locker the statement locker
  @param lock_time the locked time, in microseconds
*/
typedef void (*set_statement_lock_time_t)(struct PSI_statement_locker *locker,
                                          ulonglong lock_time);

/**
  Set a statement event rows sent metric.
  @param locker the statement locker
  @param count the number of rows sent
*/
typedef void (*set_statement_rows_sent_t)(struct PSI_statement_locker *locker,
                                          ulonglong count);

/**
  Set a statement event rows examined metric.
  @param locker the statement locker
  @param count the number of rows examined
*/
typedef void (*set_statement_rows_examined_t)(
  struct PSI_statement_locker *locker, ulonglong count);

/**
  Increment a statement event "created tmp disk tables" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_created_tmp_disk_tables_t)(
  struct PSI_statement_locker *locker, ulong count);

/**
  Increment a statement event "created tmp tables" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_created_tmp_tables_t)(
  struct PSI_statement_locker *locker, ulong count);

/**
  Increment a statement event "select full join" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_select_full_join_t)(
  struct PSI_statement_locker *locker, ulong count);

/**
  Increment a statement event "select full range join" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_select_full_range_join_t)(
  struct PSI_statement_locker *locker, ulong count);

/**
  Increment a statement event "select range join" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_select_range_t)(
  struct PSI_statement_locker *locker, ulong count);

/**
  Increment a statement event "select range check" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_select_range_check_t)(
  struct PSI_statement_locker *locker, ulong count);

/**
  Increment a statement event "select scan" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_select_scan_t)(struct PSI_statement_locker *locker,
                                            ulong count);

/**
  Increment a statement event "sort merge passes" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_sort_merge_passes_t)(
  struct PSI_statement_locker *locker, ulong count);

/**
  Increment a statement event "sort range" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_sort_range_t)(struct PSI_statement_locker *locker,
                                           ulong count);

/**
  Increment a statement event "sort rows" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_sort_rows_t)(struct PSI_statement_locker *locker,
                                          ulong count);

/**
  Increment a statement event "sort scan" metric.
  @param locker the statement locker
  @param count the metric increment value
*/
typedef void (*inc_statement_sort_scan_t)(struct PSI_statement_locker *locker,
                                          ulong count);

/**
  Set a statement event "no index used" metric.
  @param locker the statement locker
*/
typedef void (*set_statement_no_index_used_t)(
  struct PSI_statement_locker *locker);

/**
  Set a statement event "no good index used" metric.
  @param locker the statement locker
*/
typedef void (*set_statement_no_good_index_used_t)(
  struct PSI_statement_locker *locker);

/**
  End a statement event.
  @param locker the statement locker
  @param stmt_da the statement diagnostics area.
  @sa Diagnostics_area
*/
typedef void (*end_statement_v1_t)(struct PSI_statement_locker *locker,
                                   void *stmt_da);

/**
  Get a prepare statement.
  @param locker a statement locker for the running thread.
*/
typedef PSI_prepared_stmt *(*create_prepared_stmt_v1_t)(
  void *identity,
  uint stmt_id,
  PSI_statement_locker *locker,
  const char *stmt_name,
  size_t stmt_name_length,
  const char *name,
  size_t length);

/**
  destroy a prepare statement.
  @param prepared_stmt prepared statement.
*/
typedef void (*destroy_prepared_stmt_v1_t)(PSI_prepared_stmt *prepared_stmt);

/**
  repreare a prepare statement.
  @param prepared_stmt prepared statement.
*/
typedef void (*reprepare_prepared_stmt_v1_t)(PSI_prepared_stmt *prepared_stmt);

/**
  Record a prepare statement instrumentation execute event.
  @param locker a statement locker for the running thread.
  @param prepared_stmt prepared statement.
*/
typedef void (*execute_prepared_stmt_v1_t)(PSI_statement_locker *locker,
                                           PSI_prepared_stmt *prepared_stmt);

/**
  Get a digest locker for the current statement.
  @param locker a statement locker for the running thread
*/
typedef struct PSI_digest_locker *(*digest_start_v1_t)(
  struct PSI_statement_locker *locker);

/**
  Add a computed digest to the current digest instrumentation.
  @param locker a digest locker for the current statement
  @param digest the computed digest
*/
typedef void (*digest_end_v1_t)(struct PSI_digest_locker *locker,
                                const struct sql_digest_storage *digest);

/**
  Acquire a sp share instrumentation.
  @param object_type type of stored program
  @param schema_name schema name of stored program
  @param schema_name_length length of schema_name
  @param object_name object name of stored program
  @param object_name_length length of object_name
  @return a stored program share instrumentation, or NULL
*/
typedef struct PSI_sp_share *(*get_sp_share_v1_t)(uint object_type,
                                                  const char *schema_name,
                                                  uint schema_name_length,
                                                  const char *object_name,
                                                  uint object_name_length);

/**
  Release a stored program share.
  @param share the stored program share to release
*/
typedef void (*release_sp_share_v1_t)(struct PSI_sp_share *share);

typedef PSI_sp_locker *(*start_sp_v1_t)(struct PSI_sp_locker_state_v1 *state,
                                        struct PSI_sp_share *sp_share);

typedef void (*end_sp_v1_t)(struct PSI_sp_locker *locker);

typedef void (*drop_sp_v1_t)(uint object_type,
                             const char *schema_name,
                             uint schema_name_length,
                             const char *object_name,
                             uint object_name_length);

/**
  Performance Schema Statement Interface, version 1.
  @since PSI_STATEMENT_VERSION_1
*/
struct PSI_statement_service_v1
{
  /** @sa register_statement_v1_t. */
  register_statement_v1_t register_statement;
  /** @sa get_thread_statement_locker_v1_t. */
  get_thread_statement_locker_v1_t get_thread_statement_locker;
  /** @sa refine_statement_v1_t. */
  refine_statement_v1_t refine_statement;
  /** @sa start_statement_v1_t. */
  start_statement_v1_t start_statement;
  /** @sa set_statement_text_v1_t. */
  set_statement_text_v1_t set_statement_text;
  /** @sa set_statement_lock_time_t. */
  set_statement_lock_time_t set_statement_lock_time;
  /** @sa set_statement_rows_sent_t. */
  set_statement_rows_sent_t set_statement_rows_sent;
  /** @sa set_statement_rows_examined_t. */
  set_statement_rows_examined_t set_statement_rows_examined;
  /** @sa inc_statement_created_tmp_disk_tables. */
  inc_statement_created_tmp_disk_tables_t inc_statement_created_tmp_disk_tables;
  /** @sa inc_statement_created_tmp_tables. */
  inc_statement_created_tmp_tables_t inc_statement_created_tmp_tables;
  /** @sa inc_statement_select_full_join. */
  inc_statement_select_full_join_t inc_statement_select_full_join;
  /** @sa inc_statement_select_full_range_join. */
  inc_statement_select_full_range_join_t inc_statement_select_full_range_join;
  /** @sa inc_statement_select_range. */
  inc_statement_select_range_t inc_statement_select_range;
  /** @sa inc_statement_select_range_check. */
  inc_statement_select_range_check_t inc_statement_select_range_check;
  /** @sa inc_statement_select_scan. */
  inc_statement_select_scan_t inc_statement_select_scan;
  /** @sa inc_statement_sort_merge_passes. */
  inc_statement_sort_merge_passes_t inc_statement_sort_merge_passes;
  /** @sa inc_statement_sort_range. */
  inc_statement_sort_range_t inc_statement_sort_range;
  /** @sa inc_statement_sort_rows. */
  inc_statement_sort_rows_t inc_statement_sort_rows;
  /** @sa inc_statement_sort_scan. */
  inc_statement_sort_scan_t inc_statement_sort_scan;
  /** @sa set_statement_no_index_used. */
  set_statement_no_index_used_t set_statement_no_index_used;
  /** @sa set_statement_no_good_index_used. */
  set_statement_no_good_index_used_t set_statement_no_good_index_used;
  /** @sa end_statement_v1_t. */
  end_statement_v1_t end_statement;

  /** @sa create_prepared_stmt_v1_t. */
  create_prepared_stmt_v1_t create_prepared_stmt;
  /** @sa destroy_prepared_stmt_v1_t. */
  destroy_prepared_stmt_v1_t destroy_prepared_stmt;
  /** @sa reprepare_prepared_stmt_v1_t. */
  reprepare_prepared_stmt_v1_t reprepare_prepared_stmt;
  /** @sa execute_prepared_stmt_v1_t. */
  execute_prepared_stmt_v1_t execute_prepared_stmt;

  /** @sa digest_start_v1_t. */
  digest_start_v1_t digest_start;
  /** @sa digest_end_v1_t. */
  digest_end_v1_t digest_end;

  /** @sa get_sp_share_v1_t. */
  get_sp_share_v1_t get_sp_share;
  /** @sa release_sp_share_v1_t. */
  release_sp_share_v1_t release_sp_share;
  /** @sa start_sp_v1_t. */
  start_sp_v1_t start_sp;
  /** @sa start_sp_v1_t. */
  end_sp_v1_t end_sp;
  /** @sa drop_sp_v1_t. */
  drop_sp_v1_t drop_sp;
};

#endif /* HAVE_PSI_STATEMENT_1 */

/* Export the required version */
#ifdef USE_PSI_STATEMENT_1
typedef struct PSI_statement_service_v1 PSI_statement_service_t;
typedef struct PSI_statement_info_v1 PSI_statement_info;
typedef struct PSI_statement_locker_state_v1 PSI_statement_locker_state;
typedef struct PSI_sp_locker_state_v1 PSI_sp_locker_state;
#else
typedef struct PSI_placeholder PSI_statement_service_t;
typedef struct PSI_placeholder PSI_statement_info;
typedef struct PSI_placeholder PSI_statement_locker_state;
typedef struct PSI_placeholder PSI_sp_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_statement_service_t *psi_statement_service;

/** @} (end of group psi_abi_statement) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_STATEMENT_H */
