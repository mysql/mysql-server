/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

#ifndef PFS_TABLE_HELPER_H
#define PFS_TABLE_HELPER_H

/**
  @file storage/perfschema/table_helper.h
  Helpers to implement a performance schema table.
*/

#include <assert.h>
#include <stddef.h>
#include <sys/types.h>

#include "lex_string.h"

#include "my_inttypes.h"
#include "storage/perfschema/digest.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_digest.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_events.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_name.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/pfs_timer.h"

struct CHARSET_INFO;
struct PFS_host;
struct PFS_user;
struct PFS_account;
struct PFS_schema_name;
struct PFS_object_name;
struct PFS_routine_name;
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
class Json_wrapper;

/**
  @file storage/perfschema/table_helper.h
  Performance schema table helpers (declarations).
*/

/**
  @addtogroup performance_schema_tables
  @{
*/

/**
  Helper, assign a value to a @c tinyint field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_tiny(Field *f, long value);

/**
  Helper, assign a value to a @c unsigned tinyint field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_utiny(Field *f, ulong value);

/**
  Helper, read a value from an @c tinyint field.
  @param f the field to read
  @return the field value
*/
long get_field_tiny(Field *f);

ulong get_field_utiny(Field *f);

/**
  Helper, assign a value to a @c short field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_short(Field *f, long value);

/**
  Helper, assign a value to a @c unsigned short field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_ushort(Field *f, ulong value);

/**
  Helper, read a value from an @c smallint field.
  @param f the field to read
  @return the field value
*/
long get_field_short(Field *f);

ulong get_field_ushort(Field *f);

/**
  Helper, assign a value to a @c medium field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_medium(Field *f, long value);

/**
  Helper, assign a value to a @c unsigned medium field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_umedium(Field *f, ulong value);

/**
  Helper, read a value from an @c mediumint field.
  @param f the field to read
  @return the field value
*/
long get_field_medium(Field *f);

ulong get_field_umedium(Field *f);

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
  Helper, read a value from a @c long field.
  @param f the field to read
  @return the field value
*/
long get_field_long(Field *f);

ulong get_field_ulong(Field *f);

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

longlong get_field_longlong(Field *f);

/**
  Helper, read a value from an @c ulonglong field.
  @param f the field to read
  @return the field value
*/
ulonglong get_field_ulonglong(Field *f);

/**
  Helper, assign a value to a @c decimal field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_decimal(Field *f, double value);

/**
  Helper, read a value from a @c decimal field.
  @param f the field to read
  @return the field value
*/
double get_field_decimal(Field *f);

/**
  Helper, assign a value to a @c float field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_float(Field *f, double value);

/**
  Helper, read a value from a @c float field.
  @param f the field to read
  @return the field value
*/
double get_field_float(Field *f);

/**
  Helper, assign a value to a @c double field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_double(Field *f, double value);

/**
  Helper, read a value from a @c double field.
  @param f the field to read
  @return the field value
*/
double get_field_double(Field *f);

/**
  Helper, assign a value to a @code char utf8mb4 @endcode field.
  @param f the field to set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_char_utf8mb4(Field *f, const char *str, uint len);

/**
  Helper, read a value from a @code char utf8mb4 @endcode field.
  @param f the field to read
  @param[out] val the field value
  @param[out] len field value length
  @return the field value
*/
char *get_field_char_utf8mb4(Field *f, char *val, uint *len);

/**
  Helper, read a value from a @code char utf8mb4 @endcode field.
  @param f the field to read
  @param[out] val the field value
  @return the field value
*/
String *get_field_char_utf8mb4(Field *f, String *val);

/**
  Helper, assign a value to a @code varchar utf8mb4 @endcode field.
  @param f the field to set
  @param cs the string character set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_varchar(Field *f, const CHARSET_INFO *cs, const char *str,
                       uint len);

/**
  Helper, read a value from a @code varchar utf8mb4 @endcode field.
  @param f the field to read
  @param[out] val the field value
  @return the field value
*/
String *get_field_varchar_utf8mb4(Field *f, String *val);

/**
  Helper, read a value from a @code varchar utf8mb4 @endcode field.
  @param f the field to read
  @param[out] val the field value
  @param[out] len field value length
  @return the field value
*/
char *get_field_varchar_utf8mb4(Field *f, char *val, uint *len);

/**
  Helper, assign a value to a @code varchar utf8mb4 @endcode field.
  @param f the field to set
  @param str the string to assign
*/
void set_field_varchar_utf8mb4(Field *f, const char *str);

/**
  Helper, assign a value to a @code varchar utf8mb4 @endcode field.
  @param f the field to set
  @param str the string to assign
  @param len the length of the string to assign
*/
void set_field_varchar_utf8mb4(Field *f, const char *str, uint len);

