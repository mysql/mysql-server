/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_acl.h"                            /* struct ACL_* */
/**
  @file storage/perfschema/pfs_engine_table.h
  Performance schema tables (declarations).
*/

class Field;
struct PFS_engine_table_share;

/**
  @addtogroup Performance_schema_engine
  @{
*/

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

  /** Fetch the next row in this cursor. */
  virtual int rnd_next(void)= 0;
  /**
    Fetch a row by position.
    @param pos              position to fetch
  */
  virtual int rnd_pos(const void *pos)= 0;

  void get_position(void *ref);
  void set_position(const void *ref);
  virtual void reset_position(void)= 0;

  /** Destructor. */
  virtual ~PFS_engine_table()
  {}

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
    Constructor.
    @param share            table share
    @param pos              address of the m_pos position member
  */
  PFS_engine_table(const PFS_engine_table_share *share, void *pos)
    : m_share_ptr(share), m_pos_ptr(pos)
  {}

  void set_field_ulong(Field *f, ulong value);
  void set_field_ulonglong(Field *f, ulonglong value);
  void set_field_varchar_utf8(Field *f, const char* str, uint len);
  void set_field_enum(Field *f, ulonglong value);

  ulonglong get_field_enum(Field *f);

  /** Table share. */
  const PFS_engine_table_share *m_share_ptr;
  /** Opaque pointer to the m_pos position of this cursor. */
  void *m_pos_ptr;
};

/** Callback to open a table. */
typedef PFS_engine_table* (*pfs_open_table_t)(void);
/** Callback to write a row. */
typedef int (*pfs_write_row_t)(TABLE *table,
                               unsigned char *buf, Field **fields);
/** Callback to delete all rows. */
typedef int (*pfs_delete_all_rows_t)(void);

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
  /**
    Number or records.
    This number does not need to be precise,
    it is used by the optimizer to decide if the table
    has 0, 1, or many records.
  */
  ha_rows m_records;
  /** Length of the m_pos position structure. */
  uint m_ref_length;
  /** The lock, stored on behalf of the SQL layer. */
  THR_LOCK *m_thr_lock_ptr;
  /** Table fields definition. */
  TABLE_FIELD_DEF *m_field_def;
  /** Schema integrity flag. */
  bool m_checked;
};

class PFS_readonly_acl : public ACL_internal_table_access
{
public:
  PFS_readonly_acl()
  {}

  ~PFS_readonly_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

extern PFS_readonly_acl pfs_readonly_acl;

class PFS_truncatable_acl : public ACL_internal_table_access
{
public:
  PFS_truncatable_acl()
  {}

  ~PFS_truncatable_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

extern PFS_truncatable_acl pfs_truncatable_acl;

class PFS_updatable_acl : public ACL_internal_table_access
{
public:
  PFS_updatable_acl()
  {}

  ~PFS_updatable_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

extern PFS_updatable_acl pfs_updatable_acl;

class PFS_editable_acl : public ACL_internal_table_access
{
public:
  PFS_editable_acl()
  {}

  ~PFS_editable_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

extern PFS_editable_acl pfs_editable_acl;

class PFS_unknown_acl : public ACL_internal_table_access
{
public:
  PFS_unknown_acl()
  {}

  ~PFS_unknown_acl()
  {}

  ACL_internal_access_result check(ulong want_access, ulong *save_priv) const;
};

extern PFS_unknown_acl pfs_unknown_acl;

/** Position of a cursor, for simple iterations. */
struct PFS_simple_index
{
  /** Current row index. */
  uint m_index;

  PFS_simple_index(uint index)
    : m_index(index)
  {}

  void set_at(const struct PFS_simple_index *other)
  { m_index= other->m_index; }

  void set_after(const struct PFS_simple_index *other)
  { m_index= other->m_index + 1; }

  void next(void)
  { m_index++; }
};

struct PFS_double_index
{
  /** Outer index. */
  uint m_index_1;
  /** Current index within index_1. */
  uint m_index_2;

  PFS_double_index(uint index_1, uint index_2)
    : m_index_1(index_1), m_index_2(index_2)
  {}

  void set_at(const struct PFS_double_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2;
  }

  void set_after(const struct PFS_double_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2 + 1;
  }
};

struct PFS_triple_index
{
  /** Outer index. */
  uint m_index_1;
  /** Current index within index_1. */
  uint m_index_2;
  /** Current index within index_2. */
  uint m_index_3;

  PFS_triple_index(uint index_1, uint index_2, uint index_3)
    : m_index_1(index_1), m_index_2(index_2), m_index_3(index_3)
  {}

  void set_at(const struct PFS_triple_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2;
    m_index_3= other->m_index_3;
  }

  void set_after(const struct PFS_triple_index *other)
  {
    m_index_1= other->m_index_1;
    m_index_2= other->m_index_2;
    m_index_3= other->m_index_3 + 1;
  }
};

struct PFS_instrument_view_constants
{
  static const uint VIEW_MUTEX= 1;
  static const uint VIEW_RWLOCK= 2;
  static const uint VIEW_COND= 3;
  static const uint VIEW_FILE= 4;
};

struct PFS_object_view_constants
{
  static const uint VIEW_TABLE= 1;
  static const uint VIEW_EVENT= 2;
  static const uint VIEW_PROCEDURE= 3;
  static const uint VIEW_FUNCTION= 4;
};

bool pfs_show_status(handlerton *hton, THD *thd,
                     stat_print_fn *print, enum ha_stat_type stat);

/** @} */
#endif
