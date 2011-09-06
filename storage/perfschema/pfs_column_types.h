/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_COLUMN_TYPES_H
#define PFS_COLUMN_TYPES_H

/**
  @file storage/perfschema/pfs_column_types.h
  Data types for columns used in the performance schema tables (declarations)
*/

/** Size of the OBJECT_SCHEMA columns. */
#define COL_OBJECT_SCHEMA_SIZE 64

/**
  Size of the extended OBJECT_NAME columns.
  'Extended' columns are used when the object name also represents
  the name of a non SQL object, such as a file name.
  Size in bytes of:
  - performance_schema.events_waits_current (OBJECT_NAME)
  - performance_schema.events_waits_history (OBJECT_NAME)
  - performance_schema.events_waits_history_long (OBJECT_NAME)
*/
#define COL_OBJECT_NAME_EXTENDED_SIZE 512

/** Size of the OBJECT_NAME columns. */
#define COL_OBJECT_NAME_SIZE 64

/** Size of the INDEX_NAME columns. */
#define COL_INDEX_NAME_SIZE 64

/**
  Size of INFO columns.
  Size in bytes of:
  - performance_schema.events_statement_current (INFO)
  - performance_schema.events_statement_history (INFO)
  - performance_schema.events_statement_history_long (INFO)
*/
#define COL_INFO_SIZE 1024

/** Size of the SOURCE columns. */
#define COL_SOURCE_SIZE 64

/** Size of the DIGEST columns. */
#define COL_DIGEST_SIZE 64

/** Size of the DIGEST_TEXT columns. */
#define COL_DIGEST_TEXT_SIZE 1024

/**
  Enum values for the TIMER_NAME columns.
  This enum is found in the following tables:
  - performance_schema.setup_timer (TIMER_NAME)
  - performance_schema.performance_timer (TIMER_NAME)
*/
enum enum_timer_name
{
  TIMER_NAME_CYCLE= 1,
  TIMER_NAME_NANOSEC= 2,
  TIMER_NAME_MICROSEC= 3,
  TIMER_NAME_MILLISEC= 4,
  TIMER_NAME_TICK= 5
};

/** Integer, first value of @sa enum_timer_name. */
#define FIRST_TIMER_NAME (static_cast<int> (TIMER_NAME_CYCLE))
/** Integer, last value of @sa enum_timer_name. */
#define LAST_TIMER_NAME (static_cast<int> (TIMER_NAME_TICK))
/** Integer, number of values of @sa enum_timer_name. */
#define COUNT_TIMER_NAME (LAST_TIMER_NAME - FIRST_TIMER_NAME + 1)

/**
  Enum values for the various YES/NO columns.
  This enum is found in the following tables:
  - performance_schema.setup_instruments (ENABLED)
  - performance_schema.setup_instruments (TIMED)
  - performance_schema.setup_consumers (ENABLED)
*/
enum enum_yes_no
{
  ENUM_YES= 1,
  ENUM_NO= 2
};

/**
  Enum values for the various OPERATION columns.
  This enum is found in the following tables:
  - performance_schema.events_waits_current (OPERATION)
  - performance_schema.events_waits_history (OPERATION)
  - performance_schema.events_waits_history_long (OPERATION)
*/
enum enum_operation_type
{
  /* Mutex operations */
  OPERATION_TYPE_LOCK= 1,
  OPERATION_TYPE_TRYLOCK= 2,

  /* Rwlock operations */
  OPERATION_TYPE_READLOCK= 3,
  OPERATION_TYPE_WRITELOCK= 4,
  OPERATION_TYPE_TRYREADLOCK= 5,
  OPERATION_TYPE_TRYWRITELOCK= 6,

  /* Cond operations */
  OPERATION_TYPE_WAIT= 7,
  OPERATION_TYPE_TIMEDWAIT= 8,

  /* File operations */
  OPERATION_TYPE_FILECREATE= 9,
  OPERATION_TYPE_FILECREATETMP= 10,
  OPERATION_TYPE_FILEOPEN= 11,
  OPERATION_TYPE_FILESTREAMOPEN= 12,
  OPERATION_TYPE_FILECLOSE= 13,
  OPERATION_TYPE_FILESTREAMCLOSE= 14,
  OPERATION_TYPE_FILEREAD= 15,
  OPERATION_TYPE_FILEWRITE= 16,
  OPERATION_TYPE_FILESEEK= 17,
  OPERATION_TYPE_FILETELL= 18,
  OPERATION_TYPE_FILEFLUSH= 19,
  OPERATION_TYPE_FILESTAT= 20,
  OPERATION_TYPE_FILEFSTAT= 21,
  OPERATION_TYPE_FILECHSIZE= 22,
  OPERATION_TYPE_FILEDELETE= 23,
  OPERATION_TYPE_FILERENAME= 24,
  OPERATION_TYPE_FILESYNC= 25,

