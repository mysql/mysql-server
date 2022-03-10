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
/*
 * ha_warp.cc:76:
/data/warp/storage/warp/ha_warp.h:23:32: error: ‘-Werror=suggest-override’ is not an option that controls warnings [-Werror=pragmas]
*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-override"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra-semi"
#ifndef HA_WARP_HDR
#define HA_WARP_HDR
#define MYSQL_SERVER 1

// system includes
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <fstream>  
#include <iostream>  
#include <string> 
#include <thread>
#include <forward_list>
#include <unordered_map>
#include <time.h>

// MySQL utility includes
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "map_helpers.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_psi_config.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_memory.h"
#include <mysql/plugin.h>
#include <mysql/psi/mysql_file.h>
#include "template_utils.h"

// MySQL SQL includes
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/handler.h"
#include "sql_string.h"
#include "sql/dd/dd.h"
#include "sql/dd/dd_table.h"
#include "sql/dd/dd_schema.h"
#include "sql/abstract_query_plan.h"
#include "sql/field.h"
#include "sql/sql_class.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/log.h"
#include "sql/sql_thd_internal_api.h"
#include "sql/sql_executor.h"

// Fastbit includes
#include "include/fastbit/ibis.h"
#include "include/fastbit/query.h"
#include "include/fastbit/bundle.h"
#include "include/fastbit/tafel.h"
#include "include/fastbit/array_t.h"
#include "include/fastbit/mensa.h"
#include "include/fastbit/resource.h"
#include "include/fastbit/util.h"
#include "include/fastbit/qExpr.h"

// WARP includes
#include "sparse.hpp"

// used for debugging in development
#define dbug(x) std::cerr << __LINE__ << ": " << x << "\n"; fflush(stdout)

/*
  Version for file format.
  1 - Initial Version. That is, the version when the metafile was introduced.
*/
const uint16_t WARP_VERSION = 2;
const uint64_t WARP_ROWID_BATCH_SIZE = 100000;

#define BLOB_MEMROOT_ALLOC_SIZE 8192

static const char *ha_warp_exts[] = {
  ".data",
  NullS
};

/* engine variables */
static unsigned long long my_partition_max_rows, my_cache_size, my_write_cache_size;//, my_lock_wait_timeout;

static MYSQL_SYSVAR_ULONGLONG(
  partition_max_rows,
  my_partition_max_rows,
  PLUGIN_VAR_RQCMDARG,
  "The maximum number of rows in a Fastbit partition.  An entire partition must fit in the cache.",
  NULL,
  NULL,
  10000000,
  64000,
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
  1024ULL * 1024 * 1024 * 1,
  1024ULL * 1024 * 1024 * 1,
  1ULL<<63,
  0
);

static MYSQL_SYSVAR_ULONGLONG(
  write_cache_size,
  my_write_cache_size,
  PLUGIN_VAR_RQCMDARG,
  "Fastbit file cache size",
  NULL,
  NULL,
  1000000,
  32000,
  1ULL<<63,
  0
);
bool is_update = false;
std::vector<uint32_t> update_column_set;
std::vector<uint32_t> nullable_column_set;
/*
static MYSQL_SYSVAR_ULONGLONG(
  lock_wait_timeout,
  my_lock_wait_timeout,
  PLUGIN_VAR_RQCMDARG,
  "Timeout in seconds a WARP transaction may wait "
  "for a lock before being rolled back.",
  NULL,
  NULL,
  1,
  50,
  ULONG_MAX,
  0
);
*/

static MYSQL_THDVAR_ULONG(lock_wait_timeout, PLUGIN_VAR_RQCMDARG,
                          "Timeout in seconds a WARP transaction may wait "
                          "for a lock before being rolled back.",
                          nullptr, nullptr, 50, 0, ULONG_MAX, 0);

static MYSQL_THDVAR_STR(partition_filter, PLUGIN_VAR_RQCMDARG|PLUGIN_VAR_MEMALLOC,
                          "Set partition to scan in a particular table using alias.partname",
                          nullptr, nullptr, "");

