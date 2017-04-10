/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
  */

#ifndef PFS_TABLE_HELPER_H
#define PFS_TABLE_HELPER_H

/**
  @file storage/perfschema/table_helper.h
  Helpers to implement a performance schema table.
*/

#include <stddef.h>
#include <sys/types.h>

#include "lex_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "pfs_column_types.h"
#include "pfs_digest.h"
#include "pfs_engine_table.h"
#include "pfs_events.h"
#include "pfs_instr_class.h"
#include "pfs_setup_actor.h"
#include "pfs_stat.h"
#include "pfs_timer.h"

/*
  Write MD5 hash value in a string to be used
  as DIGEST for the statement.
*/
#define MD5_HASH_TO_STRING(_hash, _str)       \
  sprintf(_str,                               \
          "%02x%02x%02x%02x%02x%02x%02x%02x"  \
          "%02x%02x%02x%02x%02x%02x%02x%02x", \
          _hash[0],                           \
          _hash[1],                           \
          _hash[2],                           \
          _hash[3],                           \
          _hash[4],                           \
          _hash[5],                           \
          _hash[6],                           \
          _hash[7],                           \
          _hash[8],                           \
          _hash[9],                           \
          _hash[10],                          \
          _hash[11],                          \
          _hash[12],                          \
          _hash[13],                          \
          _hash[14],                          \
          _hash[15])

#define MD5_HASH_TO_STRING_LENGTH 32

struct PFS_host;
struct PFS_user;
struct PFS_account;
struct PFS_object_name;
struct PFS_program;
class System_variable;
class Status_variable;
struct User_variable;
struct PFS_events_waits;
struct PFS_table;
struct PFS_prepared_stmt;
struct PFS_metadata_lock;
struct PFS_setup_actor;
struct PFS_setup_object;

/**
  @file storage/perfschema/table_helper.h
  Performance schema table helpers (declarations).
*/

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  Helper, assign a value to a @c long field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_long(Field *f, long value);

/**
  Helper, assign a value to a @c ulong field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_ulong(Field *f, ulong value);

/**
  Helper, assign a value to a @c longlong field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_longlong(Field *f, longlong value);

/**
  Helper, assign a value to a @c ulonglong field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_ulonglong(Field *f, ulonglong value);

/**
  Helper, assign a value to a @code char utf8 @endcode field.
  @param f the field to set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_char_utf8(Field *f, const char *str, uint len);

/**
  Helper, assign a value to a @code varchar utf8 @endcode field.
  @param f the field to set
  @param cs the string character set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_varchar(Field *f,
                       const CHARSET_INFO *cs,
                       const char *str,
                       uint len);

/**
  Helper, assign a value to a @code varchar utf8 @endcode field.
  @param f the field to set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_varchar_utf8(Field *f, const char *str, uint len);

/**
  Helper, assign a value to a @code varchar utf8mb4 @endcode field.
  @param f the field to set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_varchar_utf8mb4(Field *f, const char *str, uint len);

/**
  Helper, assign a value to a @code varchar utf8 @endcode field.
  @param f the field to set
  @param str the string to assign
*/
void set_field_varchar_utf8(Field *f, const char *str);

/**
  Helper, assign a value to a @code varchar utf8mb4 @endcode field.
  @param f the field to set
  @param str the string to assign
*/
void set_field_varchar_utf8mb4(Field *f, const char *str);

/**
  Helper, assign a value to a @code longtext utf8 @endcode field.
  @param f the field to set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_longtext_utf8(Field *f, const char *str, uint len);

/**
  Helper, assign a value to a blob field.
  @param f the field to set
  @param val the value to assign
  @param len the length of the string to assign
*/
void set_field_blob(Field *f, const char *val, uint len);

/**
  Helper, assign a value to an @c enum field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_enum(Field *f, ulonglong value);

/**
  Helper, assign a value to a @c timestamp field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_timestamp(Field *f, ulonglong value);

/**
  Helper, assign a value to a @c double field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_double(Field *f, double value);

/**
  Helper, read a value from an @c ulonglong field.
  @param f the field to read
  @return the field value
*/
ulonglong get_field_ulonglong(Field *f);

/**
  Helper, read a value from an @c enum field.
  @param f the field to read
  @return the field value
*/
ulonglong get_field_enum(Field *f);

/**
  Helper, read a value from a @code char utf8 @endcode field.
  @param f the field to read
  @param[out] val the field value
  @return the field value
*/
String *get_field_char_utf8(Field *f, String *val);

/**
  Helper, read a value from a @code varchar utf8 @endcode field.
  @param f the field to read
  @param[out] val the field value
  @return the field value
*/
String *get_field_varchar_utf8(Field *f, String *val);

/** Name space, internal views used within table setup_instruments. */
struct PFS_instrument_view_constants
{
  static const uint FIRST_INSTRUMENT = 1;