/**
  Helper, assign a value to a text/blob field.
  @param f the field to set
  @param val the value to assign
  @param len the length of the string to assign
*/
void set_field_blob(Field *f, const char *val, size_t len);

/**
  Helper, assign a value to a text field.
  @param f the field to set
  @param val the value to assign
  @param len the length of the string to assign
  @param cs the charset of the string
*/
void set_field_text(Field *f, const char *val, size_t len,
                    const CHARSET_INFO *cs);
/**
  Helper, read a value from a @c blob field.
  @param f the field to read
  @param[out] val the field value
  @param[out] len field value length
  @return the field value
*/
char *get_field_blob(Field *f, char *val, uint *len);

/**
  Helper, assign a value to an @c enum field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_enum(Field *f, ulonglong value);

/**
  Helper, read a value from an @c enum field.
  @param f the field to read
  @return the field value
*/
ulonglong get_field_enum(Field *f);

/**
  Helper, assign a value to a @c set field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_set(Field *f, ulonglong value);

/**
  Helper, read a value from a @c set field.
  @param f the field to read
  @return the field value
*/
ulonglong get_field_set(Field *f);

/**
  Helper, assign a value to a @c date field.
  @param f the field to set
  @param value the value to assign
  @param len length of the value
*/
void set_field_date(Field *f, const char *value, uint len);

/**
  Helper, read a value from an @c date field.
  @param f the field to read
  @param[out] val the field value
  @param[out] len field value length
  @return the field value
*/
char *get_field_date(Field *f, char *val, uint *len);

/**
  Helper, assign a value to a @c time field.
  @param f the field to set
  @param value the value to assign
  @param len length of the value
*/
void set_field_time(Field *f, const char *value, uint len);

/**
  Helper, read a value from an @c time field.
  @param f the field to read
  @param[out] val the field value
  @param[out] len field value length
  @return the field value
*/
char *get_field_time(Field *f, char *val, uint *len);

/**
  Helper, assign a value to a @c datetime field.
  @param f the field to set
  @param value the value to assign
  @param len length of the value
*/
void set_field_datetime(Field *f, const char *value, uint len);

/**
  Helper, read a value from an @c datetime field.
  @param f the field to read
  @param[out] val the field value
  @param[out] len field value length
  @return the field value
*/
char *get_field_datetime(Field *f, char *val, uint *len);

/**
  Helper, assign a value to a @c timestamp field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_timestamp(Field *f, ulonglong value);

/**
  Helper, assign a value to a @c timestamp field.
  @param f the field to set
  @param value the value to assign
  @param len length of the value
*/
void set_field_timestamp(Field *f, const char *value, uint len);

/**
  Helper, read a value from an @c timestamp field.
  @param f the field to read
  @param[out] val the field value
  @param[out] len field value length
  @return the field value
*/
char *get_field_timestamp(Field *f, char *val, uint *len);

/**
  Helper, assign a value to a @c year field.
  @param f the field to set
  @param value the value to assign
*/
void set_field_year(Field *f, ulong value);

/**
  Helper, read a value from an @c year field.
  @param f the field to read
  @return the field value
*/
ulong get_field_year(Field *f);

/**
  Helper, assign a value to a JSON field.
  @param f the field to set
  @param json the value to assign
*/
void set_field_json(Field *f, const Json_wrapper *json);

void set_nullable_field_schema_name(Field *f, const PFS_schema_name *schema);
void set_field_schema_name(Field *f, const PFS_schema_name *schema);

void set_nullable_field_object_name(Field *f, const PFS_object_name *object);
void set_field_object_name(Field *f, const PFS_object_name *object);

void set_nullable_field_routine_name(Field *f, const PFS_routine_name *object);
void set_field_routine_name(Field *f, const PFS_routine_name *object);

/**
  Helper, format sql text for output.

  @param source_sqltext  raw sqltext, possibly truncated
  @param source_length  length of source_sqltext
  @param source_cs  character set of source_sqltext
  @param truncated true if source_sqltext was truncated
  @param sqltext sqltext formatted for output
 */
void format_sqltext(const char *source_sqltext, size_t source_length,
                    const CHARSET_INFO *source_cs, bool truncated,
                    String &sqltext);

/**
  Create a SOURCE column from source file and line.

  @param source_file     source file name pointer from __FILE__
  @param source_line     line number
  @param row_buffer      target string buffer
  @param row_buffer_size size of target buffer
  @param row_length      string length of combined source file and line
*/
void make_source_column(const char *source_file, size_t source_line,
                        char row_buffer[], size_t row_buffer_size,
                        uint &row_length);

