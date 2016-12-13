/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_ENGINE_TABLE_H
#define PFS_ENGINE_TABLE_H

#include "auth_common.h"                            /* struct ACL_* */
/**
  @file storage/perfschema/pfs_engine_table.h
  Performance schema tables (declarations).
*/

#include "pfs_instr_class.h"
extern thread_local_key_t THR_PFS_VG;   // global_variables
extern thread_local_key_t THR_PFS_SV;   // session_variables
extern thread_local_key_t THR_PFS_VBT;  // variables_by_thread
extern thread_local_key_t THR_PFS_SG;   // global_status
extern thread_local_key_t THR_PFS_SS;   // session_status
extern thread_local_key_t THR_PFS_SBT;  // status_by_thread
extern thread_local_key_t THR_PFS_SBU;  // status_by_user
extern thread_local_key_t THR_PFS_SBH;  // status_by_host
extern thread_local_key_t THR_PFS_SBA;  // status_by_account

class Field;
struct PFS_engine_table_share;
struct time_normalizer;

/**
  @addtogroup Performance_schema_engine
  @{
*/

/**
  Store and retrieve table state information during a query.
*/
class PFS_table_context
{
public:
  PFS_table_context(ulonglong current_version, bool restore, thread_local_key_t key);
  PFS_table_context(ulonglong current_version, ulong map_size, bool restore, thread_local_key_t key);
~PFS_table_context(void);

  bool initialize(void);
  bool is_initialized(void) { return m_initialized; }
  ulonglong current_version(void) { return m_current_version; }
  ulonglong last_version(void) { return m_last_version; }
  bool versions_match(void) { return m_last_version == m_current_version; }
  void set_item(ulong n);
  bool is_item_set(ulong n);
  thread_local_key_t m_thr_key;

private:
  ulonglong m_current_version;
  ulonglong m_last_version;
  ulong *m_map;
  ulong m_map_size;
  ulong m_word_size;
  bool m_restore;
  bool m_initialized;
  ulong m_last_item;
};

/**
  An abstract PERFORMANCE_SCHEMA table.
  Every table implemented in the performance schema schema and storage engine
  derives from this class.
*/
class PFS_engine_table
{
public:
  static const PFS_engine_table_share*
    find_engine_table_share(const char *name);

  int read_row(TABLE *table, unsigned char *buf, Field **fields);

  int update_row(TABLE *table, const unsigned char *old_buf,
                 unsigned char *new_buf, Field **fields);

  /**
    Delete a row from this table.
    @param table Table handle
    @param buf the row buffer to delete
    @param fields Table fields
    @return 0 on success
  */
  int delete_row(TABLE *table, const unsigned char *buf, Field **fields);

  /** Initialize table scan. */
  virtual int rnd_init(bool scan){return 0;};

  /** Fetch the next row in this cursor. */
  virtual int rnd_next(void)= 0;
  /**
    Fetch a row by position.
    @param pos              position to fetch
  */
  virtual int rnd_pos(const void *pos)= 0;

  void get_position(void *ref);
  void set_position(const void *ref);
  /** Reset the cursor position to the beginning of the table. */
  virtual void reset_position(void)= 0;

  /** Get the normalizer and class type for the current row. */
  void get_normalizer(PFS_instr_class *instr_class);

  /** Destructor. */
  virtual ~PFS_engine_table()
  {}