  static const uint FIRST_VIEW = 1;
  static const uint VIEW_MUTEX = 1;
  static const uint VIEW_RWLOCK = 2;
  static const uint VIEW_COND = 3;
  static const uint VIEW_FILE = 4;
  static const uint VIEW_TABLE = 5;
  static const uint VIEW_SOCKET = 6;
  static const uint VIEW_IDLE = 7;
  static const uint VIEW_METADATA = 8;
  static const uint LAST_VIEW = 8;

  static const uint VIEW_THREAD = 9;
  static const uint VIEW_STAGE = 10;
  static const uint VIEW_STATEMENT = 11;
  static const uint VIEW_TRANSACTION = 12;
  static const uint VIEW_BUILTIN_MEMORY = 13;
  static const uint VIEW_MEMORY = 14;
  static const uint VIEW_ERROR = 15;

  static const uint LAST_INSTRUMENT = 15;
};

/** Name space, internal views used within object summaries. */
struct PFS_object_view_constants
{
  static const uint FIRST_VIEW = 1;
  static const uint VIEW_TABLE = 1;
  static const uint VIEW_PROGRAM = 2;
  static const uint LAST_VIEW = 2;
};

/** Row fragment for column HOST. */
struct PFS_host_row
{
  /** Column HOST. */
  char m_hostname[HOSTNAME_LENGTH];
  /** Length in bytes of @c m_hostname. */
  uint m_hostname_length;

  /** Build a row from a memory buffer. */
  int make_row(PFS_host *pfs);
  /** Set a table field from the row. */
  void set_field(Field *f);
};

/** Row fragment for column USER. */
struct PFS_user_row
{
  /** Column USER. */
  char m_username[USERNAME_LENGTH];
  /** Length in bytes of @c m_username. */
  uint m_username_length;

  /** Build a row from a memory buffer. */
  int make_row(PFS_user *pfs);
  /** Set a table field from the row. */
  void set_field(Field *f);
};

/** Row fragment for columns USER, HOST. */
struct PFS_account_row
{
  /** Column USER. */
  char m_username[USERNAME_LENGTH];
  /** Length in bytes of @c m_username. */
  uint m_username_length;
  /** Column HOST. */
  char m_hostname[HOSTNAME_LENGTH];
  /** Length in bytes of @c m_hostname. */
  uint m_hostname_length;

  /** Build a row from a memory buffer. */
  int make_row(PFS_account *pfs);
  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

/** Row fragment for columns DIGEST, DIGEST_TEXT. */
struct PFS_digest_row
{
  /** Column SCHEMA_NAME. */
  char m_schema_name[NAME_LEN];
  /** Length in bytes of @c m_schema_name. */
  uint m_schema_name_length;
  /** Column DIGEST. */
  char m_digest[COL_DIGEST_SIZE];
  /** Length in bytes of @c m_digest. */
  uint m_digest_length;
  /** Column DIGEST_TEXT. */
  String m_digest_text;

  /** Build a row from a memory buffer. */
  int make_row(PFS_statements_digest_stat *);
  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

/** Row fragment for column EVENT_NAME. */
struct PFS_event_name_row
{
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;

  /** Build a row from a memory buffer. */
  inline int
  make_row(PFS_instr_class *pfs)
  {
    m_name = pfs->m_name;
    m_name_length = pfs->m_name_length;
    return 0;
  }

  /** Set a table field from the row. */
  inline void
  set_field(Field *f)
  {
    set_field_varchar_utf8(f, m_name, m_name_length);
  }
};

/** Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME. */
struct PFS_object_row
{
  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column SCHEMA_NAME. */
  char m_schema_name[NAME_LEN];
  /** Length in bytes of @c m_schema_name. */
  uint m_schema_name_length;
  /** Column OBJECT_NAME. */
  char m_object_name[NAME_LEN];
  /** Length in bytes of @c m_object_name. */
  uint m_object_name_length;

  /** Build a row from a memory buffer. */
  int make_row(PFS_table_share *pfs);
  int make_row(PFS_program *pfs);
  int make_row(const MDL_key *pfs);
  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
  void set_nullable_field(uint index, Field *f);
};

/** Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME, INDEX_NAME.
 */
struct PFS_index_row
{
  PFS_object_row m_object_row;
  /** Column INDEX_NAME. */
  char m_index_name[NAME_LEN];
  /** Length in bytes of @c m_index_name. */
  uint m_index_name_length;

  /** Build a row from a memory buffer. */
  int make_index_name(PFS_table_share_index *pfs_index, uint table_index);
  int make_row(PFS_table_share *pfs,
               PFS_table_share_index *pfs_index,
               uint table_index);
  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

/** Row fragment for single statistics columns (COUNT, SUM, MIN, AVG, MAX) */
struct PFS_stat_row
{
  /** Column COUNT_STAR. */
  ulonglong m_count;
  /** Column SUM_TIMER_WAIT. */
  ulonglong m_sum;
  /** Column MIN_TIMER_WAIT. */
  ulonglong m_min;
  /** Column AVG_TIMER_WAIT. */
  ulonglong m_avg;
  /** Column MAX_TIMER_WAIT. */
  ulonglong m_max;

