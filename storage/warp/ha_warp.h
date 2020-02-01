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

#include <sys/stat.h>
#include <sys/types.h>

#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "sql/handler.h"
#include "sql_string.h"

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
#include "include/fastbit/warp.h"

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
  int find_current_row(uchar *buf);
  void create_writer(TABLE *table_arg);
  MEM_ROOT blobroot; 
  uint64_t count_star_counter;
  bool count_star_query;

  /* These objects are used to access the FastBit tables for tuple reads.*/ 
  ibis::warp*         base_table; 
  ibis::table*         filtered_table;
  ibis::table::cursor* cursor;

  /* This object is used to append tuples to the table */
  ibis::tablex*         writer;

  /* A list of row numbers to delete (filled in by delete_row) */
  std::vector<uint64_t> deleted_rows;

  /* this is always the rowid of the current row */
  uint64_t current_rowid;  

  /* a SELECT list of the columns that have been fetched for the current query */
  std::string column_set;

  /* temporary buffer populated with CSV of row for insertions*/
  String buffer;

  //unsigned int column_count;

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
  int delete_table(const char *table_name, const dd::Table *table_def);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  /*
    These functions used to get/update status of the handler.
    Needed to enable concurrent inserts.
  */
  void get_status();
  void update_status();

 ulong index_flags(uint, uint, bool) const {
   DBUG_ENTER("ha_warp::index_flags");
   DBUG_RETURN(HA_READ_NEXT | HA_READ_RANGE | HA_KEYREAD_ONLY | HA_DO_INDEX_COND_PUSHDOWN);
 }

 ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key) {
    DBUG_ENTER("ha_warp::records_in_range");
    //table->s->key_parts
    DBUG_RETURN(1);
  }

  void get_auto_increment	(	
    ulonglong 	offset,
    ulonglong 	increment,
    ulonglong 	nb_desired_values,
    ulonglong * 	first_value,
    ulonglong * 	nb_reserved_values 
  );

  // bitmap indexes are not sorted 
  int index_init(uint idx, bool sorted) {
    DBUG_ENTER("ha_warp::index_init");
    DBUG_PRINT("ha_warp::index_init",("Key #%d, sorted:%d",idx, sorted));
    if(sorted) DBUG_RETURN(-1);
    
    active_index=idx; 
    base_table = new ibis::warp(share->data_dir_name);
    set_column_set();
    DBUG_RETURN(0);
  }

  int index_init(uint idx) { 
    DBUG_ENTER("ha_warp::index_init(uint)");
    active_index=idx; 
    DBUG_RETURN(0); 
  }

  int index_read(uint8_t * buf, const uint8_t * key,
                        ulonglong keypart_map,
                        enum ha_rkey_function find_flag) {
    DBUG_ENTER("ha_warp::index_read");
    DBUG_RETURN(-1);
  }

  int index_read_idx(uint8_t * buf, uint keynr, const uint8_t * key,
                           ulonglong keypart_map,
                           enum ha_rkey_function find_flag) {
    DBUG_ENTER("ha_warp::index_read_idx");
    DBUG_RETURN(-1);                           
  }

  int index_read_last(uint8_t * buf, const uint8_t * key,
                            key_part_map keypart_map) {
    DBUG_ENTER("ha_warp::index_read_last");
    DBUG_RETURN(-1);
  }

  int index_next(uchar * buf) {
    DBUG_ENTER("ha_warp::index_next");
    if(cursor->fetch() != 0) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }

    find_current_row(buf);
    DBUG_RETURN(0);
  }

  int index_prev(uchar * buf) {
    DBUG_ENTER("ha_warp::index_prev");
    DBUG_RETURN(-1);
  }

  int index_first(uchar * buf) {
    DBUG_ENTER("ha_warp::index_first");
    filtered_table = base_table->select(column_set.c_str(), "1=1");
    if(filtered_table == NULL) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    cursor = filtered_table->createCursor();
    if(cursor->fetch() != 0) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    find_current_row(buf);

    DBUG_RETURN(0);
  }

  int index_end() {
    DBUG_ENTER("ha_warp::index_end");
    if(cursor) delete cursor;
    cursor = NULL;
    if(filtered_table) delete filtered_table;
    filtered_table = NULL;
    if(base_table) delete base_table;
    base_table = NULL;
    DBUG_RETURN(0);
  }
  
  

  int make_where_clause(const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag, std::string& where_clause) {
    DBUG_ENTER("ha_warp::make_where_clause");
    where_clause = "";
    char data[1024];
    auto key_part = table->key_info[active_index].key_part;
    /* If the bit is set, then the part is being used.  Unfortunately MySQL will only 
       consider prefixes so we need to use ECP for magical performance.
    */
    unsigned char* key_offset = (unsigned char*)key;
    for (int partno = 0; partno < table->key_info[active_index].actual_key_parts; partno++ ) {
      /* given index (a,b,c) and where a=1 quit when we reach the b key part
         given a=1 and b=2 then quit when we reach the c part
      */
      if(!(keypart_map & (1<<partno))){
        DBUG_RETURN(0);
      }
      /* What field is this? */
      Field* f = table->key_info[active_index].key_part[partno].field;
      
      if(partno >0) where_clause += " AND ";

      /* Which column number does this correspond to? */
      where_clause += "c" + std::to_string(table->key_info[active_index].key_part[partno].field->field_index);
      
      switch(find_flag) {
        case HA_READ_AFTER_KEY:
          where_clause += " > ";
          break;
        case HA_READ_BEFORE_KEY:
          where_clause += " < ";
          break;
        case HA_READ_KEY_EXACT:
          where_clause += " = ";
          break;
        case HA_READ_KEY_OR_NEXT:
          where_clause += ">=";
          break;
    
        case HA_READ_KEY_OR_PREV:
          where_clause += "<=";
          break;
    
        case HA_READ_PREFIX:
        case HA_READ_PREFIX_LAST:
        case HA_READ_PREFIX_LAST_OR_PREV:
        default:
          DBUG_RETURN(-1);
      }

      bool is_unsigned = f->flags & UNSIGNED_FLAG;
    
      switch(f->real_type()) {
        case MYSQL_TYPE_TINY:
          if(is_unsigned) {
            where_clause += std::to_string((uint8_t)*(key_offset));
          } else {
            where_clause += std::to_string((int8_t)*(key_offset));
          }
          key_offset += 1;
          break;

        case MYSQL_TYPE_SHORT:
          if(is_unsigned) {
            where_clause += std::to_string((uint16_t)*(key_offset));
          } else {
            where_clause += std::to_string((int16_t)*(key_offset));
          }
          key_offset += 2;
          break;

        case MYSQL_TYPE_LONG:
          if(is_unsigned) {
            where_clause += std::to_string((uint32_t)*(key_offset));
          } else {
            where_clause += std::to_string((int32_t)*(key_offset));
          }
          key_offset += 4;
          break;
        
        /* FIXME: use 4 byte buffer that three bytes are copied into */
        case MYSQL_TYPE_INT24:
          if(is_unsigned) {
            where_clause += std::to_string((uint32_t)*(key_offset));
          } else {
            where_clause += std::to_string((int32_t)*(key_offset));
          }
          key_offset += 3;
          break;
        
        case MYSQL_TYPE_LONGLONG:
          if(is_unsigned) {
            where_clause += std::to_string((uint64_t)*(key_offset));
          } else {
            where_clause += std::to_string((int64_t)*(key_offset));
          }
          key_offset += 8;
          break;

        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_JSON: {
          // for strings, the key buffer is fixed width, and there is a two byte prefix
          // which lists the string length
          // FIXME: different data types probably have different prefix lengths
          uint16_t strlen = (uint16_t)(*key_offset);
          
          where_clause += "'" + std::string((const char*)key_offset+2, strlen) + "'";  
          key_offset += table->key_info[active_index].key_part[partno].store_length;
          break;
        }
        case MYSQL_TYPE_FLOAT:
          where_clause += std::to_string((float_t)*(key_offset));
          key_offset += 4;
          break;

        case MYSQL_TYPE_DOUBLE:
          where_clause += std::to_string((double_t)*(key_offset));
          key_offset += 8;
          break;

        
        // Support lookups for these types
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:         
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_YEAR:
        case MYSQL_TYPE_NEWDATE: 
        case MYSQL_TYPE_BIT:
        case MYSQL_TYPE_NULL:
        case MYSQL_TYPE_TIMESTAMP2:
        case MYSQL_TYPE_DATETIME2: 
        case MYSQL_TYPE_TIME2:   
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_GEOMETRY:
          DBUG_RETURN(0);
          break;
      }

      /* exclude NULL columns */
      //if(f->real_maybe_null()) {
      //  where_clause += " and n" + std::to_string(f->field_index) + " = 0";
      //}
    }
    DBUG_RETURN(0);
  }
  
  int index_read_map (uchar *buf, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag) {
      DBUG_ENTER("ha_warp::index_read_map");
      //DBUG_RETURN(HA_ERR_WRONG_COMMAND);
      std::string where_clause;
      make_where_clause(key, keypart_map, find_flag, where_clause);
      
      if(cursor) {
        delete cursor; 
      }
      cursor=NULL;
      
      if(filtered_table) {
        delete filtered_table;    
      }
      filtered_table = NULL;
      //std::cout << "SELECT " + column_set + " WHERE " + where_clause << "\n";
      filtered_table = base_table->select(column_set.c_str(), where_clause.c_str());
      if(filtered_table == NULL) {
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }

      cursor = filtered_table->createCursor();
      if(cursor->fetch() != 0) {
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }

      find_current_row(buf);
      DBUG_RETURN(0);

    }

/*
int index_read_last_map (uchar *buf, const uchar *key, key_part_map keypart_map) {
  DBUG_ENTER("ha_warp::index_read_last_map");
  DBUG_RETURN(-1);
}
 
  int index_read_idx_map (uchar *buf, uint index, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag) {
    DBUG_ENTER("ha_warp::index_read_idx_map");
    DBUG_RETURN(-1);
  }
*/


};