/** Name space, internal views used within table setup_instruments. */
struct PFS_instrument_view_constants {
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

  /*
    THREAD are displayed in table setup_threads
    instead of setup_instruments.
  */

  static const uint VIEW_STAGE = 9;
  static const uint VIEW_STATEMENT = 10;
  static const uint VIEW_TRANSACTION = 11;
  static const uint VIEW_BUILTIN_MEMORY = 12;
  static const uint VIEW_MEMORY = 13;
  static const uint VIEW_ERROR = 14;

  static const uint LAST_INSTRUMENT = 14;
};

/** Name space, internal views used within object summaries. */
struct PFS_object_view_constants {
  static const uint FIRST_VIEW = 1;
  static const uint VIEW_TABLE = 1;
  static const uint VIEW_PROGRAM = 2;
  static const uint LAST_VIEW = 2;
};

/** Row fragment for column HOST. */
struct PFS_host_row {
  /** Column HOST. */
  PFS_host_name m_host_name;

  /** Build a row from a memory buffer. */
  int make_row(PFS_host *pfs);
  /** Set a table field from the row. */
  void set_field(Field *f);
  void set_nullable_field(Field *f);
};

/** Row fragment for column USER. */
struct PFS_user_row {
  /** Column USER. */
  PFS_user_name m_user_name;

  /** Build a row from a memory buffer. */
  int make_row(PFS_user *pfs);
  /** Set a table field from the row. */
  void set_field(Field *f);
  void set_nullable_field(Field *f);
};

/** Row fragment for columns USER, HOST. */
struct PFS_account_row {
  /** Column USER. */
  PFS_user_name m_user_name;
  /** Column HOST. */
  PFS_host_name m_host_name;

  /** Build a row from a memory buffer. */
  int make_row(PFS_account *pfs);
  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
  void set_nullable_field(uint index, Field *f);
};

/** Row fragment for columns DIGEST, DIGEST_TEXT. */
struct PFS_digest_row {
  /** Column SCHEMA_NAME. */
  PFS_schema_name m_schema_name;
  /** Column DIGEST. */
  char m_digest[DIGEST_HASH_TO_STRING_LENGTH + 1];
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
struct PFS_event_name_row {
  /** Column EVENT_NAME. */
  const char *m_name;
  /** Length in bytes of @c m_name. */
  uint m_name_length;

  /** Build a row from a memory buffer. */
  inline int make_row(PFS_instr_class *pfs) {
    m_name = pfs->m_name.str();
    m_name_length = pfs->m_name.length();
    return 0;
  }

  /** Set a table field from the row. */
  inline void set_field(Field *f) {
    set_field_varchar_utf8mb4(f, m_name, m_name_length);
  }
};

/** Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME. */
struct PFS_object_row {
  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column SCHEMA_NAME. */
  PFS_schema_name m_schema_name;
  /** Column OBJECT_NAME. */
  PFS_object_name m_object_name;

  /** Build a row from a memory buffer. */
  int make_row(PFS_table_share *pfs);
  int make_row(PFS_program *pfs);
  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
  void set_nullable_field(uint index, Field *f);
};

/** Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME. */
struct PFS_object_view_row {
  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column SCHEMA_NAME. */
  PFS_schema_name_view m_schema_name;
  /** Column OBJECT_NAME. */
  PFS_object_name_view m_object_name;

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
  void set_nullable_field(uint index, Field *f);
};

/** Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME, COLUMN_NAME.
 */
struct PFS_column_row {
  /** Column OBJECT_TYPE. */
  enum_object_type m_object_type;
  /** Column SCHEMA_NAME. */
  char m_schema_name[NAME_LEN];
  /** Length in bytes of @c m_schema_name. */
  size_t m_schema_name_length;
  /** Column OBJECT_NAME. */
  char m_object_name[NAME_LEN];
  /** Length in bytes of @c m_object_name. */
  size_t m_object_name_length;
  /** Column OBJECT_NAME. */
  char m_column_name[NAME_LEN];
  /** Length in bytes of @c m_column_name. */
  size_t m_column_name_length;

  /** Build a row from a memory buffer. */
  int make_row(const MDL_key *mdl);
  /** Set a table field from the row. */
  void set_nullable_field(uint index, Field *f);
};

/**
  Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME, INDEX_NAME.
 */
struct PFS_index_row {
  PFS_object_row m_object_row;
  /** Column INDEX_NAME. */
  char m_index_name[NAME_LEN];
  /** Length in bytes of @c m_index_name. */
  size_t m_index_name_length;

