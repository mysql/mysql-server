/* Copyright (C) 2003 MySQL AB

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

#include <sys/types.h>
#include <sys/stat.h>
#include <my_dir.h>

#define DEFAULT_CHAIN_LENGTH 512

typedef struct st_tina_share {
  char *table_name;
  byte *mapped_file;                /* mapped region of file */
  uint table_name_length,use_count;
  MY_STAT file_stat;                /* Stat information for the data file */
  File data_file;                   /* Current open data file */
  pthread_mutex_t mutex;
  THR_LOCK lock;
} TINA_SHARE;

typedef struct tina_set {
	off_t begin;
	off_t end;
};

class ha_tina: public handler
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  TINA_SHARE *share;       /* Shared lock info */
  off_t current_position;  /* Current position in the file during a file scan */
  off_t next_position;     /* Next position in the file scan */
  byte byte_buffer[IO_SIZE];
  String buffer;
  tina_set chain_buffer[DEFAULT_CHAIN_LENGTH];
  tina_set *chain;
  tina_set *chain_ptr;
  byte chain_alloced;
  uint32 chain_size;

  public:
  ha_tina(TABLE *table): handler(table),
  /* 
     These definitions are found in hanler.h 
     Theses are not probably completely right.
   */
  current_position(0), next_position(0), chain_alloced(0), chain_size(DEFAULT_CHAIN_LENGTH)
  {
    /* Set our original buffers from pre-allocated memory */
    buffer.set(byte_buffer, IO_SIZE, system_charset_info);
    chain = chain_buffer;
  }
  ~ha_tina() 
  {
    if (chain_alloced)
      my_free((gptr)chain,0);
  }
  const char *table_type() const { return "CSV"; }
  const char *index_type(uint inx) { return "NONE"; }
  const char **bas_ext() const;
  ulong table_flags() const
  {
    return (HA_REC_NOT_IN_SEQ | HA_NOT_EXACT_COUNT | 
      HA_NO_AUTO_INCREMENT );
  }
  ulong index_flags(uint idx, uint part, bool all_parts) const
  {
    /* We will never have indexes so this will never be called(AKA we return zero) */
    return 0;
  }
  uint max_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_keys()          const { return 0; }
  uint max_key_parts()     const { return 0; }
  uint max_key_length()    const { return 0; }
  /*
     Called in test_quick_select to determine if indexes should be used.
   */
  virtual double scan_time() { return (double) (records+deleted) / 20.0+10; }
  /* The next method will never be called */
  virtual double read_time(ha_rows rows) { DBUG_ASSERT(0); return((double) rows /  20.0+1); }
  virtual bool fast_key_read() { return 1;}

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
  int rnd_init(bool scan=1);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  int rnd_end();
  void position(const byte *record);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int external_lock(THD *thd, int lock_type);
  int delete_all_rows(void);
  ha_rows records_in_range(int inx, const byte *start_key,uint start_key_len,
      enum ha_rkey_function start_search_flag,
      const byte *end_key,uint end_key_len,
      enum ha_rkey_function end_search_flag);
//  int delete_table(const char *from);
//  int rename_table(const char * from, const char * to);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
      enum thr_lock_type lock_type);

  /* The following methods were added just for TINA */
  int encode_quote(byte *buf);
  int find_current_row(byte *buf);
  int chain_append();
};
