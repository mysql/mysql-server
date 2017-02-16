#ifndef HA_MARIA_INCLUDED
#define HA_MARIA_INCLUDED
/* Copyright (C) 2006,2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                               /* gcc class implementation */
#endif

/* class for the maria handler */

#include <maria.h>
#include "handler.h"
#include "table.h"

#define HA_RECOVER_NONE         0       /* No automatic recover */
#define HA_RECOVER_DEFAULT      1       /* Automatic recover active */
#define HA_RECOVER_BACKUP       2       /* Make a backupfile on recover */
#define HA_RECOVER_FORCE        4       /* Recover even if we loose rows */
#define HA_RECOVER_QUICK        8       /* Don't check rows in data file */

C_MODE_START
ICP_RESULT index_cond_func_maria(void *arg);
C_MODE_END

extern TYPELIB maria_recover_typelib;
extern ulonglong maria_recover_options;

class ha_maria :public handler
{
  MARIA_HA *file;
  ulonglong int_table_flags;
  MARIA_RECORD_POS remember_pos;
  char *data_file_name, *index_file_name;
  enum data_file_type data_file_type;
  bool can_enable_indexes;
  /**
    If a transactional table is doing bulk insert with a single
    UNDO_BULK_INSERT with/without repair. 
  */
  uint8 bulk_insert_single_undo;
  int repair(THD * thd, HA_CHECK *param, bool optimize);
  int zerofill(THD * thd, HA_CHECK_OPT *check_opt);

public:
  ha_maria(handlerton *hton, TABLE_SHARE * table_arg);
  ~ha_maria() {}
  handler *clone(const char *name, MEM_ROOT *mem_root);
  const char *table_type() const
  { return "Aria"; }
  const char *index_type(uint key_number);
  const char **bas_ext() const;
  ulonglong table_flags() const
  { return int_table_flags; }
  ulong index_flags(uint inx, uint part, bool all_parts) const;
  uint max_supported_keys() const
  { return MARIA_MAX_KEY; }
  uint max_supported_key_length() const;
  uint max_supported_key_part_length() const
  { return max_supported_key_length(); }
  enum row_type get_row_type() const;
  uint checksum() const;
  void change_table_ptr(TABLE *table_arg, TABLE_SHARE *share);
  virtual double scan_time();

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(uchar * buf);
  int update_row(const uchar * old_data, uchar * new_data);
  int delete_row(const uchar * buf);
  int index_read_map(uchar * buf, const uchar * key, key_part_map keypart_map,
		     enum ha_rkey_function find_flag);
  int index_read_idx_map(uchar * buf, uint idx, const uchar * key,
			 key_part_map keypart_map,
			 enum ha_rkey_function find_flag);
  int index_read_last_map(uchar * buf, const uchar * key,
			  key_part_map keypart_map);
  int index_next(uchar * buf);
  int index_prev(uchar * buf);
  int index_first(uchar * buf);
  int index_last(uchar * buf);
  int index_next_same(uchar * buf, const uchar * key, uint keylen);
  int ft_init()
  {
    if (!ft_handler)
      return 1;
    ft_handler->please->reinit_search(ft_handler);
    return 0;
  }
  FT_INFO *ft_init_ext(uint flags, uint inx, String * key)
  {
    return maria_ft_init_search(flags, file, inx,
                                (uchar *) key->ptr(), key->length(),
                                key->charset(), table->record[0]);
  }
  int ft_read(uchar * buf);
  int index_init(uint idx, bool sorted);
  int index_end();
  int rnd_init(bool scan);
  int rnd_end(void);
  int rnd_next(uchar * buf);
  int rnd_pos(uchar * buf, uchar * pos);
  int remember_rnd_pos();
  int restart_rnd_next(uchar * buf);
  void position(const uchar * record);
  int info(uint);
  int info(uint, my_bool);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int reset(void);
  int external_lock(THD * thd, int lock_type);
  int start_stmt(THD *thd, thr_lock_type lock_type);
  int delete_all_rows(void);
  int disable_indexes(uint mode);
  int enable_indexes(uint mode);
  int indexes_are_disabled(void);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  ha_rows records_in_range(uint inx, key_range * min_key, key_range * max_key);
  void update_create_info(HA_CREATE_INFO * create_info);
  int create(const char *name, TABLE * form, HA_CREATE_INFO * create_info);
  THR_LOCK_DATA **store_lock(THD * thd, THR_LOCK_DATA ** to,
                             enum thr_lock_type lock_type);
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  int rename_table(const char *from, const char *to);
  int delete_table(const char *name);
  void drop_table(const char *name);
  int check(THD * thd, HA_CHECK_OPT * check_opt);
  int analyze(THD * thd, HA_CHECK_OPT * check_opt);
  int repair(THD * thd, HA_CHECK_OPT * check_opt);
  bool check_and_repair(THD * thd);
  bool is_crashed() const;
  bool is_changed() const;
  bool auto_repair(int error) const
  {
    /* Always auto-repair moved tables (error == HA_ERR_OLD_FILE) */
    return ((test(maria_recover_options & HA_RECOVER_ANY) &&
             error == HA_ERR_CRASHED_ON_USAGE) ||
            error == HA_ERR_OLD_FILE);

  }
  int optimize(THD * thd, HA_CHECK_OPT * check_opt);
  int restore(THD * thd, HA_CHECK_OPT * check_opt);
  int backup(THD * thd, HA_CHECK_OPT * check_opt);
  int assign_to_keycache(THD * thd, HA_CHECK_OPT * check_opt);
  int preload_keys(THD * thd, HA_CHECK_OPT * check_opt);
  bool check_if_incompatible_data(HA_CREATE_INFO * info, uint table_changes);
#ifdef HAVE_REPLICATION
  int dump(THD * thd, int fd);
  int net_read_dump(NET * net);
#endif
#ifdef HAVE_QUERY_CACHE
  my_bool register_query_cache_table(THD *thd, char *table_key,
                                     uint key_length,
                                     qc_engine_callback
                                     *engine_callback,
                                     ulonglong *engine_data);
#endif
  MARIA_HA *file_ptr(void)
  {
    return file;
  }
  static int implicit_commit(THD *thd, bool new_trn);
  /**
   * Multi Range Read interface
   */
  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                            uint n_ranges, uint mode, HANDLER_BUFFER *buf);
  int multi_range_read_next(range_id_t *range_info);
  ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                      void *seq_init_param, 
                                      uint n_ranges, uint *bufsz,
                                      uint *flags, COST_VECT *cost);
  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                uint key_parts, uint *bufsz, 
                                uint *flags, COST_VECT *cost);
  int multi_range_read_explain_info(uint mrr_mode, char *str, size_t size);
  
  /* Index condition pushdown implementation */
  Item *idx_cond_push(uint keyno, Item* idx_cond);
private:
  DsMrr_impl ds_mrr;
  friend ICP_RESULT index_cond_func_maria(void *arg);
};

#endif /* HA_MARIA_INCLUDED */
