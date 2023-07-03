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

#ifndef PFS_COLUMN_TYPES_H
#define PFS_COLUMN_TYPES_H

#include <stddef.h>

/**
  @file storage/perfschema/pfs_column_types.h
  Data types for columns used in the performance schema tables (declarations)
*/

/** Size of the OBJECT_SCHEMA columns, in characters. */
#define COL_OBJECT_SCHEMA_CHAR_SIZE 64
/** Size of the OBJECT_SCHEMA columns, in bytes. */
#define COL_OBJECT_SCHEMA_SIZE (COL_OBJECT_SCHEMA_CHAR_SIZE * 1)

/**
  Size of the extended OBJECT_NAME columns, in characters.
  'Extended' columns are used when the object name also represents
  the name of a non SQL object, such as a file name.
  Size in bytes of:
  - performance_schema.events_waits_current (OBJECT_NAME)
  - performance_schema.events_waits_history (OBJECT_NAME)
  - performance_schema.events_waits_history_long (OBJECT_NAME)
*/
#define COL_OBJECT_NAME_EXTENDED_CHAR_SIZE 512
/** Size of the extended OBJECT_NAME columns, in bytes. */
#define COL_OBJECT_NAME_EXTENDED_SIZE (COL_OBJECT_NAME_EXTENDED_CHAR_SIZE * 1)

/** Size of the OBJECT_NAME columns, in characters. */
#define COL_OBJECT_NAME_CHAR_SIZE 64
/** Size of the OBJECT_NAME columns, in bytes. */
#define COL_OBJECT_NAME_SIZE (COL_OBJECT_NAME_CHAR_SIZE * 1)

/** Size of the INDEX_NAME columns, in characters. */
#define COL_INDEX_NAME_CHAR_SIZE 64
/** Size of the INDEX_NAME columns, in bytes. */
#define COL_INDEX_NAME_SIZE (COL_INDEX_NAME_CHAR_SIZE * 1)

/**
  Size of INFO columns, in characters.
  Size of:
  - performance_schema.events_statement_current (INFO)
  - performance_schema.events_statement_history (INFO)
  - performance_schema.events_statement_history_long (INFO)
*/
#define COL_INFO_CHAR_SIZE 1024
/** Size of INFO columns, in bytes. */
#define COL_INFO_SIZE (COL_INFO_CHAR_SIZE * 1)

/** Size of the SOURCE columns, in characters. */
#define COL_SOURCE_CHAR_SIZE 64
/** Size of the SOURCE columns, in bytes. */
#define COL_SOURCE_SIZE (COL_SOURCE_CHAR_SIZE * 1)

/**
  Enum values for the TIMER_NAME columns.
  This enum is found in the following tables:
  - performance_schema.performance_timer (TIMER_NAME)
*/
enum enum_timer_name {
  TIMER_NAME_CYCLE = 1,
  TIMER_NAME_NANOSEC = 2,
  TIMER_NAME_MICROSEC = 3,
  TIMER_NAME_MILLISEC = 4,
  TIMER_NAME_THREAD_CPU = 5,
};

/** Integer, first value of @sa enum_timer_name. */
#define FIRST_TIMER_NAME (static_cast<int>(TIMER_NAME_CYCLE))
/** Integer, last value of @sa enum_timer_name. */
#define LAST_TIMER_NAME (static_cast<int>(TIMER_NAME_THREAD_CPU))
/** Integer, number of values of @sa enum_timer_name. */
#define COUNT_TIMER_NAME (LAST_TIMER_NAME - FIRST_TIMER_NAME + 1)

/**
  Enum values for the various YES/NO columns.
  This enum is found in the following tables:
  - performance_schema.setup_instruments (ENABLED)
  - performance_schema.setup_instruments (TIMED)
  - performance_schema.setup_consumers (ENABLED)
*/
enum enum_yes_no { ENUM_YES = 1, ENUM_NO = 2 };

/**
  Enum values for the various EXECUTION_ENGINE columns.
  This enum is found in the following tables:
  - performance_schema.threads
  - performance_schema.processlist
  - performance_schema.events_statements_current
  - performance_schema.events_statements_history
  - performance_schema.events_statements_history_long
*/
enum enum_executed_on_engine { ENUM_PRIMARY = 1, ENUM_SECONDARY = 2 };