static MYSQL_THDVAR_BOOL(adjust_table_stats_for_joins, PLUGIN_VAR_NOCMDARG,
                          "Sets the largest table in a query to have a row count of 2.  Can cause problems with some MySQL subquery optimizations.",
                          nullptr, nullptr, true);

static MYSQL_THDVAR_ULONG(max_degree_of_parallelism, PLUGIN_VAR_RQCMDARG,
                          "Maximum number of threads which can be used for join optimization",
                          nullptr, nullptr, std::thread::hardware_concurrency(), 1, std::thread::hardware_concurrency() * 4, 0);

SYS_VAR* system_variables[] = {
  MYSQL_SYSVAR(partition_max_rows),
  MYSQL_SYSVAR(cache_size),
  MYSQL_SYSVAR(write_cache_size),
  MYSQL_SYSVAR(lock_wait_timeout),
  MYSQL_SYSVAR(partition_filter),
  MYSQL_SYSVAR(adjust_table_stats_for_joins),
  MYSQL_SYSVAR(max_degree_of_parallelism),
  NULL
};

struct warp_filter_info {
private:
  std::set<uint64_t> dim_rownums;
  bool frozen = false;
public:
  std::string fact_column = "";
  std::string dim_alias = "";
  std::string dim_column = "";
  enum_field_types fact_column_type;
  enum_field_types dim_column_type;
  std::mutex mtx;

  warp_filter_info(std::string fact_column, std::string dim_alias, std::string dim_column) {
    this->fact_column = fact_column;
    this->dim_alias = dim_alias;
    this->dim_column = dim_column; 
  }

  /* in order to do batch operations, the mutex can be taken manually
      and then the second parameter should be false
  */
  
  void add_matching_rownum(uint64_t rownum) {
    dim_rownums.insert(rownum);
  }

  std::set<uint64_t>* get_rownums() {
    return &dim_rownums;
  }
  
};  

typedef std::unordered_map<warp_filter_info*, std::unordered_map<uint64_t, uint64_t>*> fact_table_filter;

struct WARP_SHARE {
  std::string table_name;
  uint table_name_length, use_count;
  char data_dir_name[FN_REFLEN];
  uint64_t next_rowid = 0;  
  uint64_t rowids_generated = 0;
  mysql_mutex_t mutex;
  THR_LOCK lock;
};


class warp_join_info {
  public:
  const char* alias;
  Field* field;
};

// This class is filled out during ::info and ::engine_pushdown 
class warp_pushdown_information {
  public:
  bool is_fact_table = false;
  
  // columns projected from this table
  // if the table is opened during pushdown operations, these
  // columns will be projected so that the opened and filtered
  // table can be re-used for the scan
  std::string column_set = "";

  // where clause set in ::cond_push
  std::string filter;
  
  // data directory of the table
  const char* datadir;
  
  // fields of the table
  Field** fields;

  // number of rows in the table (may be adjusted down for star schema optimization)
  uint64_t rowcount = 0;

  // fastbit objects for iterating the filtered table (may be opened in ::cond_push)
  ibis::table* base_table;
  ibis::table* filtered_table;
  ibis::table::cursor* cursor;
  
  // This points to a field on a specific table
  // in the join.  It is used to project the join
  // field information on this when the fact table is
  // opened, and to construct an in-memory hash
  // index if there is key on the dimension table
  std::unordered_map<Field*, warp_join_info> join_info;
  fact_table_filter* fact_table_filters;
  // if an index exists for the join in the dimension
  // table, then an in memory has index will be built
  // on the table to improve join performance because
  // point lookups on a bitmap index are much slower
  // than a tree index
  bool build_hash_index = false;
  
  std::string fact_filter = "";
  /*
  std::unordered_multimap<std::string, uint64_t> string_to_row_map;
  std::unordered_multimap<uint64_t, uint64_t> uint_to_row_map;
  std::unordered_multimap<int64_t, uint64_t> int_to_row_map;
  std::unordered_multimap<double, uint64_t> double_to_row_map;
  */
  // free up the fastbit resources once the table is finshed
  // being used
  
};