  inline void
  reset()
  {
    m_count = 0;
    m_sum = 0;
    m_min = 0;
    m_avg = 0;
    m_max = 0;
  }

  /** Build a row with timer fields from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_single_stat *stat)
  {
    m_count = stat->m_count;

    if ((m_count != 0) && stat->has_timed_stats())
    {
      m_sum = normalizer->wait_to_pico(stat->m_sum);
      m_min = normalizer->wait_to_pico(stat->m_min);
      m_max = normalizer->wait_to_pico(stat->m_max);
      m_avg = normalizer->wait_to_pico(stat->m_sum / m_count);
    }
    else
    {
      m_sum = 0;
      m_min = 0;
      m_avg = 0;
      m_max = 0;
    }
  }

  /** Set a table field from the row. */
  void
  set_field(uint index, Field *f)
  {
    switch (index)
    {
    case 0: /* COUNT */
      set_field_ulonglong(f, m_count);
      break;
    case 1: /* SUM */
      set_field_ulonglong(f, m_sum);
      break;
    case 2: /* MIN */
      set_field_ulonglong(f, m_min);
      break;
    case 3: /* AVG */
      set_field_ulonglong(f, m_avg);
      break;
    case 4: /* MAX */
      set_field_ulonglong(f, m_max);
      break;
    default:
      DBUG_ASSERT(false);
    }
  }
};

/** Row fragment for timer and byte count stats. Corresponds to PFS_byte_stat */
struct PFS_byte_stat_row
{
  PFS_stat_row m_waits;
  ulonglong m_bytes;

  /** Build a row with timer and byte count fields from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_byte_stat *stat)
  {
    m_waits.set(normalizer, stat);
    m_bytes = stat->m_bytes;
  }
};

/** Row fragment for table I/O statistics columns. */
struct PFS_table_io_stat_row
{
  PFS_stat_row m_all;
  PFS_stat_row m_all_read;
  PFS_stat_row m_all_write;
  PFS_stat_row m_fetch;
  PFS_stat_row m_insert;
  PFS_stat_row m_update;
  PFS_stat_row m_delete;

  /** Build a row from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_table_io_stat *stat)
  {
    PFS_single_stat all_read;
    PFS_single_stat all_write;
    PFS_single_stat all;

    m_fetch.set(normalizer, &stat->m_fetch);

    all_read.aggregate(&stat->m_fetch);

    m_insert.set(normalizer, &stat->m_insert);
    m_update.set(normalizer, &stat->m_update);
    m_delete.set(normalizer, &stat->m_delete);

    all_write.aggregate(&stat->m_insert);
    all_write.aggregate(&stat->m_update);
    all_write.aggregate(&stat->m_delete);

    all.aggregate(&all_read);
    all.aggregate(&all_write);

    m_all_read.set(normalizer, &all_read);
    m_all_write.set(normalizer, &all_write);
    m_all.set(normalizer, &all);
  }
};

/** Row fragment for table lock statistics columns. */
struct PFS_table_lock_stat_row
{
  PFS_stat_row m_all;
  PFS_stat_row m_all_read;
  PFS_stat_row m_all_write;
  PFS_stat_row m_read_normal;
  PFS_stat_row m_read_with_shared_locks;
  PFS_stat_row m_read_high_priority;
  PFS_stat_row m_read_no_insert;
  PFS_stat_row m_read_external;
  PFS_stat_row m_write_allow_write;
  PFS_stat_row m_write_concurrent_insert;
  PFS_stat_row m_write_low_priority;
  PFS_stat_row m_write_normal;
  PFS_stat_row m_write_external;