/**
  Enum values for the various OPERATION columns.
  This enum is found in the following tables:
  - performance_schema.events_waits_current (OPERATION)
  - performance_schema.events_waits_history (OPERATION)
  - performance_schema.events_waits_history_long (OPERATION)
*/
enum enum_operation_type {
  /* Mutex operations */
  OPERATION_TYPE_LOCK = 1,
  OPERATION_TYPE_TRYLOCK = 2,

  /* Rwlock operations (RW-lock) */
  OPERATION_TYPE_READLOCK = 3,
  OPERATION_TYPE_WRITELOCK = 4,
  OPERATION_TYPE_TRYREADLOCK = 5,
  OPERATION_TYPE_TRYWRITELOCK = 6,
  OPERATION_TYPE_UNLOCK = 7,

  /* Rwlock operations (SX-lock) */
  OPERATION_TYPE_SHAREDLOCK = 8,
  OPERATION_TYPE_SHAREDEXCLUSIVELOCK = 9,
  OPERATION_TYPE_EXCLUSIVELOCK = 10,
  OPERATION_TYPE_TRYSHAREDLOCK = 11,
  OPERATION_TYPE_TRYSHAREDEXCLUSIVELOCK = 12,
  OPERATION_TYPE_TRYEXCLUSIVELOCK = 13,
  OPERATION_TYPE_SHAREDUNLOCK = 14,
  OPERATION_TYPE_SHAREDEXCLUSIVEUNLOCK = 15,
  OPERATION_TYPE_EXCLUSIVEUNLOCK = 16,

  /* Cond operations */
  OPERATION_TYPE_WAIT = 17,
  OPERATION_TYPE_TIMEDWAIT = 18,

  /* File operations */
  OPERATION_TYPE_FILECREATE = 19,
  OPERATION_TYPE_FILECREATETMP = 20,
  OPERATION_TYPE_FILEOPEN = 21,
  OPERATION_TYPE_FILESTREAMOPEN = 22,
  OPERATION_TYPE_FILECLOSE = 23,
  OPERATION_TYPE_FILESTREAMCLOSE = 24,
  OPERATION_TYPE_FILEREAD = 25,
  OPERATION_TYPE_FILEWRITE = 26,
  OPERATION_TYPE_FILESEEK = 27,
  OPERATION_TYPE_FILETELL = 28,
  OPERATION_TYPE_FILEFLUSH = 29,
  OPERATION_TYPE_FILESTAT = 30,
  OPERATION_TYPE_FILEFSTAT = 31,
  OPERATION_TYPE_FILECHSIZE = 32,
  OPERATION_TYPE_FILEDELETE = 33,
  OPERATION_TYPE_FILERENAME = 34,
  OPERATION_TYPE_FILESYNC = 35,

  /* Table I/O operations */
  OPERATION_TYPE_TABLE_FETCH = 36,
  OPERATION_TYPE_TABLE_WRITE_ROW = 37,
  OPERATION_TYPE_TABLE_UPDATE_ROW = 38,
  OPERATION_TYPE_TABLE_DELETE_ROW = 39,

  /* Table lock operations */
  OPERATION_TYPE_TL_READ_NORMAL = 40,
  OPERATION_TYPE_TL_READ_WITH_SHARED_LOCKS = 41,
  OPERATION_TYPE_TL_READ_HIGH_PRIORITY = 42,
  OPERATION_TYPE_TL_READ_NO_INSERTS = 43,
  OPERATION_TYPE_TL_WRITE_ALLOW_WRITE = 44,
  OPERATION_TYPE_TL_WRITE_CONCURRENT_INSERT = 45,
  OPERATION_TYPE_TL_WRITE_LOW_PRIORITY = 46,
  OPERATION_TYPE_TL_WRITE_NORMAL = 47,
  OPERATION_TYPE_TL_READ_EXTERNAL = 48,
  OPERATION_TYPE_TL_WRITE_EXTERNAL = 49,

  /* Socket operations */
  OPERATION_TYPE_SOCKETCREATE = 50,
  OPERATION_TYPE_SOCKETCONNECT = 51,
  OPERATION_TYPE_SOCKETBIND = 52,
  OPERATION_TYPE_SOCKETCLOSE = 53,
  OPERATION_TYPE_SOCKETSEND = 54,
  OPERATION_TYPE_SOCKETRECV = 55,
  OPERATION_TYPE_SOCKETSENDTO = 56,
  OPERATION_TYPE_SOCKETRECVFROM = 57,
  OPERATION_TYPE_SOCKETSENDMSG = 58,
  OPERATION_TYPE_SOCKETRECVMSG = 59,
  OPERATION_TYPE_SOCKETSEEK = 60,
  OPERATION_TYPE_SOCKETOPT = 61,
  OPERATION_TYPE_SOCKETSTAT = 62,
  OPERATION_TYPE_SOCKETSHUTDOWN = 63,
  OPERATION_TYPE_SOCKETSELECT = 64,