// maps the table aliases for this query to pushdown information info
std::unordered_map<THD*, std::unordered_map<std::string, warp_pushdown_information*> * > pd_info;
// held when accessing or modifying the pushdown info
std::mutex pushdown_mtx;
std::mutex fact_filter_mutex;
std::mutex parallel_join_mutex;

// initializes or returns the pushdown information for a table used in a query
warp_pushdown_information* get_or_create_pushdown_info(THD* thd, const char* alias, const char* data_dir_name);
warp_pushdown_information* get_pushdown_info(THD* thd, const char* alias);
uint64_t get_pushdown_info_count(THD* thd);


// used as a type of lock to provide consistent snapshots
// to deleted rows not visible to older transactions
// when a write lock is freed it is downgraded to a 
// visibility lock.  when a transaction closes, any
// locks HISTORY locks older than that transaction are
// also freed

// history locks are for read consistent views after
// rows are deleted.  they are freed when the 
// all the transactions older then them have
// been closed.  When an EX lock is freed it is
// downgraded to a LOCK_HISTORY lock if any
// changes were made to the row under the EX lock
#define LOCK_HISTORY -1

// if a lock acquisition results in a deadlock then
// LOCK_DEADLOCK is returned from create_lock()
#define LOCK_DEADLOCK -2

// select FOR UPDATE takes READ_EX locks which can
// be converted to a LOCK_EX without checking
// for deadlocks
#define WRITE_INTENTION -3

class warp_lock {
  public:
  // trx_id that holds this lock
  uint64_t holder;

  //trx_id that this lock is waiting on
  uint64_t waiting_on;

  //lock type:LOCK_EX, LOCK_SH, LOCK_HISTORY, LOCK_DEADLOCK
  int lock_type = 0;
};

// held during commit and rollback
std::mutex commit_mtx;

// markers for the transaction logs
const char begin_marker     = 1;
const char insert_marker    = 2;
const char delete_marker    = 3;
const char commit_marker    = 4;
const char rollback_marker  = 5;
const char savepoint_marker = 6;
#define ROLLBACK_STATEMENT 0

/* each trx in the commit_list has one of these states */
#define WARP_UNCOMMITTED_TRX 0
#define WARP_COMMITTED_TRX   1
#define WARP_ROLLED_BACK_TRX 2

std::mutex trx_mutex;  
//Holds the state of a WARP transaction.  Instantiated in ::external_lock
//or ::start_stmt
class warp_trx {
  public:
  // used to log inserts during an insertion.  If a statement rolls
  // back this is used to undo the insert statements
  FILE* log = NULL;
  std::string log_filename = "";
  bool registered = false;
  bool table_lock = false;
  
  // set when a statement is a DML statement
  // forces UPDATE and DELETE statements 
  // to be read-committed
  bool is_dml = false;

  // set when a SELECT has LOCK IN SHARE MODE as
  // part of the SELECT clause - takes shared locks
  // for each traversed visible row
  bool lock_in_share_mode = false;

  // when FOR UPDATE is in a SELECT clause
  // LOCK_EX is taken on traversed visible rows
  bool for_update = false;

  
  //the transaction identifier 
  ulonglong trx_id = 0;

  //number of table locks (read or write) held by the transaction
  uint64_t lock_count = 0;

  //number of background writers that are doing work.  The commit
  //function must wait for this counter to reach zero before a 
  //transaction can be committed
  //uint64_t background_writer_count = 0;

  //the transaction was not a read-only transaction and it 
  //modified rows in the database, so it must be writen into
  //the commit bitmap when it commmits
  bool dirty = false;

  //autocommit statements commit each time the commit function
  //is called
  bool autocommit = false;

  //selected transaction isolation level.  Only SERIALIZABLE,
  //REPEATABLE-READ and READ-COMMITTED are actually supported
  enum enum_tx_isolation isolation_level;

  //called when transactions start
  int begin();
  
  //called when transcations end
  void commit();

  void rollback(bool all);
  void write_insert_log_rowid(uint64_t rowid);
  void write_delete_log_rowid(uint64_t rowid);
  void open_log();
};

class warp_global_data {
  private:
  // held when reading or modifying state
  std::mutex mtx;
  // used when reading/modifying the lock structures

