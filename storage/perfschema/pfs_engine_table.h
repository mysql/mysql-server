/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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

#ifndef PFS_ENGINE_TABLE_H
#define PFS_ENGINE_TABLE_H

#include <assert.h>
#include <mysql/components/services/pfs_plugin_table_service.h>
#include <stddef.h>
#include <sys/types.h>
#include <atomic>
#include <vector>

#include "my_base.h"
#include "my_compiler.h"

#include "my_inttypes.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/psi/mysql_mutex.h"
#include "sql/auth/auth_common.h" /* struct ACL_* */
#include "sql/key.h"
#include "storage/perfschema/pfs.h"

class PFS_engine_index_abstract;
class Plugin_table;
struct TABLE;
struct THR_LOCK;
template <class T>
class List;

/**
  @file storage/perfschema/pfs_engine_table.h
  Performance schema tables (declarations).
*/

class Field;
struct PFS_engine_table_share;
struct time_normalizer;

/**
  @addtogroup performance_schema_engine
  @{
*/

/**
  An abstract PERFORMANCE_SCHEMA table.
  Every table implemented in the performance schema schema and storage engine
  derives from this class.
*/
class PFS_engine_table {
 public:
  static PFS_engine_table_share *find_engine_table_share(const char *name);

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
  virtual int rnd_init(bool scan [[maybe_unused]]) { return 0; }

  /** Fetch the next row in this cursor. */
  virtual int rnd_next() = 0;

  virtual int index_init(uint idx [[maybe_unused]],
                         bool sorted [[maybe_unused]]) {
    assert(false);
    return HA_ERR_UNSUPPORTED;
  }

  virtual int index_read(KEY *key_infos, uint index, const uchar *key,
                         uint key_len, enum ha_rkey_function find_flag);

  virtual int index_read_last(KEY *key_infos [[maybe_unused]],
                              const uchar *key [[maybe_unused]],
                              uint key_len [[maybe_unused]]) {
    return HA_ERR_UNSUPPORTED;
  }

  /** Find key in index, read record. */
  virtual int index_next() { return HA_ERR_UNSUPPORTED; }

  virtual int index_next_same(const uchar *key, uint key_len);
  virtual int index_prev() { return HA_ERR_UNSUPPORTED; }

  virtual int index_first() { return HA_ERR_UNSUPPORTED; }

  virtual int index_last() { return HA_ERR_UNSUPPORTED; }

  /**
    Fetch a row by position.
    @param pos              position to fetch
  */
  virtual int rnd_pos(const void *pos) = 0;

  void get_position(void *ref);
  void set_position(const void *ref);
  /** Reset the cursor position to the beginning of the table. */
  virtual void reset_position() = 0;

  /** Destructor. */
  virtual ~PFS_engine_table() = default;

 protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */
  virtual int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                              bool read_all) = 0;

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
      : m_share_ptr(share),
        m_pos_ptr(pos),
        m_normalizer(nullptr),
        m_index(nullptr) {}

  /** Table share. */
  const PFS_engine_table_share *m_share_ptr;
  /** Opaque pointer to the @c m_pos position of this cursor. */
  void *m_pos_ptr;
  /** Current normalizer */
  time_normalizer *m_normalizer;
  /** Current index. */
  PFS_engine_index_abstract *m_index;
};

/** Callback to open a table. */
typedef PFS_engine_table *(*pfs_open_table_t)(PFS_engine_table_share *);
/** Callback to write a row. */
typedef int (*pfs_write_row_t)(PFS_engine_table *pfs_table, TABLE *table,
                               unsigned char *buf, Field **fields);
/** Callback to delete all rows. */
typedef int (*pfs_delete_all_rows_t)();
/** Callback to get a row count. */
typedef ha_rows (*pfs_get_row_count_t)();

/**
  PFS_key_reader: Convert key into internal format.
*/
struct PFS_key_reader {
  PFS_key_reader(const KEY *key_info, const uchar *key, uint key_len)
      : m_key_info(key_info),
        m_key_part_info(key_info->key_part),
        m_key(key),
        m_key_len(key_len),
        m_remaining_key_part_info(key_info->key_part),
        m_remaining_key(key),
        m_remaining_key_len(key_len),
        m_parts_found(0) {}

