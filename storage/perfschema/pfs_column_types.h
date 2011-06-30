/* Copyright (c) 2008 MySQL AB, 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

/** Size of the SOURCE columns. */
#define COL_SOURCE_SIZE 64

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

#define FIRST_TIMER_NAME (static_cast<int> (TIMER_NAME_CYCLE))
#define LAST_TIMER_NAME (static_cast<int> (TIMER_NAME_TICK))
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
  OPERATION_TYPE_LOCK= 1,
  OPERATION_TYPE_TRYLOCK= 2,

  OPERATION_TYPE_READLOCK= 3,
  OPERATION_TYPE_WRITELOCK= 4,
  OPERATION_TYPE_TRYREADLOCK= 5,
  OPERATION_TYPE_TRYWRITELOCK= 6,

  OPERATION_TYPE_WAIT= 7,
  OPERATION_TYPE_TIMEDWAIT= 8,

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
  OPERATION_TYPE_FILESYNC= 25
};
#define FIRST_OPERATION_TYPE (static_cast<int> (OPERATION_TYPE_LOCK))
#define LAST_OPERATION_TYPE (static_cast<int> (OPERATION_TYPE_FILESYNC))
#define COUNT_OPERATION_TYPE (LAST_OPERATION_TYPE - FIRST_OPERATION_TYPE + 1)

#endif