  /* Idle operation */
  OPERATION_TYPE_IDLE = 65,

  /* Metadata lock operation */
  OPERATION_TYPE_METADATA = 66
};
/** Integer, first value of @sa enum_operation_type. */
#define FIRST_OPERATION_TYPE (static_cast<int>(OPERATION_TYPE_LOCK))
/** Integer, last value of @sa enum_operation_type. */
#define LAST_OPERATION_TYPE (static_cast<int>(OPERATION_TYPE_METADATA))
/** Integer, number of values of @sa enum_operation_type. */
#define COUNT_OPERATION_TYPE (LAST_OPERATION_TYPE - FIRST_OPERATION_TYPE + 1)

/**
  Enum values for the various OBJECT_TYPE columns.
*/
enum enum_object_type : char {
  NO_OBJECT_TYPE = 0,

  /* Advertised in SQL ENUM */

  OBJECT_TYPE_EVENT = 1,
  OBJECT_TYPE_FUNCTION = 2,
  OBJECT_TYPE_PROCEDURE = 3,
  OBJECT_TYPE_TABLE = 4,
  OBJECT_TYPE_TRIGGER = 5,

  /* Not advertised in SQL ENUM, only displayed as VARCHAR */

  OBJECT_TYPE_TEMPORARY_TABLE = 6,
  OBJECT_TYPE_GLOBAL = 7,
  OBJECT_TYPE_SCHEMA = 8,
  OBJECT_TYPE_COMMIT = 9,
  OBJECT_TYPE_USER_LEVEL_LOCK = 10,
  OBJECT_TYPE_TABLESPACE = 11,
  OBJECT_TYPE_LOCKING_SERVICE = 12,
  OBJECT_TYPE_SRID = 13,
  OBJECT_TYPE_ACL_CACHE = 14,
  OBJECT_TYPE_COLUMN_STATISTICS = 15,
  OBJECT_TYPE_BACKUP_LOCK = 16,
  OBJECT_TYPE_RESOURCE_GROUPS = 17,
  OBJECT_TYPE_FOREIGN_KEY = 18,
  OBJECT_TYPE_CHECK_CONSTRAINT = 19
};
/** Integer, first value of @sa enum_object_type. */
#define FIRST_OBJECT_TYPE (static_cast<int>(OBJECT_TYPE_EVENT))
/** Integer, last value of @sa enum_object_type. */
#define LAST_OBJECT_TYPE (static_cast<int>(OBJECT_TYPE_CHECK_CONSTRAINT))
/** Integer, number of values of @sa enum_object_type. */
#define COUNT_OBJECT_TYPE (LAST_OBJECT_TYPE - FIRST_OBJECT_TYPE + 1)

void object_type_to_string(enum_object_type object_type, const char **string,
                           size_t *length);

void string_to_object_type(const char *string, size_t length,
                           enum_object_type *object_type);

/**
  Enum values for the NESTING_EVENT_TYPE columns.
  This enum is found in the following tables:
  - performance_schema.events_waits_current (NESTING_EVENT_TYPE)
  - performance_schema.events_stages_current (NESTING_EVENT_TYPE)
  - performance_schema.events_statements_current (NESTING_EVENT_TYPE)
*/
enum enum_event_type {
  EVENT_TYPE_TRANSACTION = 1,
  EVENT_TYPE_STATEMENT = 2,
  EVENT_TYPE_STAGE = 3,
  EVENT_TYPE_WAIT = 4
};

/** Integer, first value of @sa enum_event_type. */
#define FIRST_EVENT_TYPE (static_cast<int>(EVENT_TYPE_TRANSACTION))
/** Integer, last value of @sa enum_event_type. */
#define LAST_EVENT_TYPE (static_cast<int>(EVENT_TYPE_WAIT))
/** Integer, number of values of @sa enum_event_type. */
#define COUNT_EVENT_TYPE (LAST_EVENT_TYPE - FIRST_EVENT_TYPE + 1)