  enum ha_rkey_function read_int8(enum ha_rkey_function find_flag, bool &isnull,
                                  char *value);

  enum ha_rkey_function read_uint8(enum ha_rkey_function find_flag,
                                   bool &isnull, uchar *value);

  enum ha_rkey_function read_int16(enum ha_rkey_function find_flag,
                                   bool &isnull, short *value);

  enum ha_rkey_function read_uint16(enum ha_rkey_function find_flag,
                                    bool &isnull, ushort *value);

  enum ha_rkey_function read_int24(enum ha_rkey_function find_flag,
                                   bool &isnull, long *value);

  enum ha_rkey_function read_uint24(enum ha_rkey_function find_flag,
                                    bool &isnull, ulong *value);

  enum ha_rkey_function read_long(enum ha_rkey_function find_flag, bool &isnull,
                                  long *value);

  enum ha_rkey_function read_ulong(enum ha_rkey_function find_flag,
                                   bool &isnull, ulong *value);

  enum ha_rkey_function read_longlong(enum ha_rkey_function find_flag,
                                      bool &isnull, longlong *value);

  enum ha_rkey_function read_ulonglong(enum ha_rkey_function find_flag,
                                       bool &isnull, ulonglong *value);

  enum ha_rkey_function read_timestamp(enum ha_rkey_function find_flag,
                                       bool &isnull, ulonglong *value,
                                       uint dec);

  enum ha_rkey_function read_varchar_utf8(enum ha_rkey_function find_flag,
                                          bool &isnull, char *buffer,
                                          uint *buffer_length,
                                          uint buffer_capacity);

  enum ha_rkey_function read_text_utf8(enum ha_rkey_function find_flag,
                                       bool &isnull, char *buffer,
                                       uint *buffer_length,
                                       uint buffer_capacity);

