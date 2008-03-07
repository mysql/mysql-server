/* Copyright (C) 2003 MySQL AB

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

#include <sys/types.h>
#include <sys/stat.h>
#include <my_dir.h>
#include "transparent_file.h"

#define DEFAULT_CHAIN_LENGTH 512
/*
  Version for file format.
  1 - Initial Version. That is, the version when the metafile was introduced.
*/

#define TINA_VERSION 1

typedef struct st_tina_share {
  char *table_name;
  char data_file_name[FN_REFLEN];
  uint table_name_length, use_count;
  /*
    Below flag is needed to make log tables work with concurrent insert.
    For more details see comment to ha_tina::update_status.
  */
  my_bool is_log_table;
  /*
    Here we save the length of the file for readers. This is updated by
    inserts, updates and deletes. The var is initialized along with the
    share initialization.
  */
  off_t saved_data_file_length;
  pthread_mutex_t mutex;
  THR_LOCK lock;
  bool update_file_opened;
  bool tina_write_opened;
  File meta_file;           /* Meta file we use */
  File tina_write_filedes;  /* File handler for readers */
  bool crashed;             /* Meta file is crashed */
  ha_rows rows_recorded;    /* Number of rows in tables */
  uint data_file_version;   /* Version of the data file used */
} TINA_SHARE;

struct tina_set {
  off_t begin;
  off_t end;
};

class ha_tina: public handler
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  TINA_SHARE *share;       /* Shared lock info */
  off_t current_position;  /* Current position in the file during a file scan */
  off_t next_position;     /* Next position in the file scan */
  off_t local_saved_data_file_length; /* save position for reads */
  off_t temp_file_length;
  uchar byte_buffer[IO_SIZE];
  Transparent_file *file_buff;
  File data_file;                   /* File handler for readers */
  File update_temp_file;
  String buffer;
  /*
    The chain contains "holes" in the file, occured because of
    deletes/updates. It is used in rnd_end() to get rid of them
    in the end of the query.
  */
  tina_set chain_buffer[DEFAULT_CHAIN_LENGTH];
  tina_set *chain;
  tina_set *chain_ptr;
  uchar chain_alloced;
  uint32 chain_size;
  uint local_data_file_version;  /* Saved version of the data file used */
  bool records_is_known;
  MEM_ROOT blobroot;

private:
  bool get_write_pos(off_t *end_pos, tina_set *closest_hole);
  int open_update_temp_file_if_needed();
  int init_tina_writer();
  int init_data_file();

public:
  ha_tina(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_tina()
  {
    if (chain_alloced)
      my_free(chain, 0);
    if (file_buff)
      delete file_buff;
  }
  const char *table_type() const { return "CSV"; }
  const char *index_type(uint inx) { return "NONE"; }
  const char **bas_ext() const;
  ulonglong table_flags() const
  {
    return (HA_NO_TRANSACTIONS | HA_REC_NOT_IN_SEQ | HA_NO_AUTO_INCREMENT |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE);
  }
  ulong index_flags(uint idx, uint part, bool all_parts) const
  {
    /*
      We will never have indexes so this will never be called(AKA we return
      zero)
    */
    return 0;
  }
  uint max_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_keys()          const { return 0; }
  uint max_key_parts()     const { return 0; }
  uint max_key_length()    const { return 0; }
  /*
     Called in test_quick_select to determine if indexes should be used.
   */
  virtual double scan_time() { return (double) (stats.records+stats.deleted) / 20.0+10; }
  /* The next method will never be called */
  virtual bool fast_key_read() { return 1;}
  /* 
    TODO: return actual upper bound of number of records in the table.
    (e.g. save number of records seen on full table scan and/or use file size
    as upper bound)
  */
  ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR; }

  int open(const char *name, int mode, uint open_options);
  int close(void);
  int write_row(uchar * buf);
  int update_row(const uchar * old_data, uchar * new_data);
  int delete_row(const uchar * buf);
  int rnd_init(bool scan=1);
  int rnd_next(uchar *buf);
  int rnd_pos(uchar * buf, uchar *pos);
  bool check_and_repair(THD *thd);
  int check(THD* thd, HA_CHECK_OPT* check_opt);
  bool is_crashed() const;
  int rnd_end();
  int repair(THD* thd, HA_CHECK_OPT* check_opt);
  /* This is required for SQL layer to know that we support autorepair */
  bool auto_repair() const { return 1; }
  void position(const uchar *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int delete_all_rows(void);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  bool check_if_incompatible_data(HA_CREATE_INFO *info,
                                  uint table_changes);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
      enum thr_lock_type lock_type);

  /*
    These functions used to get/update status of the handler.
    Needed to enable concurrent inserts.
  */
  void get_status();
  void update_status();

  /* The following methods were added just for TINA */
  int encode_quote(uchar *buf);
  int find_current_row(uchar *buf);
  int chain_append();
};