  /** Build a row from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_table_lock_stat *stat)
  {
    PFS_single_stat all_read;
    PFS_single_stat all_write;
    PFS_single_stat all;

    m_read_normal.set(normalizer, &stat->m_stat[PFS_TL_READ]);
    m_read_with_shared_locks.set(normalizer,
                                 &stat->m_stat[PFS_TL_READ_WITH_SHARED_LOCKS]);
    m_read_high_priority.set(normalizer,
                             &stat->m_stat[PFS_TL_READ_HIGH_PRIORITY]);
    m_read_no_insert.set(normalizer, &stat->m_stat[PFS_TL_READ_NO_INSERT]);
    m_read_external.set(normalizer, &stat->m_stat[PFS_TL_READ_EXTERNAL]);

    all_read.aggregate(&stat->m_stat[PFS_TL_READ]);
    all_read.aggregate(&stat->m_stat[PFS_TL_READ_WITH_SHARED_LOCKS]);
    all_read.aggregate(&stat->m_stat[PFS_TL_READ_HIGH_PRIORITY]);
    all_read.aggregate(&stat->m_stat[PFS_TL_READ_NO_INSERT]);
    all_read.aggregate(&stat->m_stat[PFS_TL_READ_EXTERNAL]);

    m_write_allow_write.set(normalizer,
                            &stat->m_stat[PFS_TL_WRITE_ALLOW_WRITE]);
    m_write_concurrent_insert.set(
      normalizer, &stat->m_stat[PFS_TL_WRITE_CONCURRENT_INSERT]);
    m_write_low_priority.set(normalizer,
                             &stat->m_stat[PFS_TL_WRITE_LOW_PRIORITY]);
    m_write_normal.set(normalizer, &stat->m_stat[PFS_TL_WRITE]);
    m_write_external.set(normalizer, &stat->m_stat[PFS_TL_WRITE_EXTERNAL]);

    all_write.aggregate(&stat->m_stat[PFS_TL_WRITE_ALLOW_WRITE]);
    all_write.aggregate(&stat->m_stat[PFS_TL_WRITE_CONCURRENT_INSERT]);
    all_write.aggregate(&stat->m_stat[PFS_TL_WRITE_LOW_PRIORITY]);
    all_write.aggregate(&stat->m_stat[PFS_TL_WRITE]);
    all_write.aggregate(&stat->m_stat[PFS_TL_WRITE_EXTERNAL]);

    all.aggregate(&all_read);
    all.aggregate(&all_write);

    m_all_read.set(normalizer, &all_read);
    m_all_write.set(normalizer, &all_write);
    m_all.set(normalizer, &all);
  }
};

/** Row fragment for stage statistics columns. */
struct PFS_stage_stat_row
{
  PFS_stat_row m_timer1_row;

  /** Build a row from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_stage_stat *stat)
  {
    m_timer1_row.set(normalizer, &stat->m_timer1_stat);
  }

  /** Set a table field from the row. */
  void
  set_field(uint index, Field *f)
  {
    m_timer1_row.set_field(index, f);
  }
};

/** Row fragment for statement statistics columns. */
struct PFS_statement_stat_row
{
  PFS_stat_row m_timer1_row;
  ulonglong m_error_count;
  ulonglong m_warning_count;
  ulonglong m_rows_affected;
  ulonglong m_lock_time;
  ulonglong m_rows_sent;
  ulonglong m_rows_examined;
  ulonglong m_created_tmp_disk_tables;
  ulonglong m_created_tmp_tables;
  ulonglong m_select_full_join;
  ulonglong m_select_full_range_join;
  ulonglong m_select_range;
  ulonglong m_select_range_check;
  ulonglong m_select_scan;
  ulonglong m_sort_merge_passes;
  ulonglong m_sort_range;
  ulonglong m_sort_rows;
  ulonglong m_sort_scan;
  ulonglong m_no_index_used;
  ulonglong m_no_good_index_used;

  /** Build a row from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_statement_stat *stat)
  {
    if (stat->m_timer1_stat.m_count != 0)
    {
      m_timer1_row.set(normalizer, &stat->m_timer1_stat);

      m_error_count = stat->m_error_count;
      m_warning_count = stat->m_warning_count;
      m_lock_time = stat->m_lock_time * MICROSEC_TO_PICOSEC;
      m_rows_affected = stat->m_rows_affected;
      m_rows_sent = stat->m_rows_sent;
      m_rows_examined = stat->m_rows_examined;
      m_created_tmp_disk_tables = stat->m_created_tmp_disk_tables;
      m_created_tmp_tables = stat->m_created_tmp_tables;
      m_select_full_join = stat->m_select_full_join;
      m_select_full_range_join = stat->m_select_full_range_join;
      m_select_range = stat->m_select_range;
      m_select_range_check = stat->m_select_range_check;
      m_select_scan = stat->m_select_scan;
      m_sort_merge_passes = stat->m_sort_merge_passes;
      m_sort_range = stat->m_sort_range;
      m_sort_rows = stat->m_sort_rows;
      m_sort_scan = stat->m_sort_scan;
      m_no_index_used = stat->m_no_index_used;
      m_no_good_index_used = stat->m_no_good_index_used;
    }
    else
    {
      m_timer1_row.reset();

      m_error_count = 0;
      m_warning_count = 0;
      m_lock_time = 0;
      m_rows_affected = 0;
      m_rows_sent = 0;
      m_rows_examined = 0;
      m_created_tmp_disk_tables = 0;
      m_created_tmp_tables = 0;
      m_select_full_join = 0;
      m_select_full_range_join = 0;
      m_select_range = 0;
      m_select_range_check = 0;
      m_select_scan = 0;
      m_sort_merge_passes = 0;
      m_sort_range = 0;
      m_sort_rows = 0;
      m_sort_scan = 0;
      m_no_index_used = 0;
      m_no_good_index_used = 0;
    }
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

/** Row fragment for stored program statistics. */
struct PFS_sp_stat_row
{
  PFS_stat_row m_timer1_row;

