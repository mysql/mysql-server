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

#ifndef PFS_ENGINE_TABLE_H
#define PFS_ENGINE_TABLE_H

#include <stddef.h>
#include <sys/types.h>

#include "auth_common.h" /* struct ACL_* */
#include "key.h"
#include "lex_string.h"
#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_thread_local.h" /* thread_local_key_t */

class PFS_engine_key;
class PFS_engine_index;

typedef struct st_thr_lock THR_LOCK;
typedef struct st_table_field_def TABLE_FIELD_DEF;

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
  @addtogroup performance_schema_engine
  @{
*/

/**
  Store and retrieve table state information during a query.
*/
class PFS_table_context
{
public:
  PFS_table_context(ulonglong current_version,
                    bool restore,
                    thread_local_key_t key);
  PFS_table_context(ulonglong current_version,
                    ulong map_size,
                    bool restore,
                    thread_local_key_t key);
  ~PFS_table_context(void);

  bool initialize(void);
  bool
  is_initialized(void)
  {
    return m_initialized;
  }
  ulonglong
  current_version(void)
  {
    return m_current_version;
  }
  ulonglong
  last_version(void)
  {
    return m_last_version;
  }
  bool
  versions_match(void)
  {
    return m_last_version == m_current_version;
  }
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
  static const PFS_engine_table_share *find_engine_table_share(
    const char *name);

  int read_row(TABLE *table, unsigned char *buf, Field **fields);

  int update_row(TABLE *table,
                 const unsigned char *old_buf,
                 unsigned char *new_buf,
                 Field **fields);

  /**
    Delete a row from this table.
    @param table Table handle
    @param buf the row buffer to delete
    @param fields Table fields
    @return 0 on success
  */
  int delete_row(TABLE *table, const unsigned char *buf, Field **fields);

  /** Initialize table scan. */
  virtual int
  rnd_init(bool scan MY_ATTRIBUTE((unused)))
  {
    return 0;
  }

  /** Fetch the next row in this cursor. */
  virtual int rnd_next(void) = 0;

  virtual int
  index_init(uint idx MY_ATTRIBUTE((unused)),
             bool sorted MY_ATTRIBUTE((unused)))
  {
    DBUG_ASSERT(false);
    return HA_ERR_UNSUPPORTED;
  }

  virtual int index_read(KEY *key_infos,
                         uint index,
                         const uchar *key,
                         uint key_len,
                         enum ha_rkey_function find_flag);

  virtual int
  index_read_last(KEY *key_infos MY_ATTRIBUTE((unused)),
                  const uchar *key MY_ATTRIBUTE((unused)),
                  uint key_len MY_ATTRIBUTE((unused)))
  {
    return HA_ERR_UNSUPPORTED;
  }

  /** Find key in index, read record. */
  virtual int
  index_next()
  {
    return HA_ERR_UNSUPPORTED;
  }

  virtual int index_next_same(const uchar *key, uint key_len);
  virtual int
  index_prev()
  {
    return HA_ERR_UNSUPPORTED;
  }

  virtual int
  index_first()
  {
    return HA_ERR_UNSUPPORTED;
  }

  virtual int
  index_last()
  {
    return HA_ERR_UNSUPPORTED;
  }

  /**
    Fetch a row by position.
    @param pos              position to fetch
  */
  virtual int rnd_pos(const void *pos) = 0;

  void get_position(void *ref);
  void set_position(const void *ref);
  /** Reset the cursor position to the beginning of the table. */
  virtual void reset_position(void) = 0;

  /** Get the normalizer and class type for the current row. */
  void get_normalizer(PFS_instr_class *instr_class);

  /** Destructor. */
  virtual ~PFS_engine_table()
  {
  }

protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */
  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all) = 0;

  /**
    Update the current row values.
    @param table            Table handle
    @param old_buf          old row buffer
    @param new_buf          new row buffer
    @param fields           Table fields
  */
  virtual int update_row_values(TABLE *table,
                                const unsigned char *old_buf,
                                unsigned char *new_buf,
                                Field **fields);

  /**
    Delete a row.
    @param table            Table handle
    @param buf              Row buffer
    @param fields           Table fields
  */
  virtual int delete_row_values(TABLE *table,
                                const unsigned char *buf,
                                Field **fields);
  /**
    Constructor.
    @param share            table share
    @param pos              address of the m_pos position member
  */
  PFS_engine_table(const PFS_engine_table_share *share, void *pos)
    : m_share_ptr(share),
      m_pos_ptr(pos),
      m_normalizer(NULL),
      m_class_type(PFS_CLASS_NONE),
      m_index(NULL)
  {
  }

  /** Table share. */
  const PFS_engine_table_share *m_share_ptr;
  /** Opaque pointer to the @c m_pos position of this cursor. */
  void *m_pos_ptr;
  /** Current normalizer */
  time_normalizer *m_normalizer;
  /** Current class type */
  enum PFS_class_type m_class_type;
  /** Current index. */
  PFS_engine_index *m_index;
};