  /** Build a row from a memory buffer. */
  int make_index_name(PFS_table_share_index *pfs_index, uint table_index);
  int make_row(PFS_table_share *pfs, PFS_table_share_index *pfs_index,
               uint table_index);
  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
  void set_nullable_field(uint index, Field *f);
};

/**
  Row fragment for columns OBJECT_TYPE, SCHEMA_NAME, OBJECT_NAME, INDEX_NAME.
 */
struct PFS_index_view_row {
  PFS_object_view_row m_object_row;
  PFS_index_name_view m_index_name;

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
  void set_nullable_field(uint index, Field *f);
};

/** Row fragment for single statistics columns (COUNT, SUM, MIN, AVG, MAX) */
struct PFS_stat_row {
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

  inline void reset() {
    m_count = 0;
    m_sum = 0;
    m_min = 0;
    m_avg = 0;
    m_max = 0;
  }

  /** Build a row with timer fields from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_single_stat *stat) {
    m_count = stat->m_count;

    if ((m_count != 0) && stat->has_timed_stats()) {
      m_sum = normalizer->wait_to_pico(stat->m_sum);
      m_min = normalizer->wait_to_pico(stat->m_min);
      m_max = normalizer->wait_to_pico(stat->m_max);
      m_avg = normalizer->wait_to_pico(stat->m_sum / m_count);
    } else {
      m_sum = 0;
      m_min = 0;
      m_avg = 0;
      m_max = 0;
    }
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f) {
    switch (index) {
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
        assert(false);
    }
  }
};

/** Row fragment for timer and byte count stats. Corresponds to PFS_byte_stat */
struct PFS_byte_stat_row {
  PFS_stat_row m_waits;
  ulonglong m_bytes;

  /** Build a row with timer and byte count fields from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_byte_stat *stat) {
    m_waits.set(normalizer, stat);
    m_bytes = stat->m_bytes;
  }
};

/** Row fragment for table I/O statistics columns. */
struct PFS_table_io_stat_row {
  PFS_stat_row m_all;
  PFS_stat_row m_all_read;
  PFS_stat_row m_all_write;
  PFS_stat_row m_fetch;
  PFS_stat_row m_insert;
  PFS_stat_row m_update;
  PFS_stat_row m_delete;

  /** Build a row from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_table_io_stat *stat) {
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
struct PFS_table_lock_stat_row {
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
  inline void set(time_normalizer *normalizer,
                  const PFS_table_lock_stat *stat) {
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
struct PFS_stage_stat_row {
  PFS_stat_row m_timer1_row;

  /** Build a row from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_stage_stat *stat) {
    m_timer1_row.set(normalizer, &stat->m_timer1_stat);
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f) { m_timer1_row.set_field(index, f); }
};

/** Row fragment for statement statistics columns. */
struct PFS_statement_stat_row {
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
  /**
    CPU TIME.
    Expressed in DISPLAY units (picoseconds).
  */
  ulonglong m_cpu_time;
  ulonglong m_max_controlled_memory;
  ulonglong m_max_total_memory;
  ulonglong m_count_secondary;

  /** Build a row from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_statement_stat *stat) {
    if (stat->m_timer1_stat.m_count != 0) {
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
      m_cpu_time = stat->m_cpu_time * NANOSEC_TO_PICOSEC;
      m_max_controlled_memory = stat->m_max_controlled_memory;
      m_max_total_memory = stat->m_max_total_memory;
      m_count_secondary = stat->m_count_secondary;
    } else {
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
      m_cpu_time = 0;
      m_max_controlled_memory = 0;
      m_max_total_memory = 0;
      m_count_secondary = 0;
    }
  }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

/** Row fragment for stored program statistics. */
struct PFS_sp_stat_row {
  PFS_stat_row m_timer1_row;

  /** Build a row from a memory buffer. */
  inline void set(time_normalizer *normalizer, const PFS_sp_stat *stat) {
    m_timer1_row.set(normalizer, &stat->m_timer1_stat);
  }

  /** Set a table field from the row. */
  inline void set_field(uint index, Field *f) {
    m_timer1_row.set_field(index, f);
  }
};

/** Row fragment for transaction statistics columns. */
struct PFS_transaction_stat_row {
  PFS_stat_row m_timer1_row;
  PFS_stat_row m_read_write_row;
  PFS_stat_row m_read_only_row;
  ulonglong m_savepoint_count;
  ulonglong m_rollback_to_savepoint_count;
  ulonglong m_release_savepoint_count;