  /** Build a row from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_sp_stat *stat)
  {
    m_timer1_row.set(normalizer, &stat->m_timer1_stat);
  }

  /** Set a table field from the row. */
  inline void
  set_field(uint index, Field *f)
  {
    m_timer1_row.set_field(index, f);
  }
};

/** Row fragment for transaction statistics columns. */
struct PFS_transaction_stat_row
{
  PFS_stat_row m_timer1_row;
  PFS_stat_row m_read_write_row;
  PFS_stat_row m_read_only_row;
  ulonglong m_savepoint_count;
  ulonglong m_rollback_to_savepoint_count;
  ulonglong m_release_savepoint_count;

  /** Build a row from a memory buffer. */
  inline void
  set(time_normalizer *normalizer, const PFS_transaction_stat *stat)
  {
    /* Combine read write/read only stats */
    PFS_single_stat all;
    all.aggregate(&stat->m_read_only_stat);
    all.aggregate(&stat->m_read_write_stat);

    m_timer1_row.set(normalizer, &all);
    m_read_write_row.set(normalizer, &stat->m_read_write_stat);
    m_read_only_row.set(normalizer, &stat->m_read_only_stat);
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

/** Row fragment for error statistics columns. */
struct PFS_error_stat_row
{
  ulonglong m_count;
  ulonglong m_handled_count;
  uint m_error_index;
  ulonglong m_first_seen;
  ulonglong m_last_seen;

  /** Build a row from a memory buffer. */
  inline void
  set(const PFS_error_single_stat *stat, uint error_index)
  {
    m_count = stat->m_count;
    m_handled_count = stat->m_handled_count;
    m_error_index = error_index;
    m_first_seen = stat->m_first_seen;
    m_last_seen = stat->m_last_seen;
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f, server_error *temp_error);
};

/** Row fragment for connection statistics. */
struct PFS_connection_stat_row
{
  ulonglong m_current_connections;
  ulonglong m_total_connections;

  inline void
  set(const PFS_connection_stat *stat)
  {
    m_current_connections = stat->m_current_connections;
    m_total_connections = stat->m_total_connections;
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

void set_field_object_type(Field *f, enum_object_type object_type);
void set_field_lock_type(Field *f, PFS_TL_LOCK_TYPE lock_type);
void set_field_mdl_type(Field *f, opaque_mdl_type mdl_type);
void set_field_mdl_duration(Field *f, opaque_mdl_duration mdl_duration);
void set_field_mdl_status(Field *f, opaque_mdl_status mdl_status);
void set_field_isolation_level(Field *f, enum_isolation_level iso_level);
void set_field_xa_state(Field *f, enum_xa_transaction_state xa_state);

/** Row fragment for socket I/O statistics columns. */
struct PFS_socket_io_stat_row
{
  PFS_byte_stat_row m_read;
  PFS_byte_stat_row m_write;
  PFS_byte_stat_row m_misc;
  PFS_byte_stat_row m_all;

  inline void
  set(time_normalizer *normalizer, const PFS_socket_io_stat *stat)
  {
    PFS_byte_stat all;

    m_read.set(normalizer, &stat->m_read);
    m_write.set(normalizer, &stat->m_write);
    m_misc.set(normalizer, &stat->m_misc);

    /* Combine stats for all operations */
    all.aggregate(&stat->m_read);
    all.aggregate(&stat->m_write);
    all.aggregate(&stat->m_misc);

    m_all.set(normalizer, &all);
  }
};

/** Row fragment for file I/O statistics columns. */
struct PFS_file_io_stat_row
{
  PFS_byte_stat_row m_read;
  PFS_byte_stat_row m_write;
  PFS_byte_stat_row m_misc;
  PFS_byte_stat_row m_all;

  inline void
  set(time_normalizer *normalizer, const PFS_file_io_stat *stat)
  {
    PFS_byte_stat all;

    m_read.set(normalizer, &stat->m_read);
    m_write.set(normalizer, &stat->m_write);
    m_misc.set(normalizer, &stat->m_misc);

    /* Combine stats for all operations */
    all.aggregate(&stat->m_read);
    all.aggregate(&stat->m_write);
    all.aggregate(&stat->m_misc);

    m_all.set(normalizer, &all);
  }
};

/** Row fragment for memory statistics columns. */
struct PFS_memory_stat_row
{
  PFS_memory_stat m_stat;

  /** Build a row from a memory buffer. */
  inline void
  set(const PFS_memory_stat *stat)
  {
    m_stat = *stat;
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

struct PFS_variable_name_row
{
public:
  PFS_variable_name_row()
  {
    m_str[0] = '\0';
    m_length = 0;
  }

  int make_row(const char *str, size_t length);

  char m_str[NAME_CHAR_LEN + 1];
  uint m_length;
};

struct PFS_variable_value_row
{
public:
  /** Set the row from a status variable. */
  int make_row(const Status_variable *var);

  /** Set the row from a system variable. */
  int make_row(const System_variable *var);

  /** Set a table field from the row. */
  void set_field(Field *f);

private:
  int make_row(const CHARSET_INFO *cs, const char *str, size_t length);

  char m_str[1024];
  uint m_length;
  const CHARSET_INFO *m_charset;
};

struct PFS_user_variable_value_row
{
public:
  PFS_user_variable_value_row() : m_value(NULL), m_value_length(0)
  {
  }

  PFS_user_variable_value_row(const PFS_user_variable_value_row &rhs)
  {
    make_row(rhs.m_value, rhs.m_value_length);
  }

  ~PFS_user_variable_value_row()
  {
    clear();
  }

  int make_row(const char *val, size_t length);

  const char *
  get_value() const
  {
    return m_value;
  }

  size_t
  get_value_length() const
  {
    return m_value_length;
  }

  void clear();

private:
  char *m_value;
  size_t m_value_length;
};

class PFS_key_long : public PFS_engine_key
{
public:
  PFS_key_long(const char *name) : PFS_engine_key(name), m_key_value(0)
  {
  }

  virtual ~PFS_key_long()
  {
  }

  virtual void
  read(PFS_key_reader &reader, enum ha_rkey_function find_flag)
  {
    m_find_flag = reader.read_long(find_flag, m_is_null, &m_key_value);
  }

protected:
  bool do_match(bool record_null, long record_value);

private:
  long m_key_value;
};

class PFS_key_ulong : public PFS_engine_key
{
public:
  PFS_key_ulong(const char *name) : PFS_engine_key(name), m_key_value(0)
  {
  }

  virtual ~PFS_key_ulong()
  {
  }

  virtual void
  read(PFS_key_reader &reader, enum ha_rkey_function find_flag)
  {
    m_find_flag = reader.read_ulong(find_flag, m_is_null, &m_key_value);
  }

protected:
  bool do_match(bool record_null, ulong record_value);

private:
  ulong m_key_value;
};

class PFS_key_ulonglong : public PFS_engine_key
{
public:
  PFS_key_ulonglong(const char *name) : PFS_engine_key(name), m_key_value(0)
  {
  }

  virtual ~PFS_key_ulonglong()
  {
  }

  virtual void
  read(PFS_key_reader &reader, enum ha_rkey_function find_flag)
  {
    m_find_flag = reader.read_ulonglong(find_flag, m_is_null, &m_key_value);
  }

protected:
  bool do_match(bool record_null, ulonglong record_value);

private:
  ulonglong m_key_value;
};

class PFS_key_thread_id : public PFS_key_ulonglong
{
public:
  PFS_key_thread_id(const char *name) : PFS_key_ulonglong(name)
  {
  }

  ~PFS_key_thread_id()
  {
  }

  bool match(ulonglong thread_id);
  bool match(const PFS_thread *pfs);
  bool match_owner(const PFS_table *pfs);
  bool match_owner(const PFS_socket *pfs);
  bool match_owner(const PFS_mutex *pfs);
  bool match_owner(const PFS_prepared_stmt *pfs);
  bool match_owner(const PFS_metadata_lock *pfs);
  bool match_writer(const PFS_rwlock *pfs);
};

class PFS_key_event_id : public PFS_key_ulonglong
{
public:
  PFS_key_event_id(const char *name) : PFS_key_ulonglong(name)
  {
  }

  ~PFS_key_event_id()
  {
  }

  bool match(ulonglong event_id);
  bool match(const PFS_events *pfs);
  bool match(const PFS_events_waits *pfs);
  bool match_owner(const PFS_table *pfs);
  bool match_owner(const PFS_prepared_stmt *pfs);
  bool match_owner(const PFS_metadata_lock *pfs);
};

class PFS_key_processlist_id : public PFS_key_ulonglong
{
public:
  PFS_key_processlist_id(const char *name) : PFS_key_ulonglong(name)
  {
  }

  ~PFS_key_processlist_id()
  {
  }

  bool match(const PFS_thread *pfs);
};

class PFS_key_engine_transaction_id : public PFS_key_ulonglong
{
public:
  PFS_key_engine_transaction_id(const char *name) : PFS_key_ulonglong(name)
  {
  }

  ~PFS_key_engine_transaction_id()
  {
  }

  bool match(ulonglong engine_transaction_id);
};

class PFS_key_processlist_id_int : public PFS_key_long
{
public:
  PFS_key_processlist_id_int(const char *name) : PFS_key_long(name)
  {
  }

  ~PFS_key_processlist_id_int()
  {
  }

  bool match(const PFS_thread *pfs);
};

class PFS_key_thread_os_id : public PFS_key_ulonglong
{
public:
  PFS_key_thread_os_id(const char *name) : PFS_key_ulonglong(name)
  {
  }

  ~PFS_key_thread_os_id()
  {
  }

  bool match(const PFS_thread *pfs);
};

class PFS_key_statement_id : public PFS_key_ulonglong
{
public:
  PFS_key_statement_id(const char *name) : PFS_key_ulonglong(name)
  {
  }

  ~PFS_key_statement_id()
  {
  }

  bool match(const PFS_prepared_stmt *pfs);
};

class PFS_key_socket_id : public PFS_key_long
{
public:
  PFS_key_socket_id(const char *name) : PFS_key_long(name)
  {
  }

  ~PFS_key_socket_id()
  {
  }

  bool match(const PFS_socket *pfs);
};

class PFS_key_port : public PFS_key_long
{
public:
  PFS_key_port(const char *name) : PFS_key_long(name)
  {
  }

  ~PFS_key_port()
  {
  }

  bool match(const PFS_socket *pfs);
};

class PFS_key_error_number : public PFS_key_long
{
public:
  PFS_key_error_number(const char *name) : PFS_key_long(name)
  {
  }

  ~PFS_key_error_number()
  {
  }

  bool match_error_index(uint error_index);
};

template <int SIZE>
class PFS_key_string : public PFS_engine_key
{
public:
  PFS_key_string(const char *name) : PFS_engine_key(name), m_key_value_length(0)
  {
  }

  virtual ~PFS_key_string()
  {
  }

  virtual void
  read(PFS_key_reader &reader, enum ha_rkey_function find_flag)
  {
    if (reader.get_key_type() == HA_KEYTYPE_TEXT)
    {
      m_find_flag = reader.read_text_utf8(find_flag,
                                          m_is_null,
                                          m_key_value,
                                          &m_key_value_length,
                                          sizeof(m_key_value));
    }
    else
    {
      m_find_flag = reader.read_varchar_utf8(find_flag,
                                             m_is_null,
                                             m_key_value,
                                             &m_key_value_length,
                                             sizeof(m_key_value));
    }
  }

protected:
  bool do_match(bool record_null,
                const char *record_value,
                size_t record_value_length);
  bool do_match_prefix(bool record_null,
                       const char *record_value,
                       size_t record_value_length);

private:
  char m_key_value[SIZE *
                   SYSTEM_CHARSET_MBMAXLEN];  // FIXME FILENAME_CHARSET_MBMAXLEN
                                              // for file names
  uint m_key_value_length;
};

class PFS_key_thread_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH>
{
public:
  PFS_key_thread_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_thread_name()
  {
  }

  bool match(const PFS_thread *pfs);
  bool match(const PFS_thread_class *klass);
};

class PFS_key_event_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH>
{
public:
  PFS_key_event_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_event_name()
  {
  }

  bool match(const PFS_instr_class *klass);
  bool match(const PFS_mutex *pfs);
  bool match(const PFS_rwlock *pfs);
  bool match(const PFS_cond *pfs);
  bool match(const PFS_file *pfs);
  bool match(const PFS_socket *pfs);
  bool match_view(uint view);
};

class PFS_key_user : public PFS_key_string<USERNAME_LENGTH>
{
public:
  PFS_key_user(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_user()
  {
  }

  bool match(const PFS_thread *pfs);
  bool match(const PFS_user *pfs);
  bool match(const PFS_account *pfs);
  bool match(const PFS_setup_actor *pfs);
};

class PFS_key_host : public PFS_key_string<HOSTNAME_LENGTH>
{
public:
  PFS_key_host(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_host()
  {
  }

  bool match(const PFS_thread *pfs);
  bool match(const PFS_host *pfs);
  bool match(const PFS_account *pfs);
  bool match(const PFS_setup_actor *pfs);
  bool match(const char *host, uint host_length);
};

class PFS_key_role : public PFS_key_string<ROLENAME_LENGTH>
{
public:
  PFS_key_role(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_role()
  {
  }

  bool match(const PFS_setup_actor *pfs);
};

class PFS_key_schema : public PFS_key_string<NAME_CHAR_LEN>
{
public:
  PFS_key_schema(const char *schema) : PFS_key_string(schema)
  {
  }

  ~PFS_key_schema()
  {
  }

  bool match(const PFS_statements_digest_stat *pfs);
};

class PFS_key_digest : public PFS_key_string<MAX_KEY_LENGTH>
{
public:
  PFS_key_digest(const char *digest) : PFS_key_string(digest)
  {
  }

  ~PFS_key_digest()
  {
  }

  bool match(PFS_statements_digest_stat *pfs);
};

class PFS_key_bucket_number : public PFS_key_ulong
{
public:
  PFS_key_bucket_number(const char *name) : PFS_key_ulong(name)
  {
  }

  ~PFS_key_bucket_number()
  {
  }

  bool match(ulong value);
};

/* Generic NAME key */
class PFS_key_name : public PFS_key_string<NAME_CHAR_LEN>
{
public:
  PFS_key_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_name()
  {
  }

  bool match(const LEX_STRING *name);
  bool match(const char *name, uint name_length);
};

class PFS_key_variable_name : public PFS_key_string<NAME_CHAR_LEN>
{
public:
  PFS_key_variable_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_variable_name()
  {
  }

  bool match(const System_variable *pfs);
  bool match(const Status_variable *pfs);
  bool match(const PFS_variable_name_row *pfs);
};

// FIXME: 32
class PFS_key_engine_name : public PFS_key_string<32>
{
public:
  PFS_key_engine_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_engine_name()
  {
  }

  bool match(const char *engine_name, size_t length);
};

// FIXME: 128
class PFS_key_engine_lock_id : public PFS_key_string<128>
{
public:
  PFS_key_engine_lock_id(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_engine_lock_id()
  {
  }

  bool match(const char *engine_lock_id, size_t length);
};

class PFS_key_ip : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH>  // FIXME
// <INET6_ADDRSTRLEN+1>
// fails on freebsd
{
public:
  PFS_key_ip(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_ip()
  {
  }

  bool match(const PFS_socket *pfs);
  bool match(const char *ip, uint ip_length);
};

class PFS_key_statement_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH>
{
public:
  PFS_key_statement_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_statement_name()
  {
  }

  bool match(const PFS_prepared_stmt *pfs);
};

class PFS_key_file_name
  : public PFS_key_string<1350>  // FIXME FN_REFLEN or FN_REFLEN_SE
{
public:
  PFS_key_file_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_file_name()
  {
  }

  bool match(const PFS_file *pfs);
};

class PFS_key_object_schema : public PFS_key_string<NAME_CHAR_LEN>
{
public:
  PFS_key_object_schema(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_object_schema()
  {
  }

  bool match(const PFS_table_share *pfs);
  bool match(const PFS_program *pfs);
  bool match(const PFS_prepared_stmt *pfs);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_setup_object *pfs);
  bool match(const char *schema_name, uint schema_name_length);
};

class PFS_key_object_name : public PFS_key_string<NAME_CHAR_LEN>
{
public:
  PFS_key_object_name(const char *name) : PFS_key_string(name)
  {
  }

  ~PFS_key_object_name()
  {
  }

  bool match(const PFS_table_share *pfs);
  bool match(const PFS_program *pfs);
  bool match(const PFS_prepared_stmt *pfs);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_index_row *pfs);
  bool match(const PFS_setup_object *pfs);
  bool match(const char *schema_name, uint schema_name_length);
};

class PFS_key_object_type : public PFS_engine_key
{
public:
  PFS_key_object_type(const char *name)
    : PFS_engine_key(name), m_object_type(NO_OBJECT_TYPE)
  {
  }

  virtual ~PFS_key_object_type()
  {
  }

  virtual void read(PFS_key_reader &reader, enum ha_rkey_function find_flag);

  bool match(enum_object_type object_type);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_program *pfs);

  enum_object_type m_object_type;
};

class PFS_key_object_type_enum : public PFS_engine_key
{
public:
  PFS_key_object_type_enum(const char *name)
    : PFS_engine_key(name), m_object_type(NO_OBJECT_TYPE)
  {
  }

  virtual ~PFS_key_object_type_enum()
  {
  }

  virtual void read(PFS_key_reader &reader, enum ha_rkey_function find_flag);

  bool match(enum_object_type object_type);
  bool match(const PFS_prepared_stmt *pfs);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_program *pfs);

  enum_object_type m_object_type;
};

class PFS_key_object_instance : public PFS_engine_key
{
public:
  PFS_key_object_instance(const char *name)
    : PFS_engine_key(name), m_identity(NULL)
  {
  }

  virtual ~PFS_key_object_instance()
  {
  }

  virtual void
  read(PFS_key_reader &reader, enum ha_rkey_function find_flag)
  {
    ulonglong object_instance_begin;
    m_find_flag =
      reader.read_ulonglong(find_flag, m_is_null, &object_instance_begin);
    m_identity = (void *)object_instance_begin;
  }

  bool match(const PFS_table *pfs);
  bool match(const PFS_mutex *pfs);
  bool match(const PFS_rwlock *pfs);
  bool match(const PFS_cond *pfs);
  bool match(const PFS_file *pfs);
  bool match(const PFS_socket *pfs);
  bool match(const PFS_prepared_stmt *pfs);
  bool match(const PFS_metadata_lock *pfs);

  const void *m_identity;
};

/** @} */

#endif
