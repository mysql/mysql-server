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

#include <db.h>

typedef struct st_berkeley_share {
  ulonglong auto_ident;
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
  my_bool inited;
} BDB_SHARE;

class ha_berkeley: public handler
{
  THR_LOCK_DATA lock;
  DBT last_key,current_row;
  gptr alloc_ptr;
  byte *rec_buff;
  char *key_buff, *key_buff2, *primary_key_buff;
  DB *file, **key_file;
  DB_TXN *transaction;
  u_int32_t *key_type;
  DBC *cursor;
  BDB_SHARE *share;
  ulong int_option_flag;
  ulong alloced_rec_buff_length;
  uint primary_key,last_dup_key;
  bool fixed_length_row, fixed_length_primary_key, hidden_primary_key;

  bool  fix_rec_buff_for_blob(ulong length);
  ulong max_row_length(const byte *buf);
  int pack_row(DBT *row,const  byte *record);
  void unpack_row(char *record, DBT *row);
  DBT *pack_key(DBT *key, uint keynr, char *buff, const byte *record);
  DBT *pack_key(DBT *key, uint keynr, char *buff, const byte *key_ptr,
		uint key_length);
  int remove_key(DB_TXN *trans, uint keynr, const byte *record,
		 DBT *packed_record, DBT *prim_key);
  int remove_keys(DB_TXN *trans,const byte *record, DBT *new_record,
		  DBT *prim_key, key_map keys, int result);
  int key_cmp(uint keynr, const byte * old_row, const byte * new_row);
  int update_primary_key(DB_TXN *trans, bool primary_key_changed,
			 const byte * old_row, const byte * new_row,
			 DBT *prim_key);
  int read_row(int error, char *buf, uint keynr, DBT *row, bool);
  DBT *get_pos(DBT *to, byte *pos);

 public:
  ha_berkeley(TABLE *table): handler(table), alloc_ptr(0),rec_buff(0), file(0),
    int_option_flag(HA_READ_NEXT | HA_READ_PREV |
		    HA_REC_NOT_IN_SEQ |
		    HA_KEYPOS_TO_RNDPOS | HA_READ_ORDER | HA_LASTKEY_ORDER |
		    HA_LONGLONG_KEYS | HA_NULL_KEY |
		    HA_BLOB_KEY | 
		    HA_REQUIRE_PRIMARY_KEY | HA_NOT_EXACT_COUNT | 
		    HA_PRIMARY_KEY_IN_READ_INDEX | HA_DROP_BEFORE_CREATE),
    last_dup_key((uint) -1)
  {
  }
  ~ha_berkeley() {}
  const char *table_type() const { return "BerkeleyDB"; }
  const char **bas_ext() const;
  ulong option_flag() const { return int_option_flag; }
  uint max_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_keys()          const { return MAX_KEY; }
  uint max_key_parts()     const { return MAX_REF_PARTS; }
  uint max_key_length()    const { return MAX_KEY_LENGTH; }
  bool fast_key_read()	   { return 1;}
  bool has_transactions()  { return 1;}

  int open(const char *name, int mode, uint test_if_locked);
  void initialize(void);
  int close(void);
  double scan_time();
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint index, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_next(byte * buf);
  int index_next_same(byte * buf, const byte *key, uint keylen);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int rnd_init(bool scan=1);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  void position(const byte *record);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int external_lock(THD *thd, int lock_type);
  void position(byte *record);
  ha_rows records_in_range(int inx,
			   const byte *start_key,uint start_key_len,
			   enum ha_rkey_function start_search_flag,
			   const byte *end_key,uint end_key_len,
			   enum ha_rkey_function end_search_flag);

  int create(const char *name, register TABLE *form,
	     HA_CREATE_INFO *create_info);
  int delete_table(const char *name);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
};

extern bool berkeley_skip;
extern u_int32_t berkeley_init_flags,berkeley_lock_type,berkeley_lock_types[];
extern ulong berkeley_cache_size;
extern char *berkeley_home, *berkeley_tmpdir, *berkeley_logdir;
extern long berkeley_lock_scan_time;
extern TYPELIB berkeley_lock_typelib;

bool berkeley_init(void);
bool berkeley_end(void);
bool berkeley_flush_logs(void);
int berkeley_commit(THD *thd);
int berkeley_rollback(THD *thd);
