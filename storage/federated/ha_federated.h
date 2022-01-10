/* Copyright (c) 2004, 2021, Oracle and/or its affiliates.

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

/*
  Please read ha_exmple.cc before reading this file.
  Please keep in mind that the federated storage engine implements all methods
  that are required to be implemented. handler.h has a full list of methods
  that you can implement.
*/

#include <mysql.h>
#include "prealloced_array.h"

/* 
  handler::print_error has a case statement for error numbers.
  This value is (10000) is far out of range and will envoke the 
  default: case.  
  (Current error range is 120-159 from include/my_base.h)
*/
#define HA_FEDERATED_ERROR_WITH_REMOTE_SYSTEM 10000

#define FEDERATED_QUERY_BUFFER_SIZE STRING_BUFFER_USUAL_SIZE * 5
#define FEDERATED_RECORDS_IN_RANGE 2
#define FEDERATED_MAX_KEY_LENGTH 3500 // Same as innodb

/*
  FEDERATED_SHARE is a structure that will be shared amoung all open handlers
  The example implements the minimum of what you will probably need.
*/
typedef struct st_federated_share {
  MEM_ROOT mem_root;

  bool parsed;
  /* this key is unique db/tablename */
  const char *share_key;
  /*
    the primary select query to be used in rnd_init
  */
  char *select_query;
  /*
    remote host info, parse_url supplies
  */
  char *server_name;
  char *connection_string;
  char *scheme;
  char *connect_string;
  char *hostname;
  char *username;
  char *password;
  char *database;
  char *table_name;
  char *table;
  char *socket;
  char *sport;
  int share_key_length;
  ushort port;

  size_t table_name_length, server_name_length, connect_string_length, use_count;
  mysql_mutex_t mutex;
  THR_LOCK lock;
} FEDERATED_SHARE;

/*
  Class definition for the storage engine
*/
class ha_federated: public handler
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  FEDERATED_SHARE *share;    /* Shared lock info */
  MYSQL *mysql; /* MySQL connection */
  MYSQL_RES *stored_result;
  /**
    Array of all stored results we get during a query execution.
  */
  Prealloced_array<MYSQL_RES*, 4, true> results;
  bool position_called;
  MYSQL_ROW_OFFSET current_position;  // Current position used by ::position()
  int remote_error_number;
  char remote_error_buf[FEDERATED_QUERY_BUFFER_SIZE];
  bool ignore_duplicates, replace_duplicates;
  bool insert_dup_update;
  DYNAMIC_STRING bulk_insert;

private:
  /*
      return 0 on success
      return errorcode otherwise
  */
  uint convert_row_to_internal_format(uchar *buf, MYSQL_ROW row,
                                      MYSQL_RES *result);
  bool create_where_from_key(String *to, KEY *key_info, 
                             const key_range *start_key,
                             const key_range *end_key,
                             bool records_in_range, bool eq_range);
  int stash_remote_error();

  bool append_stmt_insert(String *query);

  int read_next(uchar *buf, MYSQL_RES *result);
  int index_read_idx_with_result_set(uchar *buf, uint index,
                                     const uchar *key,
                                     uint key_len,
                                     ha_rkey_function find_flag,
                                     MYSQL_RES **result);
  int real_query(const char *query, size_t length);
  int real_connect();
