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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include <zlib.h>
#include "azlib.h"

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
  File meta_file;           /* Meta file we use */
  azio_stream archive_write;     /* Archive file we are working with */
  bool archive_write_open;
  bool dirty;               /* Flag for if a flush should occur */
  bool crashed;             /* Meta file is crashed */
  ha_rows rows_recorded;    /* Number of rows in tables */
  ulonglong auto_increment_value;
  ulonglong forced_flushes;
  ulonglong mean_rec_length;
  char real_path[FN_REFLEN];
} ARCHIVE_SHARE;

/*
  Version for file format.
  1 - Initial Version
*/
#define ARCHIVE_VERSION 2

class ha_archive: public handler
{
  THR_LOCK_DATA lock;        /* MySQL lock */
  ARCHIVE_SHARE *share;      /* Shared lock info */
  azio_stream archive;            /* Archive file we are working with */
  my_off_t current_position;  /* The position of the row we just read */
  byte byte_buffer[IO_SIZE]; /* Initial buffer for our string */
  String buffer;             /* Buffer used for blob storage */
  ha_rows scan_rows;         /* Number of rows left in scan */
  bool delayed_insert;       /* If the insert is delayed */
  bool bulk_insert;          /* If we are performing a bulk insert */
  const byte *current_key;
  uint current_key_len;
  uint current_k_offset;

public:
  ha_archive(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_archive()
  {
  }
  const char *table_type() const { return "ARCHIVE"; }
  const char *index_type(uint inx) { return "NONE"; }
  const char **bas_ext() const;
  ulonglong table_flags() const
  {
    return (HA_NO_TRANSACTIONS | HA_REC_NOT_IN_SEQ | HA_CAN_BIT_FIELD |
            HA_FILE_BASED | HA_CAN_INSERT_DELAYED | HA_CAN_GEOMETRY);
  }
  ulong index_flags(uint idx, uint part, bool all_parts) const
  {
    return HA_ONLY_WHOLE_INDEX;
  }
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  uint max_supported_keys()          const { return 1; }
  uint max_supported_key_length()    const { return sizeof(ulonglong); }
  uint max_supported_key_part_length() const { return sizeof(ulonglong); }
  int index_init(uint keynr, bool sorted);
  virtual int index_read(byte * buf, const byte * key,
			 uint key_len, enum ha_rkey_function find_flag);
  virtual int index_read_idx(byte * buf, uint index, const byte * key,
			     uint key_len, enum ha_rkey_function find_flag);
  int index_next(byte * buf);
  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(byte * buf);
  int real_write_row(byte *buf, azio_stream *writer);
  int delete_all_rows();
  int rnd_init(bool scan=1);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  int get_row(azio_stream *file_to_read, byte *buf);
  int read_meta_file(File meta_file, ha_rows *rows, 
                     ulonglong *auto_increment,
                     ulonglong *forced_flushes,
                     char *real_path);
  int write_meta_file(File meta_file, ha_rows rows, 
                      ulonglong auto_increment, 
                      ulonglong forced_flushes, 
                      char *real_path,
                      bool dirty);
  ARCHIVE_SHARE *get_share(const char *table_name, TABLE *table, int *rc);
  int free_share(ARCHIVE_SHARE *share);
  int init_archive_writer();
  bool auto_repair() const { return 1; } // For the moment we just do this
  int read_data_header(azio_stream *file_to_read);
  int write_data_header(azio_stream *file_to_write);
  void position(const byte *record);
  void info(uint);
  void update_create_info(HA_CREATE_INFO *create_info);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int repair(THD* thd, HA_CHECK_OPT* check_opt);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  enum row_type get_row_type() const 
  { 
    return ROW_TYPE_COMPRESSED;
  }
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
  bool is_crashed() const;
  int check(THD* thd, HA_CHECK_OPT* check_opt);
  bool check_and_repair(THD *thd);
};