  /** Build a row from a memory buffer. */
  inline void set(time_normalizer *normalizer,
                  const PFS_transaction_stat *stat) {
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
struct PFS_error_stat_row {
  ulonglong m_count;
  ulonglong m_handled_count;
  uint m_error_index;
  ulonglong m_first_seen;
  ulonglong m_last_seen;

  /** Build a row from a memory buffer. */
  inline void set(const PFS_error_single_stat *stat, uint error_index) {
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
struct PFS_connection_stat_row {
  PFS_connection_stat m_stat;

  inline void set(const PFS_connection_stat *stat) { m_stat = *stat; }

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
struct PFS_socket_io_stat_row {
  PFS_byte_stat_row m_read;
  PFS_byte_stat_row m_write;
  PFS_byte_stat_row m_misc;
  PFS_byte_stat_row m_all;

  inline void set(time_normalizer *normalizer, const PFS_socket_io_stat *stat) {
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
struct PFS_file_io_stat_row {
  PFS_byte_stat_row m_read;
  PFS_byte_stat_row m_write;
  PFS_byte_stat_row m_misc;
  PFS_byte_stat_row m_all;

  inline void set(time_normalizer *normalizer, const PFS_file_io_stat *stat) {
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
struct PFS_memory_stat_row {
  PFS_memory_monitoring_stat m_stat;

  /** Build a row from a memory buffer. */
  inline void set(const PFS_memory_monitoring_stat *stat) { m_stat = *stat; }

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

struct PFS_session_all_memory_stat_row {
  size_t m_controlled_size;
  size_t m_max_controlled_size;
  size_t m_total_size;
  size_t m_max_total_size;

  /** Build a row from a memory buffer. */
  void set(const PFS_session_all_memory_stat *stat);

  /** Set a table field from the row. */
  void set_field(uint index, Field *f);
};

struct PFS_variable_name_row {
 public:
  PFS_variable_name_row() {
    m_str[0] = '\0';
    m_length = 0;
  }

  int make_row(const char *str, size_t length);

  char m_str[NAME_CHAR_LEN + 1];
  uint m_length;
};

struct PFS_variable_value_row {
 public:
  /** Set the row from a status variable. */
  int make_row(const Status_variable *var);

  /** Set the row from a system variable. */
  int make_row(const System_variable *var);

  /** Set a table field from the row. */
  void set_field(Field *f);

  const char *get_str() const { return m_str; }
  uint get_length() const { return m_length; }

 private:
  int make_row(const CHARSET_INFO *cs, const char *str, size_t length);

  char m_str[1024];
  uint m_length;
  const CHARSET_INFO *m_charset;
};

struct PFS_user_variable_value_row {
 public:
  PFS_user_variable_value_row() : m_value(nullptr), m_value_length(0) {}

  PFS_user_variable_value_row(const PFS_user_variable_value_row &rhs) {
    make_row(rhs.m_value, rhs.m_value_length);
  }

  ~PFS_user_variable_value_row() { clear(); }

  int make_row(const char *val, size_t length);

  const char *get_value() const { return m_value; }

  size_t get_value_length() const { return m_value_length; }

  void clear();

 private:
  char *m_value;
  size_t m_value_length;
};

class PFS_key_long : public PFS_engine_key {
 public:
  explicit PFS_key_long(const char *name)
      : PFS_engine_key(name), m_key_value(0) {}

  ~PFS_key_long() override = default;

  static enum ha_rkey_function stateless_read(PFS_key_reader &reader,
                                              enum ha_rkey_function find_flag,
                                              bool &is_null, long *key_value) {
    return reader.read_long(find_flag, is_null, key_value);
  }

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override {
    m_find_flag = stateless_read(reader, find_flag, m_is_null, &m_key_value);
  }

  static bool stateless_match(bool record_null, long record_value,
                              bool m_is_null, long m_key_value,
                              enum ha_rkey_function find_flag);

 protected:
  bool do_match(bool record_null, long record_value) {
    return stateless_match(record_null, record_value, m_is_null, m_key_value,
                           m_find_flag);
  }

 private:
  long m_key_value;
};

class PFS_key_ulong : public PFS_engine_key {
 public:
  explicit PFS_key_ulong(const char *name)
      : PFS_engine_key(name), m_key_value(0) {}

  ~PFS_key_ulong() override = default;

  static enum ha_rkey_function stateless_read(PFS_key_reader &reader,
                                              enum ha_rkey_function find_flag,
                                              bool &is_null, ulong *key_value) {
    return reader.read_ulong(find_flag, is_null, key_value);
  }

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override {
    m_find_flag = reader.read_ulong(find_flag, m_is_null, &m_key_value);
  }

  static bool stateless_match(bool record_null, ulong record_value,
                              bool m_is_null, ulong m_key_value,
                              enum ha_rkey_function find_flag);

 protected:
  bool do_match(bool record_null, ulong record_value);

 private:
  ulong m_key_value;
};

class PFS_key_longlong : public PFS_engine_key {
 public:
  explicit PFS_key_longlong(const char *name)
      : PFS_engine_key(name), m_key_value(0) {}

  ~PFS_key_longlong() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override {
    m_find_flag = reader.read_longlong(find_flag, m_is_null, &m_key_value);
  }

  static bool stateless_match(bool record_null, longlong record_value,
                              bool m_is_null, longlong m_key_value,
                              enum ha_rkey_function find_flag);

 protected:
  bool do_match(bool record_null, longlong record_value) {
    return stateless_match(record_null, record_value, m_is_null, m_key_value,
                           m_find_flag);
  }

 private:
  longlong m_key_value;
};

class PFS_key_ulonglong : public PFS_engine_key {
 public:
  explicit PFS_key_ulonglong(const char *name)
      : PFS_engine_key(name), m_key_value(0) {}

  ~PFS_key_ulonglong() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override {
    m_find_flag = reader.read_ulonglong(find_flag, m_is_null, &m_key_value);
  }

  static bool stateless_match(bool record_null, ulonglong record_value,
                              bool m_is_null, ulonglong m_key_value,
                              enum ha_rkey_function find_flag);

 protected:
  bool do_match(bool record_null, ulonglong record_value);

 private:
  ulonglong m_key_value;
};

class PFS_key_thread_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_thread_id(const char *name) : PFS_key_ulonglong(name) {}

  ~PFS_key_thread_id() override = default;

  bool match(ulonglong thread_id);
  bool match(const PFS_thread *pfs);
  bool match_owner(const PFS_table *pfs);
  bool match_owner(const PFS_socket *pfs);
  bool match_owner(const PFS_mutex *pfs);
  bool match_owner(const PFS_prepared_stmt *pfs);
  bool match_owner(const PFS_metadata_lock *pfs);
  bool match_writer(const PFS_rwlock *pfs);
};

class PFS_key_event_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_event_id(const char *name) : PFS_key_ulonglong(name) {}

  ~PFS_key_event_id() override = default;

  bool match(ulonglong event_id);
  bool match(const PFS_events *pfs);
  bool match(const PFS_events_waits *pfs);
  bool match_owner(const PFS_table *pfs);
  bool match_owner(const PFS_prepared_stmt *pfs);
  bool match_owner(const PFS_metadata_lock *pfs);
};

class PFS_key_processlist_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_processlist_id(const char *name) : PFS_key_ulonglong(name) {}

  ~PFS_key_processlist_id() override = default;

  bool match(const PFS_thread *pfs);
};

class PFS_key_engine_transaction_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_engine_transaction_id(const char *name)
      : PFS_key_ulonglong(name) {}

  ~PFS_key_engine_transaction_id() override = default;

  bool match(ulonglong engine_transaction_id);
};

class PFS_key_thread_os_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_thread_os_id(const char *name) : PFS_key_ulonglong(name) {}

  ~PFS_key_thread_os_id() override = default;

  bool match(const PFS_thread *pfs);
};

class PFS_key_statement_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_statement_id(const char *name) : PFS_key_ulonglong(name) {}

  ~PFS_key_statement_id() override = default;

  bool match(const PFS_prepared_stmt *pfs);
};

class PFS_key_worker_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_worker_id(const char *name) : PFS_key_ulonglong(name) {}

  ~PFS_key_worker_id() override = default;

  bool match_not_null(ulonglong worker_id);
};

class PFS_key_socket_id : public PFS_key_long {
 public:
  explicit PFS_key_socket_id(const char *name) : PFS_key_long(name) {}

  ~PFS_key_socket_id() override = default;

  bool match(const PFS_socket *pfs);
};

class PFS_key_port : public PFS_key_long {
 public:
  explicit PFS_key_port(const char *name) : PFS_key_long(name) {}

  ~PFS_key_port() override = default;

  bool match(const PFS_socket *pfs);

  /**
    match port number

    @param port   port number to match
  */
  bool match(uint port);
};

class PFS_key_error_number : public PFS_key_long {
 public:
  explicit PFS_key_error_number(const char *name) : PFS_key_long(name) {}

  ~PFS_key_error_number() override = default;

  bool match_error_index(uint error_index);
};

class PFS_key_pstring : public PFS_engine_key {
 public:
  explicit PFS_key_pstring(const char *name) : PFS_engine_key(name) {}

  ~PFS_key_pstring() override = default;

  static enum ha_rkey_function stateless_read(PFS_key_reader &reader,
                                              enum ha_rkey_function find_flag,
                                              bool &is_null, char *key_value,
                                              uint *key_value_length,
                                              uint key_value_max_length) {
    if (reader.get_key_type() == HA_KEYTYPE_TEXT) {
      return (reader.read_text_utf8(find_flag, is_null, key_value,
                                    key_value_length, key_value_max_length));
    }
    return (reader.read_varchar_utf8(find_flag, is_null, key_value,
                                     key_value_length, key_value_max_length));
  }

  static bool stateless_match(bool record_null, const char *record_string,
                              size_t record_string_length,
                              const char *m_key_value,
                              size_t m_key_value_length, bool m_is_null,
                              enum ha_rkey_function m_find_flag);

 protected:
  bool do_match(bool record_null, const char *record_value,
                size_t record_value_length);
  bool do_match_prefix(bool record_null, const char *record_value,
                       size_t record_value_length);
};

template <int SIZE>
class PFS_key_string : public PFS_key_pstring {
 public:
  explicit PFS_key_string(const char *name)
      : PFS_key_pstring(name), m_key_value_length(0) {}

  ~PFS_key_string() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override {
    m_find_flag = stateless_read(reader, find_flag, m_is_null, m_key_value,
                                 &m_key_value_length, sizeof(m_key_value));
  }

 protected:
  bool do_match(bool record_null, const char *record_value,
                size_t record_value_length) {
    return stateless_match(record_null, record_value, record_value_length,
                           m_key_value, m_key_value_length, m_is_null,
                           m_find_flag);
  }
  bool do_match_prefix(bool record_null, const char *record_string,
                       size_t record_string_length);

 private:
  char m_key_value[SIZE * FILENAME_CHARSET_MBMAXLEN];
  uint m_key_value_length;
};

class PFS_key_thread_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH> {
 public:
  explicit PFS_key_thread_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_thread_name() override = default;

  bool match(const PFS_thread *pfs);
  bool match(const PFS_thread_class *klass);
};

class PFS_key_event_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH> {
 public:
  explicit PFS_key_event_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_event_name() override = default;

  bool match(const PFS_instr_class *pfs);
  bool match(const PFS_mutex *pfs);
  bool match(const PFS_rwlock *pfs);
  bool match(const PFS_cond *pfs);
  bool match(const PFS_file *pfs);
  bool match(const PFS_socket *pfs);
  bool match_view(uint view);
};

class PFS_key_meter_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH> {
 public:
  explicit PFS_key_meter_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_meter_name() override = default;

  bool match(PFS_meter_class *pfs);
};

class PFS_key_metric_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH> {
 public:
  explicit PFS_key_metric_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_metric_name() override = default;

  bool match(PFS_metric_class *pfs);
};

class PFS_key_user : public PFS_key_string<USERNAME_LENGTH> {
 public:
  explicit PFS_key_user(const char *name) : PFS_key_string(name) {}

  ~PFS_key_user() override = default;

  bool match(const PFS_thread *pfs);
  bool match(const PFS_user *pfs);
  bool match(const PFS_account *pfs);
  bool match(const PFS_setup_actor *pfs);
};

class PFS_key_host : public PFS_key_string<HOSTNAME_LENGTH> {
 public:
  explicit PFS_key_host(const char *name) : PFS_key_string(name) {}

  ~PFS_key_host() override = default;

  bool match(const PFS_thread *pfs);
  bool match(const PFS_host *pfs);
  bool match(const PFS_account *pfs);
  bool match(const PFS_setup_actor *pfs);
  bool match(const char *host, size_t hostname_length);
};

class PFS_key_role : public PFS_key_string<ROLENAME_LENGTH> {
 public:
  explicit PFS_key_role(const char *name) : PFS_key_string(name) {}

  ~PFS_key_role() override = default;

  bool match(const PFS_setup_actor *pfs);
};

class PFS_key_schema : public PFS_key_string<NAME_CHAR_LEN> {
 public:
  explicit PFS_key_schema(const char *schema) : PFS_key_string(schema) {}

  ~PFS_key_schema() override = default;

  bool match(const PFS_statements_digest_stat *pfs);
};

class PFS_key_digest : public PFS_key_string<MAX_KEY_LENGTH> {
 public:
  explicit PFS_key_digest(const char *digest) : PFS_key_string(digest) {}

  ~PFS_key_digest() override = default;

  bool match(PFS_statements_digest_stat *pfs);
};

class PFS_key_bucket_number : public PFS_key_ulong {
 public:
  explicit PFS_key_bucket_number(const char *name) : PFS_key_ulong(name) {}

  ~PFS_key_bucket_number() override = default;

  bool match(ulong value);
};

/* Generic NAME key */
class PFS_key_name : public PFS_key_string<NAME_CHAR_LEN> {
 public:
  explicit PFS_key_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_name() override = default;

  bool match(const LEX_CSTRING *name);
  bool match(const char *name, size_t name_length);
  bool match_not_null(const LEX_STRING *name);
  bool match_not_null(const char *name, size_t name_length);
};

class PFS_key_group_name : public PFS_key_string<NAME_CHAR_LEN> {
 public:
  explicit PFS_key_group_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_group_name() override = default;

  bool match(const LEX_STRING *name);
  bool match(const char *name, size_t name_length);
  bool match(PFS_thread *pfs);
};

class PFS_key_variable_name : public PFS_key_string<NAME_CHAR_LEN> {
 public:
  explicit PFS_key_variable_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_variable_name() override = default;

  bool match(const System_variable *pfs);
  bool match(const Status_variable *pfs);
  bool match(const PFS_variable_name_row *pfs);
};

// FIXME: 32
class PFS_key_engine_name : public PFS_key_string<32> {
 public:
  explicit PFS_key_engine_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_engine_name() override = default;

  bool match(const char *engine_name, size_t length);
};

// FIXME: 128
class PFS_key_engine_lock_id : public PFS_key_string<128> {
 public:
  explicit PFS_key_engine_lock_id(const char *name) : PFS_key_string(name) {}

  ~PFS_key_engine_lock_id() override = default;

  bool match(const char *engine_lock_id, size_t length);
};

class PFS_key_ip : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH>  // FIXME
// <INET6_ADDRSTRLEN+1>
// fails on freebsd
{
 public:
  explicit PFS_key_ip(const char *name) : PFS_key_string(name) {}

  ~PFS_key_ip() override = default;

  bool match(const PFS_socket *pfs);
  bool match(const char *ip, size_t ip_length);
};

class PFS_key_statement_name : public PFS_key_string<PFS_MAX_INFO_NAME_LENGTH> {
 public:
  explicit PFS_key_statement_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_statement_name() override = default;

  bool match(const PFS_prepared_stmt *pfs);
};

class PFS_key_file_name
    : public PFS_key_string<1350>  // FIXME FN_REFLEN or FN_REFLEN_SE
{
 public:
  explicit PFS_key_file_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_file_name() override = default;

  bool match(const PFS_file *pfs);
};

class PFS_key_object_schema : public PFS_key_string<NAME_CHAR_LEN> {
 public:
  explicit PFS_key_object_schema(const char *name) : PFS_key_string(name) {}

  ~PFS_key_object_schema() override = default;

  bool match(const PFS_table_share *share);
  bool match(const PFS_program *pfs);
  bool match(const PFS_prepared_stmt *pfs);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_column_row *pfs);
  bool match(const PFS_setup_object *pfs);
  bool match(const char *schema_name, size_t schema_name_length);
};

class PFS_key_object_name : public PFS_key_string<NAME_CHAR_LEN> {
 public:
  explicit PFS_key_object_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_object_name() override = default;

  bool match(const PFS_table_share *share);
  bool match(const PFS_program *pfs);
  bool match(const PFS_prepared_stmt *pfs);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_column_row *pfs);
  bool match(const PFS_index_row *pfs);
  bool match(const PFS_setup_object *pfs);
  bool match(const char *object_name, size_t object_name_length);
};

class PFS_key_column_name : public PFS_key_string<NAME_CHAR_LEN> {
 public:
  explicit PFS_key_column_name(const char *name) : PFS_key_string(name) {}

