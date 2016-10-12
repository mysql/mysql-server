/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef PFS_TABLE_HELPER_H
#define PFS_TABLE_HELPER_H

#include "pfs_column_types.h"
#include "pfs_stat.h"
#include "pfs_timer.h"
#include "pfs_engine_table.h"
#include "pfs_instr_class.h"
#include "pfs_digest.h"

/*
  Write MD5 hash value in a string to be used
  as DIGEST for the statement.
*/
#define MD5_HASH_TO_STRING(_hash, _str)                    \
  sprintf(_str, "%02x%02x%02x%02x%02x%02x%02x%02x"         \
                "%02x%02x%02x%02x%02x%02x%02x%02x",        \
          _hash[0], _hash[1], _hash[2], _hash[3],          \
          _hash[4], _hash[5], _hash[6], _hash[7],          \
          _hash[8], _hash[9], _hash[10], _hash[11],        \
          _hash[12], _hash[13], _hash[14], _hash[15])

#define MD5_HASH_TO_STRING_LENGTH 32

struct PFS_host;
struct PFS_user;
struct PFS_account;
struct PFS_object_name;
struct PFS_program;
class System_variable;
class Status_variable;

/**
  @file storage/perfschema/table_helper.h
  Performance schema table helpers (declarations).
*/

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** Namespace, internal views used within table setup_instruments. */
struct PFS_instrument_view_constants
{
  static const uint FIRST_VIEW= 1;
  static const uint VIEW_MUTEX= 1;
  static const uint VIEW_RWLOCK= 2;
  static const uint VIEW_COND= 3;
  static const uint VIEW_FILE= 4;
  static const uint VIEW_TABLE= 5;
  static const uint VIEW_SOCKET= 6;
  static const uint VIEW_IDLE= 7;
  static const uint VIEW_METADATA= 8;
  static const uint LAST_VIEW= 8;
};

/** Namespace, internal views used within object summaries. */
struct PFS_object_view_constants
{
  static const uint FIRST_VIEW= 1;
  static const uint VIEW_TABLE= 1;
  static const uint VIEW_PROGRAM= 2;
  static const uint LAST_VIEW= 2;
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
  int make_row(PFS_statements_digest_stat*);
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
  inline void make_row(PFS_instr_class *pfs)
  {
    m_name= pfs->m_name;
    m_name_length= pfs->m_name_length;
  }