  std::mutex lock_mtx;
  std::mutex history_lock_mtx;
  char history_lock_writing = 0;
  std::string shutdown_clean_file = "shutdown_clean.warp";
  std::string warp_state_file     = "state.warp";
  std::string commit_filename     = "commits.warp";
  std::string delete_bitmap_file  = "deletes.warp";

  uint64_t rowid_batch_size = 10000;

  // each write to the state file increments the state counter
  // the state file has two records.  the state record with
  // the highest counter is used.
  uint64_t state_counter = 1;
  
  // Each time a transaction is handed out, this is pre-incremented
  // and the value is used for the tranaction identifier.  When the
  // transaction is committed, this idenifier is written into the
  // commit bitmap.
  ulonglong next_trx_id = 1;

  // Each time a write into a table starts, WARP hands out 
  // a set of 10000 rowid values, and this counter is 
  // incremented by 10000.  If a transaction runs out of
  // rowid values, it can request another batch of 10000
  // more.  Since rowid values are 64 bit, handing out 
  // in 10000 row batches is fine.  This value is persisted
  // between database restarts.
  uint64_t next_rowid = 1;

  /* this is used to read/write on disk state */
  struct on_disk_state {
    public:
    uint64_t version;
    uint64_t next_trx_id;
    uint64_t next_rowid;
    uint64_t state_counter;
  };

  // handle to the warp state
  FILE* fp = NULL;

  //rowid, warp_lock object
  std::unordered_multimap<uint64_t, warp_lock> row_locks;
  
  // rowid, trx_id
  std::unordered_map<uint64_t, uint64_t> history_locks;

  // write the current state to the state file
  void write();

  // checks the state of the on-disk state
  bool check_state();

  // reads the state from disk
  uint64_t get_state_and_return_version();

  // return false if shutdown file was not found
  bool was_shutdown_clean();

  // used to repair tables
  bool repair_tables();

  void write_clean_shutdown();

  

  public:
  //sparsebitmap* commit_bitmap = NULL;
  sparsebitmap* delete_bitmap = NULL;
  
  std::unordered_map<uint64_t, int> commit_list;
  FILE* commit_file;
  
  // opens and reads the state file.  
  // if the on-disk version of data is older than the current verion
  // the database will be upgraded.  If the on disk version is newer
  // than the current version, this will assert and the database will
  // fail to start and MySQL will be crashed on purpose!
  // if shutdown was not clean, then table repair will be executed.
  warp_global_data();

  // called at database shutdown.  
  // Calls write() to persist the state to disk
  // writes the clean shutdown file
  ~warp_global_data();
  
  uint64_t get_next_rowid_batch();
  uint64_t get_next_trx_id();
  bool is_transaction_open(uint64_t check_trx_id);
  void mark_transaction_closed(uint64_t trx_id);
  void register_open_trx(uint64_t trx);

  int create_lock(uint64_t lock_id, warp_trx* trx, int lock_type);
  int unlock(uint64_t rowid, warp_trx* trx);
  int downgrade_to_history_lock(uint64_t rowid, warp_trx* trx);
  int free_locks(warp_trx* trx);
  uint64_t get_history_lock(uint64_t rowid);
  void cleanup_history_locks();

};

//Initialized in the SE init function, destroyed when engine is removed
warp_global_data* warp_state;

//The MySQL table share functions
static WARP_SHARE *get_share(const char *table_name, TABLE* table_ptr);
static int free_share(WARP_SHARE *share); 

//Called when a transaction or statement commits.  A pointer to this
//function is registered on the hanlderton
int warp_commit(handlerton*, THD *, bool);

//Called when a transaction or statement rolls back
// A pointer to this function is registered on the hanlderton                       
int warp_rollback(handlerton *, THD *, bool);

//Called by the warp_state constructor to upgrade on disk tables when
//the on-disk version is older than the current version
int warp_upgrade_tables(uint16_t version);

// determines is a transaction_id is an open transaction
// used for UNIQUE check visibility during inserts
// and for REPEATABLE READ during scans
bool warp_is_trx_open(uint64_t trx_id);

