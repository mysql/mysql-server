/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

/* class for the the myisam handler */

#include <myisam.h>
#include <ft_global.h>

#define HA_RECOVER_NONE		0	// No automatic recover
#define HA_RECOVER_DEFAULT	1	// Automatic recover active
#define HA_RECOVER_BACKUP	2	// Make a backupfile on recover
#define HA_RECOVER_FORCE	4	// Recover even if we loose rows
#define HA_RECOVER_QUICK	8	// Don't check rows in data file

extern ulong myisam_sort_buffer_size;
extern TYPELIB myisam_recover_typelib;
extern ulong myisam_recover_options;

class ha_myisam: public handler
{
  MI_INFO *file;
  uint    int_option_flag;
  char    *data_file_name, *index_file_name;
  int repair(THD *thd, MI_CHECK &param, bool optimize);

 public:
  ha_myisam(TABLE *table): handler(table), file(0),
    int_option_flag(HA_READ_NEXT | HA_READ_PREV | HA_READ_RND_SAME |
		    HA_KEYPOS_TO_RNDPOS | HA_READ_ORDER |  HA_LASTKEY_ORDER |
		    HA_HAVE_KEY_READ_ONLY | HA_READ_NOT_EXACT_KEY |
		    HA_LONGLONG_KEYS |  HA_NULL_KEY |
		    HA_DUPP_POS | HA_BLOB_KEY | HA_AUTO_PART_KEY)
  {}
  ~ha_myisam() {}
  const char *table_type() const { return "MyISAM"; }
  const char **bas_ext() const;
  ulong option_flag() const { return int_option_flag; }
  uint max_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_keys()          const { return MI_MAX_KEY; }
  uint max_key_parts()     const { return MAX_REF_PARTS; }
  uint max_key_length()    const { return MAX_KEY_LENGTH; }

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint idx, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_next(byte * buf);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int index_next_same(byte *buf, const byte *key, uint keylen);
  int index_end() { ft_handler=NULL; return 0; }
  int ft_init()
         { if(!ft_handler) return 1; ft_reinit_search(ft_handler); return 0; }
  void *ft_init_ext(uint inx,const byte *key, uint keylen, bool presort)
               { return ft_init_search(file,inx,(byte*) key,keylen,presort); }
  int ft_read(byte *buf);
  int rnd_init(bool scan=1);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  int restart_rnd_next(byte *buf, byte *pos);
  void position(const byte *record);
  my_off_t row_position() { return mi_position(file); }
  void info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int external_lock(THD *thd, int lock_type);
  int delete_all_rows(void);
  void deactivate_non_unique_index(ha_rows rows);
  bool activate_all_index(THD *thd);
  ha_rows records_in_range(int inx,
			   const byte *start_key,uint start_key_len,
			   enum ha_rkey_function start_search_flag,
			   const byte *end_key,uint end_key_len,
			   enum ha_rkey_function end_search_flag);
  void update_create_info(HA_CREATE_INFO *create_info);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  longlong get_auto_increment();
  int rename_table(const char * from, const char * to);
  int delete_table(const char *name);
  int check(THD* thd, HA_CHECK_OPT* check_opt);
  int analyze(THD* thd,HA_CHECK_OPT* check_opt);
  int repair(THD* thd, HA_CHECK_OPT* check_opt);
  bool check_and_repair(THD *thd);
  bool is_crashed() const;
  bool auto_repair() const { return myisam_recover_options != 0; }
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int restore(THD* thd, HA_CHECK_OPT* check_opt);
  int backup(THD* thd, HA_CHECK_OPT* check_opt);
  int dump(THD* thd, int fd);
  int net_read_dump(NET* net);
};