  /** Set a table field from the row. */
  inline void set_field(Field *f)
  {
    PFS_engine_table::set_field_varchar_utf8(f, m_name, m_name_length);
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

/** Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME, INDEX_NAME. */
struct PFS_index_row
{
  PFS_object_row m_object_row;
  /** Column INDEX_NAME. */
  char m_index_name[NAME_LEN];
  /** Length in bytes of @c m_index_name. */
  uint m_index_name_length;

  /** Build a row from a memory buffer. */
  int make_row(PFS_table_share *pfs, PFS_table_share_index *pfs_index,
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

  inline void reset()
  {
    m_count= 0;
    m_sum= 0;
    m_min= 0;
    m_avg= 0;
    m_max= 0;
  }

  /** Build a row with timer fields from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_single_stat *stat)
  {
    m_count= stat->m_count;

    if ((m_count != 0) && stat->has_timed_stats())
    {
      m_sum= normalizer->wait_to_pico(stat->m_sum);
      m_min= normalizer->wait_to_pico(stat->m_min);
      m_max= normalizer->wait_to_pico(stat->m_max);
      m_avg= normalizer->wait_to_pico(stat->m_sum / m_count);
    }
    else
    {
      m_sum= 0;
      m_min= 0;
      m_avg= 0;
      m_max= 0;
    }
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f)
  {
    switch (index)
    {
      case 0: /* COUNT */
        PFS_engine_table::set_field_ulonglong(f, m_count);
        break;
      case 1: /* SUM */
        PFS_engine_table::set_field_ulonglong(f, m_sum);
        break;
      case 2: /* MIN */
        PFS_engine_table::set_field_ulonglong(f, m_min);
        break;
      case 3: /* AVG */
        PFS_engine_table::set_field_ulonglong(f, m_avg);
        break;
      case 4: /* MAX */
        PFS_engine_table::set_field_ulonglong(f, m_max);
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
  ulonglong    m_bytes;

  /** Build a row with timer and byte count fields from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_byte_stat *stat)
  {
    m_waits.set(normalizer, stat);
    m_bytes= stat->m_bytes;
  }
};

/** Row fragment for table io statistics columns. */
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
  inline void set(time_normalizer *normalizer, const PFS_table_io_stat *stat)
  {
    PFS_single_stat all_read;
    PFS_single_stat all_write;
    PFS_single_stat all;

    m_fetch.set(normalizer, & stat->m_fetch);

    all_read.aggregate(& stat->m_fetch);

    m_insert.set(normalizer, & stat->m_insert);
    m_update.set(normalizer, & stat->m_update);
    m_delete.set(normalizer, & stat->m_delete);

    all_write.aggregate(& stat->m_insert);
    all_write.aggregate(& stat->m_update);
    all_write.aggregate(& stat->m_delete);

    all.aggregate(& all_read);
    all.aggregate(& all_write);

    m_all_read.set(normalizer, & all_read);
    m_all_write.set(normalizer, & all_write);
    m_all.set(normalizer, & all);
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
  inline void set(time_normalizer *normalizer, const PFS_table_lock_stat *stat)
  {
    PFS_single_stat all_read;
    PFS_single_stat all_write;
    PFS_single_stat all;

    m_read_normal.set(normalizer, & stat->m_stat[PFS_TL_READ]);
    m_read_with_shared_locks.set(normalizer, & stat->m_stat[PFS_TL_READ_WITH_SHARED_LOCKS]);
    m_read_high_priority.set(normalizer, & stat->m_stat[PFS_TL_READ_HIGH_PRIORITY]);
    m_read_no_insert.set(normalizer, & stat->m_stat[PFS_TL_READ_NO_INSERT]);
    m_read_external.set(normalizer, & stat->m_stat[PFS_TL_READ_EXTERNAL]);

    all_read.aggregate(& stat->m_stat[PFS_TL_READ]);
    all_read.aggregate(& stat->m_stat[PFS_TL_READ_WITH_SHARED_LOCKS]);
    all_read.aggregate(& stat->m_stat[PFS_TL_READ_HIGH_PRIORITY]);
    all_read.aggregate(& stat->m_stat[PFS_TL_READ_NO_INSERT]);
    all_read.aggregate(& stat->m_stat[PFS_TL_READ_EXTERNAL]);

    m_write_allow_write.set(normalizer, & stat->m_stat[PFS_TL_WRITE_ALLOW_WRITE]);
    m_write_concurrent_insert.set(normalizer, & stat->m_stat[PFS_TL_WRITE_CONCURRENT_INSERT]);
    m_write_low_priority.set(normalizer, & stat->m_stat[PFS_TL_WRITE_LOW_PRIORITY]);
    m_write_normal.set(normalizer, & stat->m_stat[PFS_TL_WRITE]);
    m_write_external.set(normalizer, & stat->m_stat[PFS_TL_WRITE_EXTERNAL]);

    all_write.aggregate(& stat->m_stat[PFS_TL_WRITE_ALLOW_WRITE]);
    all_write.aggregate(& stat->m_stat[PFS_TL_WRITE_CONCURRENT_INSERT]);
    all_write.aggregate(& stat->m_stat[PFS_TL_WRITE_LOW_PRIORITY]);
    all_write.aggregate(& stat->m_stat[PFS_TL_WRITE]);
    all_write.aggregate(& stat->m_stat[PFS_TL_WRITE_EXTERNAL]);

    all.aggregate(& all_read);
    all.aggregate(& all_write);

    m_all_read.set(normalizer, & all_read);
    m_all_write.set(normalizer, & all_write);
    m_all.set(normalizer, & all);
  }
};

/** Row fragment for stage statistics columns. */
struct PFS_stage_stat_row
{
  PFS_stat_row m_timer1_row;

  /** Build a row from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_stage_stat *stat)
  {
    m_timer1_row.set(normalizer, & stat->m_timer1_stat);
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f)
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
  inline void set(time_normalizer *normalizer, const PFS_statement_stat *stat)
  {
    if (stat->m_timer1_stat.m_count != 0)
    {
      m_timer1_row.set(normalizer, & stat->m_timer1_stat);

      m_error_count= stat->m_error_count;
      m_warning_count= stat->m_warning_count;
      m_lock_time= stat->m_lock_time * MICROSEC_TO_PICOSEC;
      m_rows_affected= stat->m_rows_affected;
      m_rows_sent= stat->m_rows_sent;
      m_rows_examined= stat->m_rows_examined;
      m_created_tmp_disk_tables= stat->m_created_tmp_disk_tables;
      m_created_tmp_tables= stat->m_created_tmp_tables;
      m_select_full_join= stat->m_select_full_join;
      m_select_full_range_join= stat->m_select_full_range_join;
      m_select_range= stat->m_select_range;
      m_select_range_check= stat->m_select_range_check;
      m_select_scan= stat->m_select_scan;
      m_sort_merge_passes= stat->m_sort_merge_passes;
      m_sort_range= stat->m_sort_range;
      m_sort_rows= stat->m_sort_rows;
      m_sort_scan= stat->m_sort_scan;
      m_no_index_used= stat->m_no_index_used;
      m_no_good_index_used= stat->m_no_good_index_used;
    }
    else
    {
      m_timer1_row.reset();

      m_error_count= 0;
      m_warning_count= 0;
      m_lock_time= 0;
      m_rows_affected= 0;
      m_rows_sent= 0;
      m_rows_examined= 0;
      m_created_tmp_disk_tables= 0;
      m_created_tmp_tables= 0;
      m_select_full_join= 0;
      m_select_full_range_join= 0;
      m_select_range= 0;
      m_select_range_check= 0;
      m_select_scan= 0;
      m_sort_merge_passes= 0;
      m_sort_range= 0;
      m_sort_rows= 0;
      m_sort_scan= 0;
      m_no_index_used= 0;
      m_no_good_index_used= 0;
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
  inline void set(time_normalizer *normalizer, const PFS_sp_stat *stat)
  {
    m_timer1_row.set(normalizer, & stat->m_timer1_stat);
  }

  /** Set a table field from the row. */
  inline void set_field(uint index, Field *f)
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
  inline void set(time_normalizer *normalizer, const PFS_transaction_stat *stat)
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

/** Row fragment for connection statistics. */
struct PFS_connection_stat_row
{
  ulonglong m_current_connections;
  ulonglong m_total_connections;

  inline void set(const PFS_connection_stat *stat)
  {
    m_current_connections= stat->m_current_connections;
    m_total_connections= stat->m_total_connections;
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

/** Row fragment for socket io statistics columns. */
struct PFS_socket_io_stat_row
{
  PFS_byte_stat_row m_read;
  PFS_byte_stat_row m_write;
  PFS_byte_stat_row m_misc;
  PFS_byte_stat_row m_all;

  inline void set(time_normalizer *normalizer, const PFS_socket_io_stat *stat)
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

/** Row fragment for file io statistics columns. */
struct PFS_file_io_stat_row
{
  PFS_byte_stat_row m_read;
  PFS_byte_stat_row m_write;
  PFS_byte_stat_row m_misc;
  PFS_byte_stat_row m_all;

  inline void set(time_normalizer *normalizer, const PFS_file_io_stat *stat)
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
  inline void set(const PFS_memory_stat *stat)
  {
    m_stat= *stat;
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

struct PFS_variable_name_row
{
public:
  PFS_variable_name_row()
  {
    m_str[0]= '\0';
    m_length= 0;
  }

  void make_row(const char* str, size_t length);

  char m_str[NAME_CHAR_LEN+1];
  uint m_length;
};

struct PFS_variable_value_row
{
public:
  /** Set the row from a status variable. */
  void make_row(const Status_variable *var);

  /** Set the row from a system variable. */
  void make_row(const System_variable *var);

  /** Set a table field from the row. */
  void set_field(Field *f);

private:
  void make_row(const CHARSET_INFO *cs, const char* str, size_t length);

  char m_str[1024];
  uint m_length;
  const CHARSET_INFO *m_charset;
};

struct PFS_user_variable_value_row
{
public:
  PFS_user_variable_value_row()
    : m_value(NULL), m_value_length(0)
  {}

  PFS_user_variable_value_row(const PFS_user_variable_value_row& rhs)
  {
    make_row(rhs.m_value, rhs.m_value_length);
  }

  ~PFS_user_variable_value_row()
  {
    clear();
  }

  void make_row(const char* val, size_t length);

  const char *get_value() const
  { return m_value; }

  size_t get_value_length() const
  { return m_value_length; }

  void clear();

private:
  char *m_value;
  size_t m_value_length;
};

/** @} */

#endif