std::unordered_map<const char*, uint64_t> get_table_counts_in_schema(char* table_dir);
const char* get_table_with_most_rows(std::unordered_map<const char*, uint64_t>* table_counts, std::unordered_map<std::string, bool> query_tables = {});
uint64_t get_least_row_count(std::unordered_map<const char*, uint64_t>* table_counts);
bool has_empty_table(std::unordered_map<const char*, uint64_t>* table_counts);
//If any table is empty or limit 0 then abort the scan
bool abort_query = false;
bool full_partition_scan = false;
warp_trx* warp_get_trx(handlerton* hton, THD* thd);

//This is the handler where the majority of the work is done.  Handles
//creating and dropping tables, TRUNCATE table, reading from indexes,
//scanning tables, inserts, updates, deletes, engine condition pushdown
class ha_warp : public handler {
  /* MySQL lock - Fastbit has its own internal mutex implementation.  This is used to protect the share.*/
  THR_LOCK_DATA lock; 

  /* Shared lock info */
  WARP_SHARE *share;  

 /* used in visibility checks */
 uint64_t last_trx_id = 0;
 bool is_trx_visible = false;
 //warp_trx* current_trx = NULL;
 public:
 WARP_SHARE* get_warp_share();

 private:
  // used in combination with THDVAR partition_filter
  // to limit the partition scanned by a query
  std::string partition_filter_alias;
  std::string partition_filter_partition_name;

  ibis::partList* partitions = NULL;
  ibis::partList::iterator part_it;
  FILE* written_row_fp = NULL;
  
  void update_row_count();
  int reset_table();
  int encode_quote(uchar *buf);
  int set_column_set(); 
  //int set_column_set(uint32_t idxno);
  int find_current_row(uchar *buf, ibis::table::cursor* cursor);
  void create_writer(TABLE *table_arg);
  std::string get_writer_partition();
  void write_buffered_rows_to_disk();
  void foreground_write();
  int append_column_filter(const Item* cond, std::string& push_where_clause); 
  static void maintain_indexes(const char* datadir);
  void open_deleted_bitmap(int lock_mode = LOCK_SH);
  void close_deleted_bitmap();
  bool is_deleted(uint64_t rowid);
  int open_trx_log();
  int close_trx_log();

  bool lock_in_share_mode = false;
  bool lock_for_update = false;

  //std::string unique_check_where_clause = "";
  //bool table_checked_unique_keys = false;
  //bool table_has_unique_keys = false;
  //String key_part_tmp;
  //String key_part_esc;
  //bool has_unique_keys();
  //void make_unique_check_clause();
  //uint64_t lookup_in_hash_index(const uchar*, key_part_map, ha_rkey_function);
  bool bitmap_merge_join_executed = false;

  int bitmap_merge_join();
  void cleanup_pushdown_info();

  bool close_in_extra = false;
  std::mutex write_mutex;

  /* These objects are used to access the FastBit tables for tuple reads.*/ 
  ibis::table*         base_table         = NULL; 
  ibis::table*         filtered_table     = NULL;
  ibis::table::cursor* cursor             = NULL;
  //ibis::table*         idx_filtered_table = NULL;
  //ibis::table::cursor* idx_cursor         = NULL;

  /* These objects are used by WARP to add functionality to Fastbit
     such as deletion/update of rows and transactions
  */    
  FILE*                insert_log         = NULL; 

  /* WHERE clause constructed from engine condition pushdown */
  std::string          push_where_clause  = "";
  int64_t pushdown_table_count = 0;
  
  
  std::unordered_map<std::string, std::vector<uint32_t>*> matching_ridset;
  std::vector<uint32_t>* current_matching_ridset=NULL;
  std::vector<uint32_t>::iterator current_matching_ridset_it;

  //FIXME?:
  // This could totally break if a dimension table has more than ~4B rows!
  // I doubt anybody is going to try to do that with WARP so I am leaving
  // this as it is until somebody complains!  Will put a note in the 
  // release notes :)
  std::set<uint64_t>::iterator current_matching_dim_ridset_it;
  std::set<uint64_t>* current_matching_dim_ridset=NULL;

