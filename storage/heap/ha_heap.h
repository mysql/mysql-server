/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/* class for the the heap handler */

#include <heap.h>
#include "sql_class.h"                          /* THD */

class ha_heap: public handler
{
  HP_INFO *file;
  HP_SHARE *internal_share;
  key_map btree_keys;
  /* number of records changed since last statistics update */
  uint    records_changed;
  uint    key_stat_version;
  my_bool internal_table;
public:
  ha_heap(handlerton *hton, TABLE_SHARE *table);
  ~ha_heap() {}
  handler *clone(const char *name, MEM_ROOT *mem_root);
  const char *table_type() const
  {
    return (table->in_use->variables.sql_mode & MODE_MYSQL323) ?
           "HEAP" : "MEMORY";
  }
  const char *index_type(uint inx)
  {
    return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_BTREE) ?
            "BTREE" : "HASH");
  }
  /* Rows also use a fixed-size format */
  enum row_type get_row_type() const { return ROW_TYPE_FIXED; }
  const char **bas_ext() const;
  ulonglong table_flags() const
  {
    return (HA_FAST_KEY_READ | HA_NO_BLOBS | HA_NULL_IN_KEY |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
            HA_REC_NOT_IN_SEQ | HA_CAN_INSERT_DELAYED | HA_NO_TRANSACTIONS |
            HA_HAS_RECORDS | HA_STATS_RECORDS_IS_EXACT);
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_BTREE) ?
            HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_READ_RANGE :
            HA_ONLY_WHOLE_INDEX | HA_KEY_SCAN_NOT_ROR);
  }
  const key_map *keys_to_use_for_scanning() { return &btree_keys; }
  uint max_supported_keys()          const { return MAX_KEY; }
  uint max_supported_key_part_length() const { return MAX_KEY_LENGTH; }
  double scan_time()
  { return (double) (stats.records+stats.deleted) / 20.0+10; }
  double read_time(uint index, uint ranges, ha_rows rows)
  { return (double) rows /  20.0+1; }

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  void set_keys_for_scanning(void);
  int write_row(uchar * buf);
  int update_row(const uchar * old_data, uchar * new_data);
  int delete_row(const uchar * buf);
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  int index_read_map(uchar * buf, const uchar * key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_last_map(uchar *buf, const uchar *key, key_part_map keypart_map);
  int index_read_idx_map(uchar * buf, uint index, const uchar * key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_next(uchar * buf);
  int index_prev(uchar * buf);
  int index_first(uchar * buf);
  int index_last(uchar * buf);
  int rnd_init(bool scan);
  int rnd_next(uchar *buf);
  int rnd_pos(uchar * buf, uchar *pos);
  void position(const uchar *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int reset();
  int external_lock(THD *thd, int lock_type);
  int delete_all_rows(void);
  int truncate();
  int reset_auto_increment(ulonglong value);
  int disable_indexes(uint mode);
  int enable_indexes(uint mode);
  int indexes_are_disabled(void);
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int delete_table(const char *from);
  void drop_table(const char *name);
  int rename_table(const char * from, const char * to);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  void update_create_info(HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  int cmp_ref(const uchar *ref1, const uchar *ref2)
  {
    return memcmp(ref1, ref2, sizeof(HEAP_PTR));
  }
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);
private:
  void update_key_stats();
};
