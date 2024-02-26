/*
   Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

/**
  @file storage/myisammrg/ha_myisammrg.h
  MyISAM merge storage engine.
*/

#include <sys/types.h>

#include "lex_string.h"
#include "my_double2ulonglong.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "myisammrg.h"
#include "myrg_def.h"
#include "sql/handler.h"
#include "sql/table.h"

/**
  Represents one name of a MERGE child.

  @todo Add MYRG_SHARE and store children names in the
  share.
*/

class Mrg_child_def {
  /* Remembered MERGE child def version.  See top comment in ha_myisammrg.cc */
  enum_table_ref_type m_child_table_ref_type;
  ulonglong m_child_def_version;

 public:
  LEX_STRING db;
  LEX_STRING name;

  /* Access MERGE child def version.  See top comment in ha_myisammrg.cc */
  inline enum_table_ref_type get_child_table_ref_type() {
    return m_child_table_ref_type;
  }
  inline ulonglong get_child_def_version() { return m_child_def_version; }
  inline void set_child_def_version(enum_table_ref_type child_table_ref_type,
                                    ulonglong version) {
    m_child_table_ref_type = child_table_ref_type;
    m_child_def_version = version;
  }

  Mrg_child_def(char *db_arg, size_t db_len_arg, char *table_name_arg,
                size_t table_name_len_arg) {
    db.str = db_arg;
    db.length = db_len_arg;
    name.str = table_name_arg;
    name.length = table_name_len_arg;
    m_child_def_version = ~0ULL;
    m_child_table_ref_type = TABLE_REF_NULL;
  }
};

class ha_myisammrg : public handler {
  MYRG_INFO *file;
  bool is_cloned; /* This instance has been cloned */

 public:
  /* mem root for children list */
  MEM_ROOT children_mem_root{rg_key_memory_children, FN_REFLEN};
  List<Mrg_child_def> child_def_list;
  Table_ref *children_l;       /* children list */
  Table_ref **children_last_l; /* children list end */
  uint test_if_locked;         /* flags from ::open() */

  ha_myisammrg(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_myisammrg() override = default;
  const char *table_type() const override { return "MRG_MyISAM"; }
  enum ha_key_alg get_default_index_algorithm() const override {
    return HA_KEY_ALG_BTREE;
  }
  bool is_index_algorithm_supported(enum ha_key_alg key_alg) const override {
    return key_alg == HA_KEY_ALG_BTREE || key_alg == HA_KEY_ALG_RTREE;
  }
  ulonglong table_flags() const override {
    return (HA_AUTO_PART_KEY | HA_NO_TRANSACTIONS | HA_BINLOG_ROW_CAPABLE |
            HA_BINLOG_STMT_CAPABLE | HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS |
            HA_FILE_BASED | HA_ANY_INDEX_MAY_BE_UNIQUE | HA_CAN_BIT_FIELD |
            HA_COUNT_ROWS_INSTANT | HA_NO_COPY_ON_ALTER | HA_DUPLICATE_POS);
  }
  ulong index_flags(uint inx, uint, bool) const override {
    return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT)
                ? 0
                : HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE | HA_READ_ORDER |
                      HA_KEYREAD_ONLY);
  }
  uint max_supported_keys() const override { return MI_MAX_KEY; }
  uint max_supported_key_length() const override { return MI_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length(HA_CREATE_INFO *create_info
                                     [[maybe_unused]]) const override {
    return MI_MAX_KEY_LENGTH;
  }
  double scan_time() override {
    return ulonglong2double(stats.data_file_length) / IO_SIZE + file->tables;
  }

  int open(const char *name, int mode, uint test_if_locked_arg,
           const dd::Table *table_def) override;
  int add_children_list(void);
  int attach_children(void);
  int detach_children(void);
  handler *clone(const char *name, MEM_ROOT *mem_root) override;
  int close(void) override;
  int write_row(uchar *buf) override;
  int update_row(const uchar *old_data, uchar *new_data) override;
  int delete_row(const uchar *buf) override;
  int index_read_map(uchar *buf, const uchar *key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag) override;
  int index_read_idx_map(uchar *buf, uint index, const uchar *key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag) override;
  int index_read_last_map(uchar *buf, const uchar *key,
                          key_part_map keypart_map) override;
  int index_next(uchar *buf) override;
  int index_prev(uchar *buf) override;
  int index_first(uchar *buf) override;
  int index_last(uchar *buf) override;
  int index_next_same(uchar *buf, const uchar *key, uint keylen) override;
  int rnd_init(bool scan) override;
  int rnd_next(uchar *buf) override;
  int rnd_pos(uchar *buf, uchar *pos) override;
  void position(const uchar *record) override;
  ha_rows records_in_range(uint inx, key_range *min_key,
                           key_range *max_key) override;
  int truncate(dd::Table *table_def) override;
  int info(uint) override;
  int reset(void) override;
  int extra(enum ha_extra_function operation) override;
  int extra_opt(enum ha_extra_function operation, ulong cache_size) override;
  int external_lock(THD *thd, int lock_type) override;
  uint lock_count(void) const override;
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info,
             dd::Table *table_def) override;
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type) override;
  void update_create_info(HA_CREATE_INFO *create_info) override;
  void append_create_info(String *packet) override;
  MYRG_INFO *myrg_info() { return file; }
  TABLE *table_ptr() { return table; }
  bool check_if_incompatible_data(HA_CREATE_INFO *info,
                                  uint table_changes) override;
  int check(THD *thd, HA_CHECK_OPT *check_opt) override;
  int records(ha_rows *num_rows) override;
};