  /**
    Helper, assign a value to a long field.
    @param f the field to set
    @param value the value to assign
  */
  static void set_field_long(Field *f, long value);
  /**
    Helper, assign a value to a ulong field.
    @param f the field to set
    @param value the value to assign
  */
  static void set_field_ulong(Field *f, ulong value);
  /**
    Helper, assign a value to a longlong field.
    @param f the field to set
    @param value the value to assign
  */
  static void set_field_longlong(Field *f, longlong value);
  /**
    Helper, assign a value to a ulonglong field.
    @param f the field to set
    @param value the value to assign
  */
  static void set_field_ulonglong(Field *f, ulonglong value);
  /**
    Helper, assign a value to a char utf8 field.
    @param f the field to set
    @param str the string to assign
    @param len the length of the string to assign
  */
  static void set_field_char_utf8(Field *f, const char *str, uint len);
  /**
    Helper, assign a value to a varchar utf8 field.
    @param f the field to set
    @param cs the string character set
    @param str the string to assign
    @param len the length of the string to assign
  */
  static void set_field_varchar(Field *f, const CHARSET_INFO *cs, const char *str, uint len);
  /**
    Helper, assign a value to a varchar utf8 field.
    @param f the field to set
    @param str the string to assign
    @param len the length of the string to assign
  */
  static void set_field_varchar_utf8(Field *f, const char *str, uint len);
  /**
    Helper, assign a value to a longtext utf8 field.
    @param f the field to set
    @param str the string to assign
    @param len the length of the string to assign
  */
  static void set_field_longtext_utf8(Field *f, const char *str, uint len);
  /**
    Helper, assign a value to a blob field.
    @param f the field to set
    @param val the value to assign
    @param len the length of the string to assign
  */
  static void set_field_blob(Field *f, const char *val, uint len);
  /**
    Helper, assign a value to an enum field.
    @param f the field to set
    @param value the value to assign
  */
  static void set_field_enum(Field *f, ulonglong value);
  /**
    Helper, assign a value to a timestamp field.
    @param f the field to set
    @param value the value to assign
  */
  static void set_field_timestamp(Field *f, ulonglong value);
  /**
    Helper, assign a value to a double field.
    @param f the field to set
    @param value the value to assign
  */
  static void set_field_double(Field *f, double value);
  /**
    Helper, read a value from an enum field.
    @param f the field to read
    @return the field value
  */
  static ulonglong get_field_enum(Field *f);
  /**
    Helper, read a value from a char utf8 field.
    @param f the field to read
    @param[out] val the field value
    @return the field value
  */
  static String *get_field_char_utf8(Field *f, String *val);
  /**
    Helper, read a value from a varchar utf8 field.
    @param f the field to read
    @param[out] val the field value
    @return the field value
  */
  static String *get_field_varchar_utf8(Field *f, String *val);

protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */
  virtual int read_row_values(TABLE *table, unsigned char *buf,
                              Field **fields, bool read_all)= 0;

  /**
    Update the current row values.
    @param table            Table handle
    @param old_buf          old row buffer
    @param new_buf          new row buffer
    @param fields           Table fields
  */
  virtual int update_row_values(TABLE *table, const unsigned char *old_buf,
                                unsigned char *new_buf, Field **fields);

  /**
    Delete a row.
    @param table            Table handle
    @param buf              Row buffer
    @param fields           Table fields
  */
  virtual int delete_row_values(TABLE *table, const unsigned char *buf,
                                Field **fields);
  /**
    Constructor.
    @param share            table share
    @param pos              address of the m_pos position member
  */
  PFS_engine_table(const PFS_engine_table_share *share, void *pos)
    : m_share_ptr(share), m_pos_ptr(pos),
      m_normalizer(NULL), m_class_type(PFS_CLASS_NONE)
  {}

  /** Table share. */
  const PFS_engine_table_share *m_share_ptr;
  /** Opaque pointer to the m_pos position of this cursor. */
  void *m_pos_ptr;
  /** Current normalizer */
  time_normalizer *m_normalizer;
  /** Current class type */
  enum PFS_class_type m_class_type;
};

/** Callback to open a table. */
typedef PFS_engine_table* (*pfs_open_table_t)(void);
/** Callback to write a row. */
typedef int (*pfs_write_row_t)(TABLE *table,
                               unsigned char *buf, Field **fields);
/** Callback to delete all rows. */
typedef int (*pfs_delete_all_rows_t)(void);
/** Callback to get a row count. */
typedef ha_rows (*pfs_get_row_count_t)(void);

/**
  A PERFORMANCE_SCHEMA table share.
  This data is shared by all the table handles opened on the same table.
*/
struct PFS_engine_table_share
{
  static void check_all_tables(THD *thd);
  void check_one_table(THD *thd);
  static void init_all_locks(void);
  static void delete_all_locks(void);
  /** Get the row count. */
  ha_rows get_row_count(void) const;
  /** Write a row. */
  int write_row(TABLE *table, unsigned char *buf, Field **fields) const;

  /** Table name. */
  LEX_STRING m_name;
  /** Table ACL. */
  const ACL_internal_table_access *m_acl;
  /** Open table function. */
  pfs_open_table_t m_open_table;
  /** Write row function. */
  pfs_write_row_t m_write_row;
  /** Delete all rows function. */
  pfs_delete_all_rows_t m_delete_all_rows;
  /** Get rows count function. */
  pfs_get_row_count_t m_get_row_count;
  /** Length of the m_pos position structure. */
  uint m_ref_length;
  /** The lock, stored on behalf of the SQL layer. */
  THR_LOCK *m_thr_lock_ptr;
  /** Table fields definition. */
  TABLE_FIELD_DEF *m_field_def;
  /** Schema integrity flag. */
  bool m_checked;
  /** Table is available even if the Performance Schema is disabled. */
  bool m_perpetual;
};

/**
  Privileges for read only tables.
  The only operation allowed is SELECT.
*/
class PFS_readonly_acl : public ACL_internal_table_access
{
public:
  PFS_readonly_acl()
  {}

  ~PFS_readonly_acl()
  {}