/** Callback to open a table. */
typedef PFS_engine_table *(*pfs_open_table_t)(void);
/** Callback to write a row. */
typedef int (*pfs_write_row_t)(TABLE *table,
                               unsigned char *buf,
                               Field **fields);
/** Callback to delete all rows. */
typedef int (*pfs_delete_all_rows_t)(void);
/** Callback to get a row count. */
typedef ha_rows (*pfs_get_row_count_t)(void);

/**
  PFS_key_reader: Convert key into internal format.
*/
struct PFS_key_reader
{
  PFS_key_reader(const KEY *key_info, const uchar *key, uint key_len)
    : m_key_info(key_info),
      m_key_part_info(key_info->key_part),
      m_key(key),
      m_key_len(key_len),
      m_remaining_key_part_info(key_info->key_part),
      m_remaining_key(key),
      m_remaining_key_len(key_len),
      m_parts_found(0)
  {
  }

  enum ha_rkey_function read_uchar(enum ha_rkey_function find_flag,
                                   bool &isnull,
                                   uchar *value);

  enum ha_rkey_function read_long(enum ha_rkey_function find_flag,
                                  bool &isnull,
                                  long *value);

  enum ha_rkey_function read_ulong(enum ha_rkey_function find_flag,
                                   bool &isnull,
                                   ulong *value);

  enum ha_rkey_function read_ulonglong(enum ha_rkey_function find_flag,
                                       bool &isnull,
                                       ulonglong *value);

  enum ha_rkey_function read_varchar_utf8(enum ha_rkey_function find_flag,
                                          bool &isnull,
                                          char *buffer,
                                          uint *buffer_length,
                                          uint buffer_capacity);

  enum ha_rkey_function read_text_utf8(enum ha_rkey_function find_flag,
                                       bool &isnull,
                                       char *buffer,
                                       uint *buffer_length,
                                       uint buffer_capacity);

  ha_base_keytype
  get_key_type(void)
  {
    return (enum ha_base_keytype)m_remaining_key_part_info->type;
  }

private:
  const KEY *m_key_info;
  const KEY_PART_INFO *m_key_part_info;
  const uchar *m_key;
  uint m_key_len;
  const KEY_PART_INFO *m_remaining_key_part_info;
  const uchar *m_remaining_key;
  uint m_remaining_key_len;

public:
  uint m_parts_found;
};

class PFS_engine_key
{
public:
  PFS_engine_key(const char *name) : m_name(name), m_is_null(true)
  {
  }

  virtual ~PFS_engine_key()
  {
  }

  virtual void read(PFS_key_reader &reader,
                    enum ha_rkey_function find_flag) = 0;

  const char *m_name;

protected:
  enum ha_rkey_function m_find_flag;
  bool m_is_null;
};

class PFS_engine_index
{
public:
  PFS_engine_index(PFS_engine_key *key_1)
    : m_key_ptr_1(key_1),
      m_key_ptr_2(NULL),
      m_key_ptr_3(NULL),
      m_key_ptr_4(NULL),
      m_fields(0),
      m_key_info(NULL)
  {
  }

