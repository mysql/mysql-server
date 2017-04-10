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

#ifndef MYSQL_PSI_TABLE_H
#define MYSQL_PSI_TABLE_H

/**
  @file include/mysql/psi/psi_table.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_table Table Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "psi_base.h"

C_MODE_START

struct TABLE_SHARE;

/**
  Interface for an instrumented table operation.
  This is an opaque structure.
*/
struct PSI_table_locker;
typedef struct PSI_table_locker PSI_table_locker;

/**
  @todo Do not expose unconditionally PSI_table_io_operation.
*/

/** IO operation performed on an instrumented table. */
enum PSI_table_io_operation
{
  /** Row fetch. */
  PSI_TABLE_FETCH_ROW = 0,
  /** Row write. */
  PSI_TABLE_WRITE_ROW = 1,
  /** Row update. */
  PSI_TABLE_UPDATE_ROW = 2,
  /** Row delete. */
  PSI_TABLE_DELETE_ROW = 3
};
typedef enum PSI_table_io_operation PSI_table_io_operation;

/**
  @todo Do not expose unconditionally PSI_table_locker_state.
*/

/**
  State data storage for @c start_table_io_wait_v1_t,
  @c start_table_lock_wait_v1_t.
  This structure provide temporary storage to a table locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa start_table_io_wait_v1_t
  @sa start_table_lock_wait_v1_t
*/
struct PSI_table_locker_state
{
  /** Internal state. */
  uint m_flags;
  /** Current io operation. */
  enum PSI_table_io_operation m_io_operation;
  /** Current table handle. */
  struct PSI_table *m_table;
  /** Current table share. */
  struct PSI_table_share *m_table_share;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Internal data. */
  void *m_wait;
  /**
    Implementation specific.
    For table io, the table io index.
    For table lock, the lock type.
  */
  uint m_index;
};
typedef struct PSI_table_locker_state PSI_table_locker_state;

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_TABLE_VERSION_1
  Performance Schema Table Interface number for version 1.
  This version is supported.
*/
#define PSI_TABLE_VERSION_1 1

/**
  @def PSI_TABLE_VERSION_2
  Performance Schema Table Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_TABLE_VERSION_2 2

/**
  @def PSI_CURRENT_TABLE_VERSION
  Performance Schema Table Interface number for the most recent version.
  The most current version is @c PSI_TABLE_VERSION_1
*/
#define PSI_CURRENT_TABLE_VERSION 1

#ifndef USE_PSI_TABLE_2
#ifndef USE_PSI_TABLE_1
#define USE_PSI_TABLE_1
#endif /* USE_PSI_TABLE_1 */
#endif /* USE_PSI_TABLE_2 */

#ifdef USE_PSI_TABLE_1
#define HAVE_PSI_TABLE_1
#endif /* USE_PSI_TABLE_1 */

#ifdef USE_PSI_TABLE_2
#define HAVE_PSI_TABLE_2
#endif /* USE_PSI_TABLE_2 */

/** Entry point for the performance schema interface. */
struct PSI_table_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_TABLE_VERSION_1
    @sa PSI_TABLE_VERSION_2
    @sa PSI_CURRENT_TABLE_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_table_bootstrap PSI_table_bootstrap;

#ifdef HAVE_PSI_TABLE_1

/**
  Interface for an instrumented table share.
  This is an opaque structure.
*/
struct PSI_table_share;
typedef struct PSI_table_share PSI_table_share;

/**
  Interface for an instrumented table handle.
  This is an opaque structure.
*/
struct PSI_table;
typedef struct PSI_table PSI_table;

/** Lock operation performed on an instrumented table. */
enum PSI_table_lock_operation
{
  /** Table lock, in the server layer. */
  PSI_TABLE_LOCK = 0,
  /** Table lock, in the storage engine layer. */
  PSI_TABLE_EXTERNAL_LOCK = 1
};
typedef enum PSI_table_lock_operation PSI_table_lock_operation;

/**
  Acquire a table share instrumentation.
  @param temporary True for temporary tables
  @param share The SQL layer table share
  @return a table share instrumentation, or NULL
*/
typedef struct PSI_table_share *(*get_table_share_v1_t)(
  bool temporary, struct TABLE_SHARE *share);

/**
  Release a table share.
  @param share the table share to release
*/
typedef void (*release_table_share_v1_t)(struct PSI_table_share *share);

