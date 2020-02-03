/*  Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#ifndef WarpHeader
#define WarpHeader
#include <sys/stat.h>
#include <sys/types.h>

#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "sql/handler.h"
#include "sql_string.h"

#include <fcntl.h>
#include <mysql/plugin.h>
#include <mysql/psi/mysql_file.h>
#include <algorithm>

#include "map_helpers.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_psi_config.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_memory.h"
#include "sql/field.h"
#include "sql/sql_class.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "template_utils.h"


#include <fstream>  
#include <iostream>  
#include <string> 
#include <atomic>
#include <vector>

#include "include/fastbit/ibis.h"
#include "include/fastbit/query.h"
#include "include/fastbit/bundle.h"
#include "include/fastbit/tafel.h"
#include "include/fastbit/array_t.h"
#include "include/fastbit/mensa.h"
#include "include/fastbit/resource.h"
#include "include/fastbit/util.h"

/*
  Version for file format.
  1 - Initial Version. That is, the version when the metafile was introduced.
*/

#define WARP_VERSION 1
/* 1 million rows per partition is small, but I want to stress test the 
   database with lots of partitions to start
*/
#define WARP_PARTITION_MAX_ROWS 1000000
#define BLOB_MEMROOT_ALLOC_SIZE 8192

struct WARP_SHARE {
  std::string table_name;
  uint table_name_length, use_count;
  char data_dir_name[FN_REFLEN];
    
  mysql_mutex_t mutex;
  THR_LOCK lock;
};

static WARP_SHARE *get_share(const char *table_name, TABLE* table_ptr);
static int free_share(WARP_SHARE *share); 

struct COLUMN_INFO {
  std::string name;
  int datatype;
  bool is_unsigned;
};

class ha_warp : public handler {
  THR_LOCK_DATA lock; /* MySQL lock */
  WARP_SHARE *share;  /* Shared lock info */
  bool records_is_known;
  
 private:
  void update_row_count();
  int reset_table();
  int encode_quote(uchar *buf);
  int set_column_set(); 
  int set_column_set(uint32_t idxno);
  int find_current_row(uchar *buf);
  void create_writer(TABLE *table_arg);

  /* These objects are used to access the FastBit tables for tuple reads.*/ 
  ibis::mensa*         base_table; 
  ibis::table*         filtered_table;
  ibis::table::cursor* cursor;

  /* This object is used to append tuples to the table */
  ibis::tablex*         writer;

  /* A list of row numbers to delete (filled in by delete_row) */
  std::vector<uint64_t> deleted_rows;

  /* this is always the rowid of the current row */
  uint64_t current_rowid;  

  /* a SELECT lists of the columns that have been fetched for the current query */
  std::string column_set;
  std::string index_column_set;

  /* temporary buffer populated with CSV of row for insertions*/
  String buffer;

  /* storage for BLOBS */
  MEM_ROOT blobroot; 
 public:
  ha_warp(handlerton *hton, TABLE_SHARE *table_arg);
 
  ~ha_warp() {
    //free_root(&blobroot, MYF(0));
  }
 
  const char *table_type() const { return "WARP"; }
 
  ulonglong table_flags() const {
    // return (HA_NO_TRANSACTIONS | HA_NO_AUTO_INCREMENT | HA_BINLOG_ROW_CAPABLE | HA_CAN_REPAIR);
    return (HA_NO_TRANSACTIONS | HA_BINLOG_ROW_CAPABLE | HA_CAN_REPAIR);
  }
 
  uint max_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_keys() const { return 1000; }
  uint max_key_parts() const { return 1000; }
  uint max_key_length() const { return HA_MAX_REC_LENGTH; }
  uint max_supported_keys() const { return 16384; }
  uint max_supported_key_length() const { return 1024; }
  uint max_supported_key_part_length(
      HA_CREATE_INFO *create_info MY_ATTRIBUTE((unused))) const {
    return 1024;
  }

  /*
     Called in test_quick_select to determine if indexes should be used.
   */
  virtual double scan_time() {
    return (double)(stats.records + stats.deleted) / 20.0 + 10;
  }
  /* The next method will never be called */
  virtual bool fast_key_read() { return 1; }
  ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR; }

  int open(const char *name, int mode, uint open_options,
           const dd::Table *table_def);
  int close(void);

  int write_row(uchar *buf);
  int update_row(const uchar *old_data, uchar *new_data);
  int delete_row(const uchar *buf);
  int rnd_init(bool scan = 1);
  int rnd_next(uchar *buf);
  int rnd_pos(uchar *buf, uchar *pos);
  bool check_and_repair(THD *thd);
  int check(THD *thd, HA_CHECK_OPT *check_opt);
  bool is_crashed() const;
  int rnd_end();
  int repair(THD *thd, HA_CHECK_OPT *check_opt);
  /* This is required for SQL layer to know that we support autorepair */
  bool auto_repair() const { return 1; }
  void position(const uchar *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  //int delete_all_rows(void);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info,
             dd::Table *table_def);
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);
  int delete_table(const char *table_name, const dd::Table *);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  /*
    These functions used to get/update status of the handler.
    Needed to enable concurrent inserts.
  */
  void get_status();
  void update_status();

  // Functions to support indexing
  ulong index_flags(uint, uint, bool) const;
  ha_rows records_in_range(uint idxno, key_range *, key_range *); 
  int index_init(uint idxno, bool sorted);
  int index_init(uint idxno);
  int index_next(uchar * buf);
  int index_first(uchar * buf);
  int index_end();
  int index_read_map (uchar *buf, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag);
  int index_read_idx_map (uchar *buf, uint idxno, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag);
  int make_where_clause(const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag, std::string& where_clause, uint32_t idxno=0);
  void get_auto_increment(ulonglong, ulonglong, ulonglong, ulonglong *, ulonglong *);

};
#endif