  ~PFS_key_column_name() override = default;

  bool match(const PFS_column_row *pfs);
};

class PFS_key_object_type : public PFS_engine_key {
 public:
  explicit PFS_key_object_type(const char *name)
      : PFS_engine_key(name), m_object_type(NO_OBJECT_TYPE) {}

  ~PFS_key_object_type() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override;

  bool match(enum_object_type object_type);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_column_row *pfs);
  bool match(const PFS_program *pfs);

 private:
  bool do_match(bool record_null, enum_object_type record_value);
  enum_object_type m_object_type;
};

class PFS_key_object_type_enum : public PFS_engine_key {
 public:
  explicit PFS_key_object_type_enum(const char *name)
      : PFS_engine_key(name), m_object_type(NO_OBJECT_TYPE) {}

  ~PFS_key_object_type_enum() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override;

  bool match(enum_object_type object_type);
  bool match(const PFS_prepared_stmt *pfs);
  bool match(const PFS_object_row *pfs);
  bool match(const PFS_program *pfs);

 private:
  bool do_match(bool record_null, enum_object_type record_value);
  enum_object_type m_object_type;
};

class PFS_key_object_instance : public PFS_engine_key {
 public:
  explicit PFS_key_object_instance(const char *name)
      : PFS_engine_key(name), m_identity(nullptr) {}

  ~PFS_key_object_instance() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override {
    ulonglong object_instance_begin{0};
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
