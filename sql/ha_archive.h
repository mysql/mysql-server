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
  gzFile archive_write;     /* Archive file we are working with */
  bool dirty;               /* Flag for if a flush should occur */
  bool crashed;             /* Meta file is crashed */
  ha_rows rows_recorded;    /* Number of rows in tables */
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
  ha_rows scan_rows;         /* Number of rows left in scan */
  bool delayed_insert;       /* If the insert is delayed */
  bool bulk_insert;          /* If we are performing a bulk insert */

public:
  ha_archive(TABLE *table_arg);
  ~ha_archive()
  {
  }
  const char *table_type() const { return "ARCHIVE"; }
  const char *index_type(uint inx) { return "NONE"; }
  const char **bas_ext() const;
  ulong table_flags() const
  {
    return (HA_REC_NOT_IN_SEQ | HA_NOT_EXACT_COUNT | HA_NO_AUTO_INCREMENT |
            HA_FILE_BASED | HA_CAN_INSERT_DELAYED);
  }
  ulong index_flags(uint idx, uint part, bool all_parts) const
  {
    return 0;
  }
  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(byte * buf);
  int real_write_row(byte *buf, gzFile writer);
  int delete_all_rows();
  int rnd_init(bool scan=1);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  int get_row(gzFile file_to_read, byte *buf);
  int read_meta_file(File meta_file, ha_rows *rows);
  int write_meta_file(File meta_file, ha_rows rows, bool dirty);
  ARCHIVE_SHARE *get_share(const char *table_name, TABLE *table);
  int free_share(ARCHIVE_SHARE *share);
  bool auto_repair() const { return 1; } // For the moment we just do this
  int read_data_header(gzFile file_to_read);
  int write_data_header(gzFile file_to_write);
  void position(const byte *record);
  void info(uint);
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

bool archive_db_init(void);
bool archive_db_end(void);