  /* Table io operations */
  OPERATION_TYPE_TABLE_FETCH= 26,
  OPERATION_TYPE_TABLE_WRITE_ROW= 27,
  OPERATION_TYPE_TABLE_UPDATE_ROW= 28,
  OPERATION_TYPE_TABLE_DELETE_ROW= 29,

  /* Table lock operations */
  OPERATION_TYPE_TL_READ_NORMAL= 30,
  OPERATION_TYPE_TL_READ_WITH_SHARED_LOCKS= 31,
  OPERATION_TYPE_TL_READ_HIGH_PRIORITY= 32,
  OPERATION_TYPE_TL_READ_NO_INSERTS= 33,
  OPERATION_TYPE_TL_WRITE_ALLOW_WRITE= 34,
  OPERATION_TYPE_TL_WRITE_CONCURRENT_INSERT= 35,
  OPERATION_TYPE_TL_WRITE_DELAYED= 36,
  OPERATION_TYPE_TL_WRITE_LOW_PRIORITY= 37,
  OPERATION_TYPE_TL_WRITE_NORMAL= 38,
  OPERATION_TYPE_TL_READ_EXTERNAL= 39,
  OPERATION_TYPE_TL_WRITE_EXTERNAL= 40,

  /* Socket operations */
  OPERATION_TYPE_SOCKETCREATE = 41,
  OPERATION_TYPE_SOCKETCONNECT = 42,
  OPERATION_TYPE_SOCKETBIND = 43,
  OPERATION_TYPE_SOCKETCLOSE = 44,
  OPERATION_TYPE_SOCKETSEND = 45,
  OPERATION_TYPE_SOCKETRECV = 46,
  OPERATION_TYPE_SOCKETSENDTO = 47,
  OPERATION_TYPE_SOCKETRECVFROM = 48,
  OPERATION_TYPE_SOCKETSENDMSG = 49,
  OPERATION_TYPE_SOCKETRECVMSG = 50,
  OPERATION_TYPE_SOCKETSEEK = 51,
  OPERATION_TYPE_SOCKETOPT = 52,
  OPERATION_TYPE_SOCKETSTAT = 53,
  OPERATION_TYPE_SOCKETSHUTDOWN = 54,
  OPERATION_TYPE_SOCKETSELECT = 55,

  /* Idle operation */
  OPERATION_TYPE_IDLE= 56
};
/** Integer, first value of @sa enum_operation_type. */
#define FIRST_OPERATION_TYPE (static_cast<int> (OPERATION_TYPE_LOCK))
/** Integer, last value of @sa enum_operation_type. */
#define LAST_OPERATION_TYPE (static_cast<int> (OPERATION_TYPE_IDLE))
/** Integer, number of values of @sa enum_operation_type. */
#define COUNT_OPERATION_TYPE (LAST_OPERATION_TYPE - FIRST_OPERATION_TYPE + 1)

/**
  Enum values for the various OBJECT_TYPE columns.
*/
enum enum_object_type
{
  OBJECT_TYPE_TABLE= 1,
  OBJECT_TYPE_TEMPORARY_TABLE= 2
};
/** Integer, first value of @sa enum_object_type. */
#define FIRST_OBJECT_TYPE (static_cast<int> (OBJECT_TYPE_TABLE))
/** Integer, last value of @sa enum_object_type. */
#define LAST_OBJECT_TYPE (static_cast<int> (OBJECT_TYPE_TEMPORARY_TABLE))
/** Integer, number of values of @sa enum_object_type. */
#define COUNT_OBJECT_TYPE (LAST_OBJECT_TYPE - FIRST_OBJECT_TYPE + 1)

/**
  Enum values for the NESTING_EVENT_TYPE columns.
  This enum is found in the following tables:
  - performance_schema.events_waits_current (NESTING_EVENT_TYPE)
  - performance_schema.events_stages_current (NESTING_EVENT_TYPE)
  - performance_schema.events_statements_current (NESTING_EVENT_TYPE)
*/
enum enum_event_type
{
  EVENT_TYPE_STATEMENT= 1,
  EVENT_TYPE_STAGE= 2,
  EVENT_TYPE_WAIT= 3
};

/** Integer, first value of @sa enum_event_type. */
#define FIRST_EVENT_TYPE (static_cast<int> (EVENT_TYPE_STATEMENT))
/** Integer, last value of @sa enum_event_type. */
#define LAST_EVENT_TYPE (static_cast<int> (EVENT_TYPE_WAIT))
/** Integer, number of values of @sa enum_event_type. */
#define COUNT_EVENT_TYPE (LAST_EVENT_TYPE - FIRST_EVENT_TYPE + 1)

#endif