  virtual ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

/** Singleton instance of PFS_readonly_acl. */
extern PFS_readonly_acl pfs_readonly_acl;

/**
  Privileges for truncatable tables.
  Operations allowed are SELECT and TRUNCATE.
*/
class PFS_truncatable_acl : public ACL_internal_table_access
{
public:
  PFS_truncatable_acl()
  {}

  ~PFS_truncatable_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

/** Singleton instance of PFS_truncatable_acl. */
extern PFS_truncatable_acl pfs_truncatable_acl;

/**
  Privileges for updatable tables.
  Operations allowed are SELECT and UPDATE.
*/
class PFS_updatable_acl : public ACL_internal_table_access
{
public:
  PFS_updatable_acl()
  {}

  ~PFS_updatable_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

/** Singleton instance of PFS_updatable_acl. */
extern PFS_updatable_acl pfs_updatable_acl;

/**
  Privileges for editable tables.
  Operations allowed are SELECT, INSERT, UPDATE, DELETE and TRUNCATE.
*/
class PFS_editable_acl : public ACL_internal_table_access
{
public:
  PFS_editable_acl()
  {}

  ~PFS_editable_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

/** Singleton instance of PFS_editable_acl. */
extern PFS_editable_acl pfs_editable_acl;

/**
  Privileges for unknown tables.
*/
class PFS_unknown_acl : public ACL_internal_table_access
{
public:
  PFS_unknown_acl()
  {}

  ~PFS_unknown_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

/** Singleton instance of PFS_unknown_acl. */
extern PFS_unknown_acl pfs_unknown_acl;


/**
  Privileges for world readable tables.
*/
class PFS_readonly_world_acl : public PFS_readonly_acl
{
public:
  PFS_readonly_world_acl()
  {}

  ~PFS_readonly_world_acl()
  {}
  virtual ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};


/** Singleton instance of PFS_readonly_world_acl */
extern PFS_readonly_world_acl pfs_readonly_world_acl;


/**
Privileges for world readable truncatable tables.
*/
class PFS_truncatable_world_acl : public PFS_truncatable_acl
{
public:
  PFS_truncatable_world_acl()
  {}

  ~PFS_truncatable_world_acl()
  {}
  virtual ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};


/** Singleton instance of PFS_readonly_world_acl */
extern PFS_truncatable_world_acl pfs_truncatable_world_acl;


/** Position of a cursor, for simple iterations. */
struct PFS_simple_index
{
  /** Current row index. */
  uint m_index;

  /**
    Constructor.
    @param index the index initial value.
  */
  PFS_simple_index(uint index)
    : m_index(index)
  {}

  /**
    Set this index at a given position.
    @param index an index
  */
  void set_at(uint index)
  { m_index= index; }

  /**
    Set this index at a given position.
    @param other a position
  */
  void set_at(const struct PFS_simple_index *other)
  { m_index= other->m_index; }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const struct PFS_simple_index *other)
  { m_index= other->m_index + 1; }

  /** Set this index to the next record. */
  void next(void)
  { m_index++; }
};

/** Position of a double cursor, for iterations using 2 nested loops. */
struct PFS_double_index
{
  /** Outer index. */
  uint m_index_1;
  /** Current index within index_1. */
  uint m_index_2;

  /**
    Constructor.
    @param index_1 the first index initial value.
    @param index_2 the second index initial value.
  */
  PFS_double_index(uint index_1, uint index_2)
    : m_index_1(index_1), m_index_2(index_2)
  {}

  /**
    Set this index at a given position.
  */
  void set_at(uint index_1, uint index_2)
  {
    m_index_1= index_1;
    m_index_2= index_2;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void set_at(const struct PFS_double_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const struct PFS_double_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2 + 1;
  }
};

/** Position of a triple cursor, for iterations using 3 nested loops. */
struct PFS_triple_index
{
  /** Outer index. */
  uint m_index_1;
  /** Current index within index_1. */
  uint m_index_2;
  /** Current index within index_2. */
  uint m_index_3;

  /**
    Constructor.
    @param index_1 the first index initial value.
    @param index_2 the second index initial value.
    @param index_3 the third index initial value.
  */
  PFS_triple_index(uint index_1, uint index_2, uint index_3)
    : m_index_1(index_1), m_index_2(index_2), m_index_3(index_3)
  {}

  /**
    Set this index at a given position.
  */
  void set_at(uint index_1, uint index_2, uint index_3)
  {
    m_index_1= index_1;
    m_index_2= index_2;
    m_index_3= index_3;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void set_at(const struct PFS_triple_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2;
    m_index_3= other->m_index_3;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const struct PFS_triple_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2;
    m_index_3= other->m_index_3 + 1;
  }
};

bool pfs_show_status(handlerton *hton, THD *thd,
                     stat_print_fn *print, enum ha_stat_type stat);

/** @} */
#endif
