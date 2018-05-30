/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/myisam/ha_myisam.h
  MyISAM storage engine.
*/

#include <stddef.h>
#include <sys/types.h>

#include "ft_global.h"
#include "my_icp.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "myisam.h"
#include "sql/handler.h" /* handler */
#include "sql/table.h"   /* TABLE_SHARE */
#include "sql_string.h"
#include "typelib.h"

struct TABLE_SHARE;
struct HA_CREATE_INFO;

#define HA_RECOVER_DEFAULT 1 /* Automatic recover active */
#define HA_RECOVER_BACKUP 2  /* Make a backupfile on recover */
#define HA_RECOVER_FORCE 4   /* Recover even if we loose rows */
#define HA_RECOVER_QUICK 8   /* Don't check rows in data file */
#define HA_RECOVER_OFF 16    /* No automatic recover */

extern TYPELIB myisam_recover_typelib;
extern const char *myisam_recover_names[];
extern ulonglong myisam_recover_options;
extern const char *myisam_stats_method_names[];

int table2myisam(TABLE *table_arg, MI_KEYDEF **keydef_out,
                 MI_COLUMNDEF **recinfo_out, uint *records_out);
int check_definition(MI_KEYDEF *t1_keyinfo, MI_COLUMNDEF *t1_recinfo,
                     uint t1_keys, uint t1_recs, MI_KEYDEF *t2_keyinfo,
                     MI_COLUMNDEF *t2_recinfo, uint t2_keys, uint t2_recs,
                     bool strict);

C_MODE_START
ICP_RESULT index_cond_func_myisam(void *arg);
C_MODE_END

class Myisam_handler_share : public Handler_share {
 public:
  Myisam_handler_share() : m_share(NULL) {}
  ~Myisam_handler_share() {}
  MYISAM_SHARE *m_share;
};

class ha_myisam : public handler {
  MI_INFO *file;
  ulonglong int_table_flags;
  char *data_file_name, *index_file_name;
  bool can_enable_indexes;
  int repair(THD *thd, MI_CHECK &param, bool optimize);

 public:
  ha_myisam(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_myisam() {}
  handler *clone(const char *name, MEM_ROOT *mem_root);
  const char *table_type() const { return "MyISAM"; }
  virtual enum ha_key_alg get_default_index_algorithm() const {
    return HA_KEY_ALG_BTREE;
  }
  virtual bool is_index_algorithm_supported(enum ha_key_alg key_alg) const {
    return key_alg == HA_KEY_ALG_BTREE || key_alg == HA_KEY_ALG_RTREE;
  }
  ulonglong table_flags() const { return int_table_flags; }
  int index_init(uint idx, bool sorted);
  int index_end();
  int rnd_end();

  ulong index_flags(uint inx, uint, bool) const {
    if (table_share->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT) return 0;

    ulong flags = HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE | HA_READ_ORDER |
                  HA_KEYREAD_ONLY | HA_DO_INDEX_COND_PUSHDOWN;

    // @todo: Check if spatial indexes really have all these properties
    if (table_share->key_info[inx].flags & HA_SPATIAL)
      flags |= HA_KEY_SCAN_NOT_ROR;

    return flags;
  }
  uint max_supported_keys() const { return MI_MAX_KEY; }
  uint max_supported_key_length() const { return MI_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length(
      HA_CREATE_INFO *create_info MY_ATTRIBUTE((unused))) const {
    return MI_MAX_KEY_LENGTH;
  }
  uint checksum() const;

  int open(const char *name, int mode, uint test_if_locked,
           const dd::Table *table_def);
  int close(void);
  int write_row(uchar *buf);
  int update_row(const uchar *old_data, uchar *new_data);
  int delete_row(const uchar *buf);
  int index_read_map(uchar *buf, const uchar *key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_idx_map(uchar *buf, uint index, const uchar *key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_read_last_map(uchar *buf, const uchar *key,
                          key_part_map keypart_map);
  int index_next(uchar *buf);
  int index_prev(uchar *buf);
  int index_first(uchar *buf);
  int index_last(uchar *buf);
  int index_next_same(uchar *buf, const uchar *key, uint keylen);
  int ft_init() {
    if (!ft_handler) return 1;
    ft_handler->please->reinit_search(ft_handler);
    return 0;
  }
  FT_INFO *ft_init_ext(uint flags, uint inx, String *key) {
    return ft_init_search(flags, file, inx, (uchar *)key->ptr(),
                          (uint)key->length(), key->charset(),
                          table->record[0]);
  }
  int ft_read(uchar *buf);
  int rnd_init(bool scan);
  int rnd_next(uchar *buf);
  int rnd_pos(uchar *buf, uchar *pos);
  void position(const uchar *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int reset(void);
  int external_lock(THD *thd, int lock_type);
  int delete_all_rows(void);
  int disable_indexes(uint mode);
  int enable_indexes(uint mode);
  int indexes_are_disabled(void);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  void update_create_info(HA_CREATE_INFO *create_info);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info,
             dd::Table *table_def);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  int rename_table(const char *from, const char *to,
                   const dd::Table *from_table_def, dd::Table *to_table_def);
  int delete_table(const char *name, const dd::Table *table_def);
  int check(THD *thd, HA_CHECK_OPT *check_opt);
  int analyze(THD *thd, HA_CHECK_OPT *check_opt);
  int repair(THD *thd, HA_CHECK_OPT *check_opt);
  bool check_and_repair(THD *thd);
  bool is_crashed() const;
  bool auto_repair() const { return myisam_recover_options != HA_RECOVER_OFF; }
  int optimize(THD *thd, HA_CHECK_OPT *check_opt);
  int assign_to_keycache(THD *thd, HA_CHECK_OPT *check_opt);
  int preload_keys(THD *thd, HA_CHECK_OPT *check_opt);
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);
  MI_INFO *file_ptr(void) { return file; }

 public:
  /**
   * Multi Range Read interface
   */
  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                            uint n_ranges, uint mode, HANDLER_BUFFER *buf);
  int multi_range_read_next(char **range_info);
  ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                      void *seq_init_param, uint n_ranges,
                                      uint *bufsz, uint *flags,
                                      Cost_estimate *cost);
  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                uint *bufsz, uint *flags, Cost_estimate *cost);

  /* Index condition pushdown implementation */
  Item *idx_cond_push(uint keyno, Item *idx_cond);

 private:
  DsMrr_impl ds_mrr;
  friend ICP_RESULT index_cond_func_myisam(void *arg);
};