/**
  Drop a table share.
  @param temporary True for temporary tables
  @param schema_name the table schema name
  @param schema_name_length the table schema name length
  @param table_name the table name
  @param table_name_length the table name length
*/
typedef void (*drop_table_share_v1_t)(bool temporary,
                                      const char *schema_name,
                                      int schema_name_length,
                                      const char *table_name,
                                      int table_name_length);

/**
  Open an instrumentation table handle.
  @param share the table to open
  @param identity table handle identity
  @return a table handle, or NULL
*/
typedef struct PSI_table *(*open_table_v1_t)(struct PSI_table_share *share,
                                             const void *identity);

/**
  Unbind a table handle from the current thread.
  This operation happens when an opened table is added to the open table cache.
  @param table the table to unbind
*/
typedef void (*unbind_table_v1_t)(struct PSI_table *table);

/**
  Rebind a table handle to the current thread.
  This operation happens when a table from the open table cache
  is reused for a thread.
  @param table the table to unbind
*/
typedef PSI_table *(*rebind_table_v1_t)(PSI_table_share *share,
                                        const void *identity,
                                        PSI_table *table);

/**
  Close an instrumentation table handle.
  Note that the table handle is invalid after this call.
  @param table the table handle to close
*/
typedef void (*close_table_v1_t)(struct TABLE_SHARE *server_share,
                                 struct PSI_table *table);

/**
  Record a table instrumentation io wait start event.
  @param state data storage for the locker
  @param table the instrumented table
  @param op the operation to perform
  @param index the operation index
  @param src_file the source file name
  @param src_line the source line number
*/
typedef struct PSI_table_locker *(*start_table_io_wait_v1_t)(
  struct PSI_table_locker_state *state,
  struct PSI_table *table,
  enum PSI_table_io_operation op,
  uint index,
  const char *src_file,
  uint src_line);

/**
  Record a table instrumentation io wait end event.
  @param locker a table locker for the running thread
  @param numrows the number of rows involved in io
*/
typedef void (*end_table_io_wait_v1_t)(struct PSI_table_locker *locker,
                                       ulonglong numrows);

/**
  Record a table instrumentation lock wait start event.
  @param state data storage for the locker
  @param table the instrumented table
  @param op the operation to perform
  @param flags the operation flags
  @param src_file the source file name
  @param src_line the source line number
*/
typedef struct PSI_table_locker *(*start_table_lock_wait_v1_t)(
  struct PSI_table_locker_state *state,
  struct PSI_table *table,
  enum PSI_table_lock_operation op,
  ulong flags,
  const char *src_file,
  uint src_line);

/**
  Record a table instrumentation lock wait end event.
  @param locker a table locker for the running thread
*/
typedef void (*end_table_lock_wait_v1_t)(struct PSI_table_locker *locker);

/**
  Record a table unlock event.
  @param table instrumentation for the table being unlocked
*/
typedef void (*unlock_table_v1_t)(struct PSI_table *table);

/**
  Performance Schema Transaction Interface, version 1.
  @since PSI_TABLE_VERSION_1
*/
struct PSI_table_service_v1
{
  /** @sa get_table_share_v1_t. */
  get_table_share_v1_t get_table_share;
  /** @sa release_table_share_v1_t. */
  release_table_share_v1_t release_table_share;
  /** @sa drop_table_share_v1_t. */
  drop_table_share_v1_t drop_table_share;
  /** @sa open_table_v1_t. */
  open_table_v1_t open_table;
  /** @sa unbind_table_v1_t. */
  unbind_table_v1_t unbind_table;
  /** @sa rebind_table_v1_t. */
  rebind_table_v1_t rebind_table;
  /** @sa close_table_v1_t. */
  close_table_v1_t close_table;
  /** @sa start_table_io_wait_v1_t. */
  start_table_io_wait_v1_t start_table_io_wait;
  /** @sa end_table_io_wait_v1_t. */
  end_table_io_wait_v1_t end_table_io_wait;
  /** @sa start_table_lock_wait_v1_t. */
  start_table_lock_wait_v1_t start_table_lock_wait;
  /** @sa end_table_lock_wait_v1_t. */
  end_table_lock_wait_v1_t end_table_lock_wait;
  /** @sa end_table_lock_wait_v1_t. */
  unlock_table_v1_t unlock_table;
};

#endif /* HAVE_PSI_TABLE_1 */

/* Export the required version */
#ifdef USE_PSI_TABLE_1
typedef struct PSI_table_service_v1 PSI_table_service_t;
#else
typedef struct PSI_placeholder PSI_table_service_t;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_table_service_t *psi_table_service;

/** @} (end of group psi_abi_table) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_TRANSACTION_H */