  ha_base_keytype get_key_type() {
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

class PFS_engine_key {
 public:
  explicit PFS_engine_key(const char *name) : m_name(name), m_is_null(true) {}

  virtual ~PFS_engine_key() = default;

  virtual void read(PFS_key_reader &reader,
                    enum ha_rkey_function find_flag) = 0;

  const char *m_name;

 protected:
  enum ha_rkey_function m_find_flag;
  bool m_is_null;
};

class PFS_engine_index_abstract {
 public:
  PFS_engine_index_abstract() = default;

  virtual ~PFS_engine_index_abstract() = default;

  void set_key_info(KEY *key_info) { m_key_info = key_info; }

  virtual void read_key(const uchar *key, uint key_len,
                        enum ha_rkey_function find_flag) = 0;

 public:
  uint m_fields{0};
  KEY *m_key_info{nullptr};
};

class PFS_engine_index : public PFS_engine_index_abstract {
 public:
  explicit PFS_engine_index(PFS_engine_key *key_1)
      : m_key_ptr_1(key_1),
        m_key_ptr_2(nullptr),
        m_key_ptr_3(nullptr),
        m_key_ptr_4(nullptr),
        m_key_ptr_5(nullptr) {}

  PFS_engine_index(PFS_engine_key *key_1, PFS_engine_key *key_2)
      : m_key_ptr_1(key_1),
        m_key_ptr_2(key_2),
        m_key_ptr_3(nullptr),
        m_key_ptr_4(nullptr),
        m_key_ptr_5(nullptr) {}

  PFS_engine_index(PFS_engine_key *key_1, PFS_engine_key *key_2,
                   PFS_engine_key *key_3)
      : m_key_ptr_1(key_1),
        m_key_ptr_2(key_2),
        m_key_ptr_3(key_3),
        m_key_ptr_4(nullptr),
        m_key_ptr_5(nullptr) {}

  PFS_engine_index(PFS_engine_key *key_1, PFS_engine_key *key_2,
                   PFS_engine_key *key_3, PFS_engine_key *key_4)
      : m_key_ptr_1(key_1),
        m_key_ptr_2(key_2),
        m_key_ptr_3(key_3),
        m_key_ptr_4(key_4),
        m_key_ptr_5(nullptr) {}

  PFS_engine_index(PFS_engine_key *key_1, PFS_engine_key *key_2,
                   PFS_engine_key *key_3, PFS_engine_key *key_4,
                   PFS_engine_key *key_5)
      : m_key_ptr_1(key_1),
        m_key_ptr_2(key_2),
        m_key_ptr_3(key_3),
        m_key_ptr_4(key_4),
        m_key_ptr_5(key_5) {}

  ~PFS_engine_index() override = default;

  void read_key(const uchar *key, uint key_len,
                enum ha_rkey_function find_flag) override;

  PFS_engine_key *m_key_ptr_1;
  PFS_engine_key *m_key_ptr_2;
  PFS_engine_key *m_key_ptr_3;
  PFS_engine_key *m_key_ptr_4;
  PFS_engine_key *m_key_ptr_5;
};

/**

  A PERFORMANCE_SCHEMA table share.
  This data is shared by all the table handles opened on the same table.
*/
struct PFS_engine_table_share {
  static void get_all_tables(List<const Plugin_table> *tables);
  static void init_all_locks();
  static void delete_all_locks();

  /** Get the row count. */
  ha_rows get_row_count() const;
  /** Write a row. */
  int write_row(PFS_engine_table *pfs_table, TABLE *table, unsigned char *buf,
                Field **fields) const;
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
  /** Table definition. */
  const Plugin_table *m_table_def;
  /** Table is available even if the Performance Schema is disabled. */
  bool m_perpetual;

  /* Interface to be implemented by plugin who adds its own table in PFS. */
  PFS_engine_table_proxy m_st_table;
  /* Number of table objects using this share currently. */
  std::atomic<int> m_ref_count;
  /* is marked to be deleted? */
  bool m_in_purgatory;
};

/**
 * A class to keep list of table shares for non-native performance schema
 * tables i.e. table created by plugins/components in performance schema.
 */
class PFS_dynamic_table_shares {
 public:
  PFS_dynamic_table_shares() = default;

  void init_mutex();

  void destroy_mutex();

  void lock_share_list() { mysql_mutex_lock(&LOCK_pfs_share_list); }

  void unlock_share_list() { mysql_mutex_unlock(&LOCK_pfs_share_list); }

  void add_share(PFS_engine_table_share *share) {
    mysql_mutex_assert_owner(&LOCK_pfs_share_list);
    shares_vector.push_back(share);
  }

  PFS_engine_table_share *find_share(const char *table_name, bool is_dead_too);

  void remove_share(PFS_engine_table_share *share);

 private:
  std::vector<PFS_engine_table_share *> shares_vector;
  mysql_mutex_t LOCK_pfs_share_list;
};

/* List of table shares added by plugin/component */
extern PFS_dynamic_table_shares pfs_external_table_shares;

/**
  Privileges for read only tables.
  The only operation allowed is SELECT.
*/
class PFS_readonly_acl : public ACL_internal_table_access {
 public:
  PFS_readonly_acl() = default;

  ~PFS_readonly_acl() override = default;

  ACL_internal_access_result check(ulong want_access, ulong *granted_access,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_readonly_acl. */
extern PFS_readonly_acl pfs_readonly_acl;

/**
  Privileges for truncatable tables.
  Operations allowed are SELECT and TRUNCATE.
*/
class PFS_truncatable_acl : public ACL_internal_table_access {
 public:
  PFS_truncatable_acl() = default;

  ~PFS_truncatable_acl() override = default;

  ACL_internal_access_result check(ulong want_access, ulong *granted_access,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_truncatable_acl. */
extern PFS_truncatable_acl pfs_truncatable_acl;

/**
  Privileges for updatable tables.
  Operations allowed are SELECT and UPDATE.
*/
class PFS_updatable_acl : public ACL_internal_table_access {
 public:
  PFS_updatable_acl() = default;

  ~PFS_updatable_acl() override = default;

  ACL_internal_access_result check(ulong want_access, ulong *granted_access,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_updatable_acl. */
extern PFS_updatable_acl pfs_updatable_acl;

/**
  Privileges for editable tables.
  Operations allowed are SELECT, INSERT, UPDATE, DELETE and TRUNCATE.
*/
class PFS_editable_acl : public ACL_internal_table_access {
 public:
  PFS_editable_acl() = default;

  ~PFS_editable_acl() override = default;

  ACL_internal_access_result check(ulong want_access, ulong *granted_access,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_editable_acl. */
extern PFS_editable_acl pfs_editable_acl;

/**
  Privileges for unknown tables.
*/
class PFS_unknown_acl : public ACL_internal_table_access {
 public:
  PFS_unknown_acl() = default;

  ~PFS_unknown_acl() override = default;

  ACL_internal_access_result check(ulong want_access, ulong *granted_access,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_unknown_acl. */
extern PFS_unknown_acl pfs_unknown_acl;

/**
  Privileges for world readable tables.
*/
class PFS_readonly_world_acl : public PFS_readonly_acl {
 public:
  PFS_readonly_world_acl() = default;

  ~PFS_readonly_world_acl() override = default;
  ACL_internal_access_result check(ulong want_access, ulong *save_priv,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_readonly_world_acl */
extern PFS_readonly_world_acl pfs_readonly_world_acl;

/**
Privileges for world readable truncatable tables.
*/
class PFS_truncatable_world_acl : public PFS_truncatable_acl {
 public:
  PFS_truncatable_world_acl() = default;

  ~PFS_truncatable_world_acl() override = default;
  ACL_internal_access_result check(ulong want_access, ulong *save_priv,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_readonly_world_acl */
extern PFS_truncatable_world_acl pfs_truncatable_world_acl;

/**
  Privileges for readable processlist tables.
*/
class PFS_readonly_processlist_acl : public PFS_readonly_acl {
 public:
  PFS_readonly_processlist_acl() = default;

  ~PFS_readonly_processlist_acl() override = default;
  ACL_internal_access_result check(ulong want_access, ulong *save_priv,
                                   bool any_combination_will_do) const override;
};

/** Singleton instance of PFS_readonly_processlist_acl */
extern PFS_readonly_processlist_acl pfs_readonly_processlist_acl;

/** Position of a cursor, for simple iterations. */
struct PFS_simple_index {
  /** Current row index. */
  uint m_index;

  /**
    Constructor.
    @param index the index initial value.
  */
  PFS_simple_index(uint index) : m_index(index) {}

  /**
    Set this index at a given position.
    @param index an index
  */
  void set_at(uint index) { m_index = index; }

  /**
    Set this index at a given position.
    @param other a position
  */
  void set_at(const PFS_simple_index *other) { m_index = other->m_index; }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const PFS_simple_index *other) {
    m_index = other->m_index + 1;
  }

  /** Set this index to the next record. */
  void next() { m_index++; }
};

/** Position of a double cursor, for iterations using 2 nested loops. */
struct PFS_double_index {
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
      : m_index_1(index_1), m_index_2(index_2) {}

  /**
    Set this index at a given position.
  */
  void set_at(uint index_1, uint index_2) {
    m_index_1 = index_1;
    m_index_2 = index_2;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void set_at(const PFS_double_index *other) {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const PFS_double_index *other) {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2 + 1;
  }
};

/** Position of a triple cursor, for iterations using 3 nested loops. */
struct PFS_triple_index {
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
      : m_index_1(index_1), m_index_2(index_2), m_index_3(index_3) {}

  /**
    Set this index at a given position.
  */
  void set_at(uint index_1, uint index_2, uint index_3) {
    m_index_1 = index_1;
    m_index_2 = index_2;
    m_index_3 = index_3;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void set_at(const PFS_triple_index *other) {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
    m_index_3 = other->m_index_3;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void set_after(const PFS_triple_index *other) {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
    m_index_3 = other->m_index_3 + 1;
  }
};

/** @} */

#endif
