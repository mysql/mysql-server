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

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#include <zlib.h>

/*
  Please read ha_archive.cc first. If you are looking for more general
  answers on how storage engines work, look at ha_example.cc and
  ha_example.h.
*/

typedef struct st_archive_share {
  char *table_name;
  char data_file_name[FN_REFLEN];
  uint table_name_length,use_count;
  pthread_mutex_t mutex;
  THR_LOCK lock;
  File meta_file;                   /* Meta file we use */
  gzFile archive_write;             /* Archive file we are working with */
  bool dirty;                       /* Flag for if a flush should occur */
  ulonglong rows_recorded;          /* Number of rows in tables */
} ARCHIVE_SHARE;

/*
  Version for file format.
  1 - Initial Version
*/
#define ARCHIVE_VERSION 1

class ha_archive: public handler
{
  THR_LOCK_DATA lock;        /* MySQL lock */
  ARCHIVE_SHARE *share;      /* Shared lock info */
  gzFile archive;            /* Archive file we are working with */
  z_off_t current_position;  /* The position of the row we just read */
  byte byte_buffer[IO_SIZE]; /* Initial buffer for our string */
  String buffer;             /* Buffer used for blob storage */
  ulonglong scan_rows;       /* Number of rows left in scan */

public:
  ha_archive(TABLE *table): handler(table)
  {
    /* Set our original buffer from pre-allocated memory */
    buffer.set(byte_buffer, IO_SIZE, system_charset_info);

    /* The size of the offset value we will use for position() */
    ref_length = sizeof(z_off_t);
  }
  ~ha_archive()
  {
  }
  const char *table_type() const { return "ARCHIVE"; }
  const char *index_type(uint inx) { return "NONE"; }
  const char **bas_ext() const;
  ulong table_flags() const
  {
    return (HA_REC_NOT_IN_SEQ | HA_NOT_EXACT_COUNT | HA_NO_AUTO_INCREMENT |
            HA_FILE_BASED);
  }
  ulong index_flags(uint idx, uint part, bool all_parts) const
  {
    return 0;
  }
  /*
    Have to put something here, there is no real limit as far as
    archive is concerned.
  */
  uint max_supported_record_length() const { return UINT_MAX; }
  /*
    Called in test_quick_select to determine if indexes should be used.
  */
  virtual double scan_time() { return (double) (records) / 20.0+10; }
  /* The next method will never be called */
  virtual double read_time(uint index, uint ranges, ha_rows rows)
  { return (double) rows /  20.0+1; }
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
  int get_row(gzFile file_to_read, byte *buf);
  int read_meta_file(File meta_file, ulonglong *rows);
  int write_meta_file(File meta_file, ulonglong rows, bool dirty);
  ARCHIVE_SHARE *get_share(const char *table_name, TABLE *table);
  int free_share(ARCHIVE_SHARE *share);
  int rebuild_meta_file(char *table_name, File meta_file);
  int read_data_header(gzFile file_to_read);
  int write_data_header(gzFile file_to_write);
  void position(const byte *record);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int external_lock(THD *thd, int lock_type);
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
};
