/* Copyright (c) 2003, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <sys/types.h>
#include <zlib.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "sql/handler.h"
#include "sql_string.h"
#include "storage/archive/azlib.h"

/**
  @file storage/archive/ha_archive.h
  Archive storage engine.
*/

/*
  Please read ha_archive.cc first. If you are looking for more general
  answers on how storage engines work, look at ha_example.cc and
  ha_example.h.
*/

struct archive_record_buffer {
  uchar *buffer;
  uint32 length;
};

class Archive_share : public Handler_share {
 public:
  mysql_mutex_t mutex;
  THR_LOCK lock;
  azio_stream archive_write; /* Archive file we are working with */
  ha_rows rows_recorded;     /* Number of rows in tables */
  char table_name[FN_REFLEN];
  char data_file_name[FN_REFLEN];
  bool in_optimize;
  bool archive_write_open;
  bool dirty;   /* Flag for if a flush should occur */
  bool crashed; /* Meta file is crashed */
  Archive_share();
  ~Archive_share() override {
    DBUG_PRINT("ha_archive", ("~Archive_share: %p", this));
    if (archive_write_open) {
      mysql_mutex_lock(&mutex);
      (void)close_archive_writer();
      mysql_mutex_unlock(&mutex);
    }
    thr_lock_delete(&lock);
    mysql_mutex_destroy(&mutex);
  }
  int init_archive_writer();
  void close_archive_writer();
  int write_v1_metafile();
  int read_v1_metafile();
};

/*
  Version for file format.
  1 - Initial Version (Never Released)
  2 - Stream Compression, separate blobs, no packing
  3 - One steam (row and blobs), with packing
*/
#define ARCHIVE_VERSION 3

class ha_archive : public handler {
  THR_LOCK_DATA lock;   /* MySQL lock */
  Archive_share *share; /* Shared lock info */

  azio_stream archive;        /* Archive file we are working with */
  my_off_t current_position;  /* The position of the row we just read */
  uchar byte_buffer[IO_SIZE]; /* Initial buffer for our string */
  String buffer;              /* Buffer used for blob storage */
  ha_rows scan_rows;          /* Number of rows left in scan */
  bool bulk_insert;           /* If we are performing a bulk insert */
  const uchar *current_key;
  uint current_key_len;
  uint current_k_offset;
  archive_record_buffer *record_buffer;
  bool archive_reader_open;

  archive_record_buffer *create_record_buffer(unsigned int length);
  void destroy_record_buffer(archive_record_buffer *r);
  unsigned int pack_row_v1(uchar *record);

 public:
  ha_archive(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_archive() override = default;
  const char *table_type() const override { return "ARCHIVE"; }
  ulonglong table_flags() const override {
    return (HA_NO_TRANSACTIONS | HA_CAN_BIT_FIELD | HA_BINLOG_ROW_CAPABLE |
            HA_BINLOG_STMT_CAPABLE | HA_STATS_RECORDS_IS_EXACT |
            HA_COUNT_ROWS_INSTANT | HA_CAN_REPAIR | HA_FILE_BASED |
            HA_CAN_GEOMETRY | HA_UPDATE_NOT_SUPPORTED |
            HA_DELETE_NOT_SUPPORTED);
  }
  ulong index_flags(uint, uint, bool) const override {
    return HA_ONLY_WHOLE_INDEX;
  }
  void get_auto_increment(ulonglong offset, ulonglong increment,
                          ulonglong nb_desired_values, ulonglong *first_value,
                          ulonglong *nb_reserved_values) override;
  uint max_supported_keys() const override { return 1; }
  uint max_supported_key_length() const override { return sizeof(ulonglong); }
  uint max_supported_key_part_length(HA_CREATE_INFO *create_info
                                     [[maybe_unused]]) const override {
    return sizeof(ulonglong);
  }
  int records(ha_rows *num_rows) override {
    *num_rows = share->rows_recorded;
    return 0;
  }
  int index_init(uint keynr, bool sorted) override;
  int index_read(uchar *buf, const uchar *key, uint key_len,
                 enum ha_rkey_function find_flag) override;
  virtual int index_read_idx(uchar *buf, uint index, const uchar *key,
                             uint key_len, enum ha_rkey_function find_flag);
  int index_next(uchar *buf) override;
  int open(const char *name, int mode, uint test_if_locked,
           const dd::Table *table_def) override;
  int close(void) override;
  int write_row(uchar *buf) override;
  int real_write_row(uchar *buf, azio_stream *writer);
  int truncate(dd::Table *table_def) override;
  int rnd_init(bool scan = true) override;
  int rnd_next(uchar *buf) override;
  int rnd_pos(uchar *buf, uchar *pos) override;
  int get_row(azio_stream *file_to_read, uchar *buf);
  int get_row_version2(azio_stream *file_to_read, uchar *buf);
  int get_row_version3(azio_stream *file_to_read, uchar *buf);
  Archive_share *get_share(const char *table_name, int *rc);
  int init_archive_reader();
  bool auto_repair() const override {
    return true;
  }  // For the moment we just do this
  int read_data_header(azio_stream *file_to_read);
  void position(const uchar *record) override;
  int info(uint) override;
  int extra(enum ha_extra_function operation) override;
  void update_create_info(HA_CREATE_INFO *create_info) override;
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info,
             dd::Table *table_def) override;
  int optimize(THD *thd, HA_CHECK_OPT *check_opt) override;
  int repair(THD *thd, HA_CHECK_OPT *check_opt) override;
  void start_bulk_insert(ha_rows rows) override;
  int end_bulk_insert() override;
  enum row_type get_real_row_type(const HA_CREATE_INFO *) const override {
    return ROW_TYPE_COMPRESSED;
  }
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type) override;
  bool is_crashed() const override;
  int check_for_upgrade(HA_CHECK_OPT *check_opt) override;
  int check(THD *thd, HA_CHECK_OPT *check_opt) override;
  bool check_and_repair(THD *thd) override;
  uint32 max_row_length(const uchar *buf);
  bool fix_rec_buff(unsigned int length);
  int unpack_row(azio_stream *file_to_read, uchar *record);
  unsigned int pack_row(uchar *record, azio_stream *writer);
  bool check_if_incompatible_data(HA_CREATE_INFO *info,
                                  uint table_changes) override;
};