public:
  ha_federated(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_federated() {}
  /* The name that will be used for display purposes */
  const char *table_type() const { return "FEDERATED"; }
  /*
    Next pointer used in transaction
  */
  ha_federated *trx_next;
  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
   */
  // perhaps get index type
  const char *index_type(uint inx) { return "REMOTE"; }
  const char **bas_ext() const;
  /*
    This is a list of flags that says what the storage engine
    implements. The current table flags are documented in
    handler.h
  */
  ulonglong table_flags() const
  {
    /* fix server to be able to get remote server table flags */
    return (HA_PRIMARY_KEY_IN_READ_INDEX |
            HA_PRIMARY_KEY_REQUIRED_FOR_POSITION | HA_FILE_BASED |
            HA_REC_NOT_IN_SEQ | HA_AUTO_PART_KEY | HA_CAN_INDEX_BLOBS |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
            HA_NO_PREFIX_CHAR_KEYS | HA_PRIMARY_KEY_REQUIRED_FOR_DELETE |
            HA_NO_TRANSACTIONS /* until fixed by WL#2952 */ |
            HA_PARTIAL_COLUMN_READ | HA_NULL_IN_KEY |
            HA_CAN_REPAIR);
  }
  /*
    This is a bitmap of flags that says how the storage engine
    implements indexes. The current index flags are documented in
    handler.h. If you do not implement indexes, just return zero
    here.

    part is the key part to check. First key part is 0
    If all_parts it's set, MySQL want to know the flags for the combined
    index up to and including 'part'.
  */
    /* fix server to be able to get remote server index flags */
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return (HA_READ_NEXT | HA_READ_RANGE | HA_READ_AFTER_KEY);
  }
  uint max_supported_record_length() const { return HA_MAX_REC_LENGTH; }
  uint max_supported_keys()          const { return MAX_KEY; }
  uint max_supported_key_parts()     const { return MAX_REF_PARTS; }
  uint max_supported_key_length()    const { return FEDERATED_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length(HA_CREATE_INFO
                    *create_info MY_ATTRIBUTE((unused))) const
  { return FEDERATED_MAX_KEY_LENGTH; }
  /*
    Called in test_quick_select to determine if indexes should be used.
    Normally, we need to know number of blocks . For federated we need to
    know number of blocks on remote side, and number of packets and blocks
    on the network side (?)
    Talk to Kostja about this - how to get the
    number of rows * ...
    disk scan time on other side (block size, size of the row) + network time ...
    The reason for "records * 1000" is that such a large number forces 
    this to use indexes "
  */
  double scan_time()
  {
    DBUG_PRINT("info", ("records %lu", (ulong) stats.records));
    return (double)(stats.records*1000); 
  }
  /*
    The next method will never be called if you do not implement indexes.
  */
  double read_time(uint index, uint ranges, ha_rows rows) 
  {
    /*
      Per Brian, this number is bugus, but this method must be implemented,
      and at a later date, he intends to document this issue for handler code
    */
    return (double) rows /  20.0+1;
  }

  const key_map *keys_to_use_for_scanning() { return &key_map_full; }
  /*
    Everything below are methods that we implment in ha_federated.cc.

    Most of these methods are not obligatory, skip them and
    MySQL will treat them as not implemented
  */
  int open(const char *name, int mode, uint test_if_locked);    // required
  int close(void);                                              // required

  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  int write_row(uchar *buf);
  int update_row(const uchar *old_data, uchar *new_data);
  int delete_row(const uchar *buf);
  int index_init(uint keynr, bool sorted);
  ha_rows estimate_rows_upper_bound();
  int index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag);
  int index_read(uchar *buf, const uchar *key,
                 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(uchar *buf, uint idx, const uchar *key,
                     uint key_len, enum ha_rkey_function find_flag);
  int index_next(uchar *buf);
  int index_end();
  int read_range_first(const key_range *start_key,
                               const key_range *end_key,
                               bool eq_range, bool sorted);
  int read_range_next();
  /*
    unlike index_init(), rnd_init() can be called two times
    without rnd_end() in between (it only makes sense if scan=1).
    then the second call should prepare for the new table scan
    (e.g if rnd_init allocates the cursor, second call should
    position it to the start of the table, no need to deallocate
    and allocate it again
  */
  int rnd_init(bool scan);                                      //required
  int rnd_end();
  int rnd_next(uchar *buf);                                      //required
  int rnd_next_int(uchar *buf);
  int rnd_pos(uchar *buf, uchar *pos);                            //required
  void position(const uchar *record);                            //required
  int info(uint);                                              //required
  int extra(ha_extra_function operation);

  void update_auto_increment(void);
  int repair(THD* thd, HA_CHECK_OPT* check_opt);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);

  int delete_all_rows(void);
  int truncate();
  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info);                      //required
  ha_rows records_in_range(uint inx, key_range *start_key,
                                   key_range *end_key);
  uint8 table_cache_type() { return HA_CACHE_TBL_NOCACHE; }

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);     //required
  bool get_error_message(int error, String *buf);
  
  MYSQL_RES *store_result(MYSQL *mysql);
  void free_result();
  
  int external_lock(THD *thd, int lock_type);
  int connection_commit();
  int connection_rollback();
  int connection_autocommit(bool state);
  int execute_simple_query(const char *query, int len);
  int reset(void);
  int rnd_pos_by_record(uchar *record);
};

