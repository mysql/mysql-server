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
#ifndef HA_WARP_HDR
#define HA_WARP_HDR
#define MYSQL_SERVER 1
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"

#include <sys/stat.h>
#include <sys/types.h>

#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "sql/handler.h"
#include "sql_string.h"
#include "sql/dd/dd.h"
#include "sql/dd/dd_table.h"
#include "sql/dd/dd_schema.h"
#include "sql/abstract_query_plan.h"

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
#include <thread>

#include "include/fastbit/ibis.h"
#include "include/fastbit/query.h"
#include "include/fastbit/bundle.h"
#include "include/fastbit/tafel.h"
#include "include/fastbit/array_t.h"
#include "include/fastbit/mensa.h"
#include "include/fastbit/resource.h"
#include "include/fastbit/util.h"

#include "sparse.hpp"

/*
  Version for file format.
  1 - Initial Version. That is, the version when the metafile was introduced.
*/

#define WARP_VERSION 1
/* 1 million rows per partition is small, but I want to stress test the 
   database with lots of partitions to start
*/
#define BLOB_MEMROOT_ALLOC_SIZE 8192

/* engine variables */
static unsigned long long my_partition_max_rows, my_cache_size, my_write_cache_size;

static MYSQL_SYSVAR_ULONGLONG(
  partition_max_rows,
  my_partition_max_rows,
  PLUGIN_VAR_RQCMDARG,
  "The maximum number of rows in a Fastbit partition.  An entire partition must fit in the cache.",
  NULL,
  NULL,
  1024 * 1024,
  1024 * 1024,
  1ULL<<63,
  0
);

static MYSQL_SYSVAR_ULONGLONG(
  cache_size,
  my_cache_size,
  PLUGIN_VAR_RQCMDARG,
  "Fastbit file cache size",
  NULL,
  NULL,
  1024ULL * 1024 * 1024 * 4,
  1024ULL * 1024 * 1024 * 4,
  1ULL<<63,
  0
);

static MYSQL_SYSVAR_ULONGLONG(
  write_cache_size,
  my_write_cache_size,
  PLUGIN_VAR_RQCMDARG,
  "The number of rows to cache in ram before flushing to disk.",
  NULL,
  NULL,
  1024 * 1024,
  1024 * 1024,
  1ULL<<63,
  0
);

SYS_VAR* system_variables[] = {
  MYSQL_SYSVAR(partition_max_rows),
  MYSQL_SYSVAR(cache_size),
  MYSQL_SYSVAR(write_cache_size),
  NULL
};

struct WARP_SHARE {
  std::string table_name;
  uint table_name_length, use_count;
  char data_dir_name[FN_REFLEN];
  uint64_t next_rowid = 0;  
  mysql_mutex_t mutex;
  THR_LOCK lock;
};

static WARP_SHARE *get_share(const char *table_name, TABLE* table_ptr);
static int free_share(WARP_SHARE *share); 

class ha_warp : public handler {
  /* MySQL lock - Fastbit has its own internal mutex implementation.  This is used to protect the share.*/
  THR_LOCK_DATA lock; 

  /* Shared lock info */
  WARP_SHARE *share;  
  
  
 private:
  void update_row_count();
  int reset_table();
  int encode_quote(uchar *buf);
  int set_column_set(); 
  int set_column_set(uint32_t idxno);
  int find_current_row(uchar *buf, ibis::table::cursor* cursor);
  void create_writer(TABLE *table_arg);
  static int get_writer_partno(ibis::tablex* writer, char* datadir);
  static void background_write(ibis::tablex* writer,  char* datadir, TABLE* table, WARP_SHARE* share);
  void foreground_write();
  bool append_column_filter(const Item* cond, std::string& push_where_clause); 
  static void maintain_indexes(char* datadir, TABLE* table);
  void open_deleted_bitmap(int lock_mode = LOCK_SH);
  void close_deleted_bitmap();
  bool is_deleted(uint64_t rowid);
  void write_dirty_rows();
  
  /* These objects are used to access the FastBit tables for tuple reads.*/ 
  ibis::mensa*         base_table         = NULL; 
  ibis::table*         filtered_table     = NULL;
  ibis::table*         idx_filtered_table = NULL;
  ibis::table::cursor* cursor             = NULL;
  ibis::table::cursor* idx_cursor         = NULL;
  sparsebitmap*        deleted_bitmap     = NULL;       
  
  /* WHERE clause constructed from engine condition pushdown */
  std::string          push_where_clause  = "";

  /* This object is used to append tuples to the table */
  ibis::tablex* writer = NULL;

  /* A list of row numbers to delete (filled in by delete_row) */
  std::vector<uint64_t> deleted_rows;

  /* this is always the rowid of the current row */
  uint64_t current_rowid = 0;  

  /* a SELECT lists of the columns that have been fetched for the current query */
  std::string column_set = "";
  std::string index_column_set = "";

  /* temporary buffer populated with CSV of row for insertions*/
  String buffer;

  /* storage for BLOBS */
  MEM_ROOT blobroot; 

  /* set to true if the table has deleted rows */
  bool has_deleted_rows = false;

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
  uint max_keys() const { return 16384; }
  uint max_key_parts() const { return 1; }
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
    //return (double)(stats.records + stats.deleted) / 20.0 + 10;
    return 1.0/(stats.records > 0 ? stats.records : 1);
  }

  /* The next method will never be called */
  virtual bool fast_key_read() { return 1; }
  ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR; }

  int open(const char *name, int mode, uint open_options,
           const dd::Table *table_def);
  int close(void);

  std::string make_unique_check_clause();
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
  int delete_all_rows(void);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info,
             dd::Table *table_def);
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);
  int delete_table(const char *table_name, const dd::Table *);
  //int truncate(dd::Table *);
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
  int make_where_clause(const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag, std::string& where_clause);
  void get_auto_increment	(	
    ulonglong 	offset,
    ulonglong 	increment,
    ulonglong 	nb_desired_values,
    ulonglong * 	first_value,
    ulonglong * 	nb_reserved_values 
  );

  int engine_push(AQP::Table_access *table_aqp);
  const Item* cond_push(const Item *cond,	bool other_tbls_ok );
	
  int rename_table(const char * from, const char * to, const dd::Table* , dd::Table* ) {
    DBUG_ENTER("ha_example::rename_table ");
    std::string cmd = "mv " + std::string(from) + ".data/ " + std::string(to) + ".data/";
    
    system(cmd.c_str()); 
    DBUG_RETURN(0);
  }

  
};
#endif