/**
  Enum values for transaction state columns.
*/
enum enum_transaction_state {
  TRANS_STATE_ACTIVE = 1,
  TRANS_STATE_COMMITTED = 2,
  TRANS_STATE_ROLLED_BACK = 3
};

/** Integer, first value of @sa enum_transaction_state. */
#define FIRST_TRANS_STATE (static_cast<int>(TRANS_STATE_ACTIVE))
/** Integer, last value of @sa enum_transaction_state. */
#define LAST_TRANS_STATE (static_cast<int>(TRANS_STATE_ROLLED_BACK))
/** Integer, number of values of @sa enum_transaction_state. */
#define COUNT_TRANS_STATE (LAST_TRANS_STATE - FIRST_TRANS_STATE + 1)

/**
  Enum values for XA transaction state columns. Enums 1-5 match those used by
  the server. See XID_STATE::enum xa_states in xa.h.
*/
enum enum_xa_transaction_state {
  TRANS_STATE_XA_NOTR,
  TRANS_STATE_XA_ACTIVE,
  TRANS_STATE_XA_IDLE,
  TRANS_STATE_XA_PREPARED,
  TRANS_STATE_XA_ROLLBACK_ONLY,
  TRANS_STATE_XA_COMMITTED
};

/** Integer, first value of @sa enum_xa_transaction_state. */
#define FIRST_TRANS_STATE_XA (static_cast<int>(TRANS_STATE_XA_NOTR))
/** Integer, last value of @sa enum_xa_transaction_state. */
#define LAST_TRANS_STATE_XA (static_cast<int>(TRANS_STATE_XA_COMMITTED))
/** Integer, number of values of @sa enum_xa_transaction_state. */
#define COUNT_TRANS_STATE_XA (LAST_TRANS_STATE_XA - FIRST_TRANS_STATE_XA + 1)

/**
  Enum values for transaction isolation level columns.
  See enum_tx_isolation in handler.h.
*/
enum enum_isolation_level {
  TRANS_LEVEL_READ_UNCOMMITTED,
  TRANS_LEVEL_READ_COMMITTED,
  TRANS_LEVEL_REPEATABLE_READ,
  TRANS_LEVEL_SERIALIZABLE
};

/** Integer, first value of @sa enum_isolation_level. */
#define FIRST_TRANS_LEVEL (static_cast<int>(TRANS_LEVEL_READ_UNCOMMITTED))
/** Integer, last value of @sa enum_isolation_level. */
#define LAST_TRANS_LEVEL (static_cast<int>(TRANS_LEVEL_SERIALIZABLE))
/** Integer, number of values of @sa enum_isolation_level. */
#define COUNT_TRANS_LEVEL (LAST_TRANS_LEVEL - FIRST_TRANS_LEVEL + 1)

/**
  Enum values for transaction access mode columns.
*/
enum enum_transaction_mode {
  TRANS_MODE_READ_ONLY = 1,
  TRANS_MODE_READ_WRITE = 2
};

/** Integer, first value of @sa enum_transaction_mode. */
#define FIRST_TRANS_MODE (static_cast<int>(TRANS_MODE_READ_WRITE))
/** Integer, last value of @sa enum_transaction_mode. */
#define LAST_TRANS_MODE (static_cast<int>(TRANS_MODE_READ_ONLY))
/** Integer, number of values of @sa enum_transaction_mode. */
#define COUNT_TRANS_MODE (LAST_TRANS_MODE - FIRST_TRANS_MODE + 1)

/* Flags exposed in setup_instruments.properties */
#define INSTR_PROPERTIES_SET_SINGLETON (1 << 0)
#define INSTR_PROPERTIES_SET_PROGRESS (1 << 1)
#define INSTR_PROPERTIES_SET_USER (1 << 2)
#define INSTR_PROPERTIES_SET_GLOBAL_STAT (1 << 3)
#define INSTR_PROPERTIES_SET_MUTABLE (1 << 4)
#define INSTR_PROPERTIES_SET_QUOTA_BY_DEFAULT (1 << 5)

/* Flags exposed in setup_instruments.enforced */
#define INSTR_FLAGS_SET_CONTROLLED (1 << 0)

/* All valid flags, only INSTR_FLAGS_SET_CONTROLLED so far. */
#define INSTR_FLAGS_MASK (1)

/* Flags exposed in setup_threads.properties */
#define THREAD_PROPERTIES_SET_SINGLETON (1 << 0)
#define THREAD_PROPERTIES_SET_USER (1 << 1)

#endif