  PFS_engine_index(PFS_engine_key *key_1, PFS_engine_key *key_2)
    : m_key_ptr_1(key_1),
      m_key_ptr_2(key_2),
      m_key_ptr_3(NULL),
      m_key_ptr_4(NULL),
      m_fields(0),
      m_key_info(NULL)
  {
  }

  PFS_engine_index(PFS_engine_key *key_1,
                   PFS_engine_key *key_2,
                   PFS_engine_key *key_3)
    : m_key_ptr_1(key_1),
      m_key_ptr_2(key_2),
      m_key_ptr_3(key_3),
      m_key_ptr_4(NULL),
      m_fields(0),
      m_key_info(NULL)
  {
  }

  PFS_engine_index(PFS_engine_key *key_1,
                   PFS_engine_key *key_2,
                   PFS_engine_key *key_3,
                   PFS_engine_key *key_4)
    : m_key_ptr_1(key_1),
      m_key_ptr_2(key_2),
      m_key_ptr_3(key_3),
      m_key_ptr_4(key_4),
      m_fields(0),
      m_key_info(NULL)
  {
  }

  virtual ~PFS_engine_index()
  {
  }

  void
  set_key_info(KEY *key_info)
  {
    m_key_info = key_info;
  }

  void read_key(const uchar *key,
                uint key_len,
                enum ha_rkey_function find_flag);

  PFS_engine_key *m_key_ptr_1;
  PFS_engine_key *m_key_ptr_2;
  PFS_engine_key *m_key_ptr_3;
  PFS_engine_key *m_key_ptr_4;

  uint m_fields;
  KEY *m_key_info;
};

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
  /** Table Access Control List. */
  const ACL_internal_table_access *m_acl;
  /** Open table function. */
  pfs_open_table_t m_open_table;
  /** Write row function. */
  pfs_write_row_t m_write_row;
  /** Delete all rows function. */
  pfs_delete_all_rows_t m_delete_all_rows;
  /** Get rows count function. */
  pfs_get_row_count_t m_get_row_count;
  /** Length of the @c m_pos position structure. */
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
  {
  }

  ~PFS_readonly_acl()
  {
  }

  virtual ACL_internal_access_result check(ulong want_access,
                                           ulong *save_priv) const;
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
  {
  }

  ~PFS_truncatable_acl()
  {
  }

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
  {
  }

  ~PFS_updatable_acl()
  {
  }

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
  {
  }

  ~PFS_editable_acl()
  {
  }

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
  {
  }

  ~PFS_unknown_acl()
  {
  }

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
  {
  }

  ~PFS_readonly_world_acl()
  {
  }
  virtual ACL_internal_access_result check(ulong want_access,
                                           ulong *save_priv) const;
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
  {
  }

  ~PFS_truncatable_world_acl()
  {
  }
  virtual ACL_internal_access_result check(ulong want_access,
                                           ulong *save_priv) const;
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
  PFS_simple_index(uint index) : m_index(index)
  {
  }

  /**
    Set this index at a given position.
    @param index an index
  */
  void
  set_at(uint index)
  {
    m_index = index;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void
  set_at(const PFS_simple_index *other)
  {
    m_index = other->m_index;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void
  set_after(const PFS_simple_index *other)
  {
    m_index = other->m_index + 1;
  }

  /** Set this index to the next record. */
  void
  next(void)
  {
    m_index++;
  }
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
  {
  }

  /**
    Set this index at a given position.
  */
  void
  set_at(uint index_1, uint index_2)
  {
    m_index_1 = index_1;
    m_index_2 = index_2;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void
  set_at(const PFS_double_index *other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void
  set_after(const PFS_double_index *other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2 + 1;
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
  {
  }

  /**
    Set this index at a given position.
  */
  void
  set_at(uint index_1, uint index_2, uint index_3)
  {
    m_index_1 = index_1;
    m_index_2 = index_2;
    m_index_3 = index_3;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void
  set_at(const PFS_triple_index *other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
    m_index_3 = other->m_index_3;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void
  set_after(const PFS_triple_index *other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
    m_index_3 = other->m_index_3 + 1;
  }
};

/** @} */
#endif