  uint32_t rownum = 0;
  uint32_t running_join_threads = 0;
  
  uint64_t fetch_count = 0;
  bool all_jobs_completed = false;
  uint32_t running_dimension_merges = 0;
  std::mutex dimension_merge_mutex;
  bool all_dimension_merges_completed = false;

  //static void warp_filter_table(ibis::mensa::table* filtered_table, std::string filter_column, std::vector<std::string>* batch, std::string push_where_clause, std::vector<uint64_t> matching_rowids, std::mutex* mtx, uint64_t* thread_count);
 
  fact_table_filter fact_table_filters;

  // used for index lookups
  //std::string          idx_where_clause   = "";
  //ibis::qExpr*         idx_qexpr;
  
  /* This object is used to append tuples to the table */
  ibis::tablex* writer = NULL;

  /* A list of row numbers to delete (filled in by delete_row) */
  std::vector<uint64_t> deleted_rows;

  /* this is always the rowid of the current row */
  uint64_t current_rowid = 0;  

  /* a SELECT lists of the columns that have been fetched for the current query */
  std::string column_set = "";
  //std::string index_column_set = "";

  /* temporary buffer populated with CSV of row for insertions*/
  String buffer;

  /* storage for BLOBS */
  MEM_ROOT blobroot; 

  /* set to true if the table has deleted rows */
  bool has_deleted_rows = false;
  
 public:
  ha_warp(handlerton *hton, TABLE_SHARE *table_arg);
  handlerton* warp_hton;
  ~ha_warp() {
    blobroot.ClearForReuse();
  }
 
  const char *table_type() const { return "WARP"; }
 
  ulonglong table_flags() const {
    // return (HA_NO_TRANSACTIONS | HA_NO_AUTO_INCREMENT | HA_BINLOG_ROW_CAPABLE | HA_CAN_REPAIR);
    return (HA_BINLOG_ROW_CAPABLE | HA_NO_AUTO_INCREMENT | HA_CAN_REPAIR);
  }
 
  uint max_record_length() const { return 0; }
  uint max_keys() const { return 0; }
  uint max_key_parts() const { return 0; }
  uint max_key_length() const { return 0; }
  uint max_supported_keys() const { return 0; }
  uint max_supported_key_length() const { return 0; }
  uint max_supported_key_part_length(
      HA_CREATE_INFO *create_info MY_ATTRIBUTE((unused))) const {
    return 0;
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

  
  const char **bas_ext() const ;
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
  warp_trx* create_trx(THD* thd);
  int external_lock(THD *thd, int lock_type);
  int start_stmt(THD *thd, thr_lock_type lock_type);
  int register_trx_with_mysql(THD* thd, warp_trx* trx);
  bool is_trx_visible_to_read(uint64_t row_trx_id);
  bool is_row_visible_to_read(uint64_t rowid);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  //int truncate(dd::Table *);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  /*
    These functions used to get/update status of the handler.
    Needed to enable concurrent inserts.
  */
  void get_status();
  void update_status();
  ulong index_flags(uint, uint, bool) const {
    return 0;
  };
  
  // Functions to support indexing
  /*
  
  ha_rows records_in_range(uint idxno, key_range *, key_range *); 
  int index_init(uint idxno, bool sorted);
  int index_init(uint idxno);
  int index_next(uchar * buf);
  int index_first(uchar * buf);
  int index_end();
  int index_read_map (uchar *buf, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag);
  int index_read_idx_map (uchar *buf, uint idxno, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag);
  int make_where_clause(const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag);
  void get_auto_increment	(	
    ulonglong 	offset,
    ulonglong 	increment,
    ulonglong 	nb_desired_values,
    ulonglong * 	first_value,
    ulonglong * 	nb_reserved_values 
  );
  */

  // Functions to support engine condition pushdown (ECP)
  int engine_push(AQP::Table_access *table_aqp);
  const Item* warp_cond_push(const Item *cond,	bool other_tbls_ok );
	
  int rename_table(const char * from, const char * to, const dd::Table* , dd::Table* );

  std::string explain_extra() const;

};
#endif
