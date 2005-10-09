/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


/* Classes in mysql */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

// TODO: create log.h and move all the log header stuff there

class Query_log_event;
class Load_log_event;
class Slave_log_event;
class Format_description_log_event;
class sp_rcontext;
class sp_cache;

enum enum_enable_or_disable { LEAVE_AS_IS, ENABLE, DISABLE };
enum enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY, RNEXT_SAME };
enum enum_duplicates { DUP_ERROR, DUP_REPLACE, DUP_UPDATE };
enum enum_log_type { LOG_CLOSED, LOG_TO_BE_OPENED, LOG_NORMAL, LOG_NEW, LOG_BIN};
enum enum_delay_key_write { DELAY_KEY_WRITE_NONE, DELAY_KEY_WRITE_ON,
			    DELAY_KEY_WRITE_ALL };

enum enum_check_fields { CHECK_FIELD_IGNORE, CHECK_FIELD_WARN,
			 CHECK_FIELD_ERROR_FOR_NULL };

extern char internal_table_name[2];
extern const char **errmesg;

#define TC_LOG_PAGE_SIZE   8192
#define TC_LOG_MIN_SIZE    (3*TC_LOG_PAGE_SIZE)

#define TC_HEURISTIC_RECOVER_COMMIT   1
#define TC_HEURISTIC_RECOVER_ROLLBACK 2
extern uint tc_heuristic_recover;

/*
  Transaction Coordinator log - a base abstract class
  for two different implementations
*/
class TC_LOG
{
  public:
  int using_heuristic_recover();
  TC_LOG() {}
  virtual ~TC_LOG() {}

  virtual int open(const char *opt_name)=0;
  virtual void close()=0;
  virtual int log(THD *thd, my_xid xid)=0;
  virtual void unlog(ulong cookie, my_xid xid)=0;
};

class TC_LOG_DUMMY: public TC_LOG // use it to disable the logging
{
  public:
  int open(const char *opt_name)        { return 0; }
  void close()                          { }
  int log(THD *thd, my_xid xid)         { return 1; }
  void unlog(ulong cookie, my_xid xid)  { }
};

#ifdef HAVE_MMAP
class TC_LOG_MMAP: public TC_LOG
{
  public:                // only to keep Sun Forte on sol9x86 happy
  typedef enum {
    POOL,                 // page is in pool
    ERROR,                // last sync failed
    DIRTY                 // new xids added since last sync
  } PAGE_STATE;

  private:
  typedef struct st_page {
    struct st_page *next; // page a linked in a fifo queue
    my_xid *start, *end;  // usable area of a page
    my_xid *ptr;          // next xid will be written here
    int size, free;       // max and current number of free xid slots on the page
    int waiters;          // number of waiters on condition
    PAGE_STATE state;     // see above
    pthread_mutex_t lock; // to access page data or control structure
    pthread_cond_t  cond; // to wait for a sync
  } PAGE;

  char logname[FN_REFLEN];
  File fd;
  my_off_t file_length;
  uint npages, inited;
  uchar *data;
  struct st_page *pages, *syncing, *active, *pool, *pool_last;
  /*
    note that, e.g. LOCK_active is only used to protect
    'active' pointer, to protect the content of the active page
    one has to use active->lock.
    Same for LOCK_pool and LOCK_sync
  */
  pthread_mutex_t LOCK_active, LOCK_pool, LOCK_sync;
  pthread_cond_t COND_pool, COND_active;

  public:
  TC_LOG_MMAP(): inited(0) {}
  int open(const char *opt_name);
  void close();
  int log(THD *thd, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover();

  private:
  void get_active_from_pool();
  int sync();
  int overflow();
};
#else
#define TC_LOG_MMAP TC_LOG_DUMMY
#endif

extern TC_LOG *tc_log;
extern TC_LOG_MMAP tc_log_mmap;
extern TC_LOG_DUMMY tc_log_dummy;

/* log info errors */
#define LOG_INFO_EOF -1
#define LOG_INFO_IO  -2
#define LOG_INFO_INVALID -3
#define LOG_INFO_SEEK -4
#define LOG_INFO_MEM -6
#define LOG_INFO_FATAL -7
#define LOG_INFO_IN_USE -8

/* bitmap to SQL_LOG::close() */
#define LOG_CLOSE_INDEX		1
#define LOG_CLOSE_TO_BE_OPENED	2
#define LOG_CLOSE_STOP_EVENT	4

struct st_relay_log_info;

typedef struct st_log_info
{
  char log_file_name[FN_REFLEN];
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  bool fatal; // if the purge happens to give us a negative offset
  pthread_mutex_t lock;
  st_log_info():fatal(0) { pthread_mutex_init(&lock, MY_MUTEX_INIT_FAST);}
  ~st_log_info() { pthread_mutex_destroy(&lock);}
} LOG_INFO;

typedef struct st_user_var_events
{
  user_var_entry *user_var_event;
  char *value;
  ulong length;
  Item_result type;
  uint charset_number;
} BINLOG_USER_VAR_EVENT;

#define RP_LOCK_LOG_IS_ALREADY_LOCKED 1
#define RP_FORCE_ROTATE               2

class Log_event;

/*
  TODO split MYSQL_LOG into base MYSQL_LOG and
  MYSQL_QUERY_LOG, MYSQL_SLOW_LOG, MYSQL_BIN_LOG
  most of the code from MYSQL_LOG should be in the MYSQL_BIN_LOG
  only (TC_LOG included)

  TODO use mmap instead of IO_CACHE for binlog
  (mmap+fsync is two times faster than write+fsync)
*/

class MYSQL_LOG: public TC_LOG
{
 private:
  /* LOCK_log and LOCK_index are inited by init_pthread_objects() */
  pthread_mutex_t LOCK_log, LOCK_index;
  pthread_cond_t update_cond;
  ulonglong bytes_written;
  time_t last_time,query_start;
  IO_CACHE log_file;
  IO_CACHE index_file;
  char *name;
  char time_buff[20],db[NAME_LEN+1];
  char log_file_name[FN_REFLEN],index_file_name[FN_REFLEN];
  // current file sequence number for load data infile binary logging
  uint file_id;
  uint open_count;				// For replication
  volatile enum_log_type log_type;
  enum cache_type io_cache_type;
  bool write_error, inited;
  bool need_start_event;
  /*
    no_auto_events means we don't want any of these automatic events :
    Start/Rotate/Stop. That is, in 4.x when we rotate a relay log, we don't want
    a Rotate_log event to be written to the relay log. When we start a relay log
    etc. So in 4.x this is 1 for relay logs, 0 for binlogs.
    In 5.0 it's 0 for relay logs too!
  */
  bool no_auto_events;
  /*
     The max size before rotation (usable only if log_type == LOG_BIN: binary
     logs and relay logs).
     For a binlog, max_size should be max_binlog_size.
     For a relay log, it should be max_relay_log_size if this is non-zero,
     max_binlog_size otherwise.
     max_size is set in init(), and dynamically changed (when one does SET
     GLOBAL MAX_BINLOG_SIZE|MAX_RELAY_LOG_SIZE) by fix_max_binlog_size and
     fix_max_relay_log_size).
  */
  ulong max_size;

  ulong prepared_xids; /* for tc log - number of xids to remember */
  pthread_mutex_t LOCK_prep_xids;
  pthread_cond_t  COND_prep_xids;
  friend class Log_event;

public:
  MYSQL_LOG();
  /*
    note that there's no destructor ~MYSQL_LOG() !
    The reason is that we don't want it to be automatically called
    on exit() - but only during the correct shutdown process
  */

  int open(const char *opt_name);
  void close();
  int log(THD *thd, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover(IO_CACHE *log, Format_description_log_event *fdle);

  /*
     These describe the log's format. This is used only for relay logs.
     _for_exec is used by the SQL thread, _for_queue by the I/O thread. It's
     necessary to have 2 distinct objects, because the I/O thread may be reading
     events in a different format from what the SQL thread is reading (consider
     the case of a master which has been upgraded from 5.0 to 5.1 without doing
     RESET MASTER, or from 4.x to 5.0).
  */
  Format_description_log_event *description_event_for_exec,
    *description_event_for_queue;

  void reset_bytes_written()
  {
    bytes_written = 0;
  }
  void harvest_bytes_written(ulonglong* counter)
  {
#ifndef DBUG_OFF
    char buf1[22],buf2[22];
#endif
    DBUG_ENTER("harvest_bytes_written");
    (*counter)+=bytes_written;
    DBUG_PRINT("info",("counter: %s  bytes_written: %s", llstr(*counter,buf1),
		       llstr(bytes_written,buf2)));
    bytes_written=0;
    DBUG_VOID_RETURN;
  }
  void set_max_size(ulong max_size_arg);
  void signal_update();
  void wait_for_update(THD* thd, bool master_or_slave);
  void set_need_start_event() { need_start_event = 1; }
  void init(enum_log_type log_type_arg,
	    enum cache_type io_cache_type_arg,
	    bool no_auto_events_arg, ulong max_size);
  void init_pthread_objects();
  void cleanup();
  bool open(const char *log_name,
            enum_log_type log_type,
            const char *new_name,
	    enum cache_type io_cache_type_arg,
	    bool no_auto_events_arg, ulong max_size,
            bool null_created);
  const char *generate_name(const char *log_name, const char *suffix,
                            bool strip_ext, char *buff);
  /* simplified open_xxx wrappers for the gigantic open above */
  bool open_query_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(generate_name(log_name, ".log", 0, buf),
                LOG_NORMAL, 0, WRITE_CACHE, 0, 0, 0);
  }
  bool open_slow_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(generate_name(log_name, "-slow.log", 0, buf),
                LOG_NORMAL, 0, WRITE_CACHE, 0, 0, 0);
  }
  bool open_index_file(const char *index_file_name_arg,
                       const char *log_name);
  void new_file(bool need_lock);
  bool write(THD *thd, enum enum_server_command command,
	     const char *format,...);
  bool write(THD *thd, const char *query, uint query_length,
	     time_t query_start=0);
  bool write(Log_event* event_info); // binary log write
  bool write(THD *thd, IO_CACHE *cache, Log_event *commit_event);

  void start_union_events(THD *thd);
  void stop_union_events(THD *thd);
  bool is_query_in_union(THD *thd, query_id_t query_id_param);

  /*
    v stands for vector
    invoked as appendv(buf1,len1,buf2,len2,...,bufn,lenn,0)
  */
  bool appendv(const char* buf,uint len,...);
  bool append(Log_event* ev);

  int generate_new_name(char *new_name,const char *old_name);
  void make_log_name(char* buf, const char* log_ident);
  bool is_active(const char* log_file_name);
  int update_log_index(LOG_INFO* linfo, bool need_update_threads);
  void rotate_and_purge(uint flags);
  bool flush_and_sync();
  int purge_logs(const char *to_log, bool included,
                 bool need_mutex, bool need_update_threads,
                 ulonglong *decrease_log_space);
  int purge_logs_before_date(time_t purge_time);
  int purge_first_log(struct st_relay_log_info* rli, bool included);
  bool reset_logs(THD* thd);
  void close(uint exiting);

  // iterating through the log index file
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
		   bool need_mutex);
  int find_next_log(LOG_INFO* linfo, bool need_mutex);
  int get_current_log(LOG_INFO* linfo);
  uint next_file_id();
  inline bool is_open() { return log_type != LOG_CLOSED; }
  inline char* get_index_fname() { return index_file_name;}
  inline char* get_log_fname() { return log_file_name; }
  inline char* get_name() { return name; }
  inline pthread_mutex_t* get_log_lock() { return &LOCK_log; }
  inline IO_CACHE* get_log_file() { return &log_file; }

  inline void lock_index() { pthread_mutex_lock(&LOCK_index);}
  inline void unlock_index() { pthread_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32 get_open_count() { return open_count; }
};


typedef struct st_copy_info {
  ha_rows records;
  ha_rows deleted;
  ha_rows updated;
  ha_rows copied;
  ha_rows error_count;
  enum enum_duplicates handle_duplicates;
  int escape_char, last_errno;
  bool ignore;
  /* for INSERT ... UPDATE */
  List<Item> *update_fields;
  List<Item> *update_values;
  /* for VIEW ... WITH CHECK OPTION */
  TABLE_LIST *view;
} COPY_INFO;


class key_part_spec :public Sql_alloc {
public:
  const char *field_name;
  uint length;
  key_part_spec(const char *name,uint len=0) :field_name(name), length(len) {}
  bool operator==(const key_part_spec& other) const;
};


class Alter_drop :public Sql_alloc {
public:
  enum drop_type {KEY, COLUMN };
  const char *name;
  enum drop_type type;
  Alter_drop(enum drop_type par_type,const char *par_name)
    :name(par_name), type(par_type) {}
};


class Alter_column :public Sql_alloc {
public:
  const char *name;
  Item *def;
  Alter_column(const char *par_name,Item *literal)
    :name(par_name), def(literal) {}
};


class Key :public Sql_alloc {
public:
  enum Keytype { PRIMARY, UNIQUE, MULTIPLE, FULLTEXT, SPATIAL, FOREIGN_KEY};
  enum Keytype type;
  enum ha_key_alg algorithm;
  List<key_part_spec> columns;
  const char *name;
  bool generated;

  Key(enum Keytype type_par, const char *name_arg, enum ha_key_alg alg_par,
      bool generated_arg, List<key_part_spec> &cols)
    :type(type_par), algorithm(alg_par), columns(cols), name(name_arg),
    generated(generated_arg)
  {}
  ~Key() {}
  /* Equality comparison of keys (ignoring name) */
  friend bool foreign_key_prefix(Key *a, Key *b);
};

class Table_ident;

class foreign_key: public Key {
public:
  enum fk_match_opt { FK_MATCH_UNDEF, FK_MATCH_FULL,
		      FK_MATCH_PARTIAL, FK_MATCH_SIMPLE};
  enum fk_option { FK_OPTION_UNDEF, FK_OPTION_RESTRICT, FK_OPTION_CASCADE,
		   FK_OPTION_SET_NULL, FK_OPTION_NO_ACTION, FK_OPTION_DEFAULT};

  Table_ident *ref_table;
  List<key_part_spec> ref_columns;
  uint delete_opt, update_opt, match_opt;
  foreign_key(const char *name_arg, List<key_part_spec> &cols,
	      Table_ident *table,   List<key_part_spec> &ref_cols,
	      uint delete_opt_arg, uint update_opt_arg, uint match_opt_arg)
    :Key(FOREIGN_KEY, name_arg, HA_KEY_ALG_UNDEF, 0, cols),
    ref_table(table), ref_columns(cols),
    delete_opt(delete_opt_arg), update_opt(update_opt_arg),
    match_opt(match_opt_arg)
  {}
};

typedef struct st_mysql_lock
{
  TABLE **table;
  uint table_count,lock_count;
  THR_LOCK_DATA **locks;
} MYSQL_LOCK;


class LEX_COLUMN : public Sql_alloc
{
public:
  String column;
  uint rights;
  LEX_COLUMN (const String& x,const  uint& y ): column (x),rights (y) {}
};

#include "sql_lex.h"				/* Must be here */

/* Needed to be able to have an I_List of char* strings in mysqld.cc. */

class i_string: public ilink
{
public:
  const char* ptr;
  i_string():ptr(0) { }
  i_string(const char* s) : ptr(s) {}
};

/* needed for linked list of two strings for replicate-rewrite-db */
class i_string_pair: public ilink
{
public:
  const char* key;
  const char* val;
  i_string_pair():key(0),val(0) { }
  i_string_pair(const char* key_arg, const char* val_arg) : 
    key(key_arg),val(val_arg) {}
};


class delayed_insert;
class select_result;

#define THD_SENTRY_MAGIC 0xfeedd1ff
#define THD_SENTRY_GONE  0xdeadbeef

#define THD_CHECK_SENTRY(thd) DBUG_ASSERT(thd->dbug_sentry == THD_SENTRY_MAGIC)

struct system_variables
{
  ulonglong myisam_max_extra_sort_file_size;
  ulonglong myisam_max_sort_file_size;
  ha_rows select_limit;
  ha_rows max_join_size;
  ulong auto_increment_increment, auto_increment_offset;
  ulong bulk_insert_buff_size;
  ulong join_buff_size;
  ulong long_query_time;
  ulong max_allowed_packet;
  ulong max_error_count;
  ulong max_heap_table_size;
  ulong max_length_for_sort_data;
  ulong max_sort_length;
  ulong max_tmp_tables;
  ulong max_insert_delayed_threads;
  ulong multi_range_count;
  ulong myisam_repair_threads;
  ulong myisam_sort_buff_size;
  ulong myisam_stats_method;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong optimizer_prune_level;
  ulong optimizer_search_depth;
  ulong preload_buff_size;
  ulong query_cache_type;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong div_precincrement;
  ulong sortbuff_size;
  ulong table_type;
  ulong tmp_table_size;
  ulong tx_isolation;
  ulong completion_type;
  /* Determines which non-standard SQL behaviour should be enabled */
  ulong sql_mode;
  /* check of key presence in updatable view */
  ulong updatable_views_with_limit;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong log_warnings;
  ulong group_concat_max_len;
  /*
    In slave thread we need to know in behalf of which
    thread the query is being run to replicate temp tables properly
  */
  ulong pseudo_thread_id;

  my_bool low_priority_updates;
  my_bool new_mode;
  my_bool query_cache_wlock_invalidate;
  my_bool engine_condition_pushdown;
#ifdef HAVE_REPLICATION
  ulong sync_replication;
  ulong sync_replication_slave_id;
  ulong sync_replication_timeout;
#endif /* HAVE_REPLICATION */
#ifdef HAVE_INNOBASE_DB
  my_bool innodb_table_locks;
  my_bool innodb_support_xa;
#endif /* HAVE_INNOBASE_DB */
#ifdef HAVE_NDBCLUSTER_DB
  ulong ndb_autoincrement_prefetch_sz;
  my_bool ndb_force_send;
  my_bool ndb_use_exact_count;
  my_bool ndb_use_transactions;
  my_bool ndb_index_stat_enable;
  ulong ndb_index_stat_cache_entries;
  ulong ndb_index_stat_update_freq;
#endif /* HAVE_NDBCLUSTER_DB */
  my_bool old_alter_table;
  my_bool old_passwords;

  /* Only charset part of these variables is sensible */
  CHARSET_INFO  *character_set_client;
  CHARSET_INFO  *character_set_results;

  /* Both charset and collation parts of these variables are important */
  CHARSET_INFO	*collation_server;
  CHARSET_INFO	*collation_database;
  CHARSET_INFO  *collation_connection;

  Time_zone *time_zone;

  /* DATE, DATETIME and TIME formats */
  DATE_TIME_FORMAT *date_format;
  DATE_TIME_FORMAT *datetime_format;
  DATE_TIME_FORMAT *time_format;
};


/* per thread status variables */

typedef struct system_status_var
{
  ulong bytes_received;
  ulong bytes_sent;
  ulong com_other;
  ulong com_stat[(uint) SQLCOM_END];
  ulong created_tmp_disk_tables;
  ulong created_tmp_tables;
  ulong ha_commit_count;
  ulong ha_delete_count;
  ulong ha_read_first_count;
  ulong ha_read_last_count;
  ulong ha_read_key_count;
  ulong ha_read_next_count;
  ulong ha_read_prev_count;
  ulong ha_read_rnd_count;
  ulong ha_read_rnd_next_count;
  ulong ha_rollback_count;
  ulong ha_update_count;
  ulong ha_write_count;
  ulong ha_prepare_count;
  ulong ha_discover_count;
  ulong ha_savepoint_count;
  ulong ha_savepoint_rollback_count;

  /* KEY_CACHE parts. These are copies of the original */
  ulong key_blocks_changed;
  ulong key_blocks_used;
  ulong key_cache_r_requests;
  ulong key_cache_read;
  ulong key_cache_w_requests;
  ulong key_cache_write;
  /* END OF KEY_CACHE parts */

  ulong net_big_packet_count;
  ulong opened_tables;
  ulong select_full_join_count;
  ulong select_full_range_join_count;
  ulong select_range_count;
  ulong select_range_check_count;
  ulong select_scan_count;
  ulong long_query_count;
  ulong filesort_merge_passes;
  ulong filesort_range_count;
  ulong filesort_rows;
  ulong filesort_scan_count;
  /* Prepared statements and binary protocol */
  ulong com_stmt_prepare;
  ulong com_stmt_execute;
  ulong com_stmt_send_long_data;
  ulong com_stmt_fetch;
  ulong com_stmt_reset;
  ulong com_stmt_close;

  double last_query_cost;
} STATUS_VAR;

/*
  This is used for 'show status'. It must be updated to the last ulong
  variable in system_status_var
*/

#define last_system_status_var com_stmt_close


void free_tmp_table(THD *thd, TABLE *entry);


/* The following macro is to make init of Query_arena simpler */
#ifndef DBUG_OFF
#define INIT_ARENA_DBUG_INFO is_backup_arena= 0
#else
#define INIT_ARENA_DBUG_INFO
#endif


class Query_arena
{
public:
  /*
    List of items created in the parser for this query. Every item puts
    itself to the list on creation (see Item::Item() for details))
  */
  Item *free_list;
  MEM_ROOT *mem_root;                   // Pointer to current memroot
#ifndef DBUG_OFF
  bool is_backup_arena; /* True if this arena is used for backup. */
#endif
  enum enum_state
  {
    INITIALIZED= 0, INITIALIZED_FOR_SP= 1, PREPARED= 2,
    CONVENTIONAL_EXECUTION= 3, EXECUTED= 4, ERROR= -1
  };

  enum_state state;

  /* We build without RTTI, so dynamic_cast can't be used. */
  enum Type
  {
    STATEMENT, PREPARED_STATEMENT, STORED_PROCEDURE
  };

  Query_arena(MEM_ROOT *mem_root_arg, enum enum_state state_arg) :
    free_list(0), mem_root(mem_root_arg), state(state_arg)
  { INIT_ARENA_DBUG_INFO; }
  /*
    This constructor is used only when Query_arena is created as
    backup storage for another instance of Query_arena.
  */
  Query_arena() { INIT_ARENA_DBUG_INFO; }

  virtual Type type() const;
  virtual ~Query_arena() {};

  inline bool is_stmt_prepare() const { return state == INITIALIZED; }
  inline bool is_first_sp_execute() const
  { return state == INITIALIZED_FOR_SP; }
  inline bool is_stmt_prepare_or_first_sp_execute() const
  { return (int)state < (int)PREPARED; }
  inline bool is_first_stmt_execute() const { return state == PREPARED; }
  inline bool is_stmt_execute() const
  { return state == PREPARED || state == EXECUTED; }
  inline bool is_conventional() const
  { return state == CONVENTIONAL_EXECUTION; }

  inline gptr alloc(unsigned int size) { return alloc_root(mem_root,size); }
  inline gptr calloc(unsigned int size)
  {
    gptr ptr;
    if ((ptr=alloc_root(mem_root,size)))
      bzero((char*) ptr,size);
    return ptr;
  }
  inline char *strdup(const char *str)
  { return strdup_root(mem_root,str); }
  inline char *strmake(const char *str, uint size)
  { return strmake_root(mem_root,str,size); }
  inline char *memdup(const char *str, uint size)
  { return memdup_root(mem_root,str,size); }
  inline char *memdup_w_gap(const char *str, uint size, uint gap)
  {
    gptr ptr;
    if ((ptr=alloc_root(mem_root,size+gap)))
      memcpy(ptr,str,size);
    return ptr;
  }

  void set_query_arena(Query_arena *set);

  void free_items();
  /* Close the active state associated with execution of this statement */
  virtual void cleanup_stmt();
};


class Server_side_cursor;

/*
  State of a single command executed against this connection.
  One connection can contain a lot of simultaneously running statements,
  some of which could be:
   - prepared, that is, contain placeholders,
   - opened as cursors. We maintain 1 to 1 relationship between
     statement and cursor - if user wants to create another cursor for his
     query, we create another statement for it. 
  To perform some action with statement we reset THD part to the state  of
  that statement, do the action, and then save back modified state from THD
  to the statement. It will be changed in near future, and Statement will
  be used explicitly.
*/

class Statement: public ilink, public Query_arena
{
  Statement(const Statement &rhs);              /* not implemented: */
  Statement &operator=(const Statement &rhs);   /* non-copyable */
public:
  /* FIXME: these must be protected */
  MEM_ROOT main_mem_root;
  LEX     main_lex;

  /*
    Uniquely identifies each statement object in thread scope; change during
    statement lifetime. FIXME: must be const
  */
   ulong id;

  /*
    - if set_query_id=1, we set field->query_id for all fields. In that case 
    field list can not contain duplicates.
    0: Means query_id is not set and no indicator to handler of fields used
       is set
    1: Means query_id is set for fields in list and bit in read set is set
       to inform handler of that field is to be read
    2: Means query is set for fields in list and bit is set in update set
       to inform handler that it needs to update this field in write_row
       and update_row
  */
  ulong set_query_id;
  /*
    This variable is used in post-parse stage to declare that sum-functions,
    or functions which have sense only if GROUP BY is present, are allowed.
    For example in queries
    SELECT MIN(i) FROM foo
    SELECT GROUP_CONCAT(a, b, MIN(i)) FROM ... GROUP BY ...
    MIN(i) have no sense.
    Though it's grammar-related issue, it's hard to catch it out during the
    parse stage because GROUP BY clause goes in the end of query. This
    variable is mainly used in setup_fields/fix_fields.
    See item_sum.cc for details.
  */
  bool allow_sum_func;

  LEX_STRING name; /* name for named prepared statements */
  LEX *lex;                                     // parse tree descriptor
  /*
    Points to the query associated with this statement. It's const, but
    we need to declare it char * because all table handlers are written
    in C and need to point to it.

    Note that (A) if we set query = NULL, we must at the same time set
    query_length = 0, and protect the whole operation with the
    LOCK_thread_count mutex. And (B) we are ONLY allowed to set query to a
    non-NULL value if its previous value is NULL. We do not need to protect
    operation (B) with any mutex. To avoid crashes in races, if we do not
    know that thd->query cannot change at the moment, one should print
    thd->query like this:
      (1) reserve the LOCK_thread_count mutex;
      (2) check if thd->query is NULL;
      (3) if not NULL, then print at most thd->query_length characters from
      it. We will see the query_length field as either 0, or the right value
      for it.
    Assuming that the write and read of an n-bit memory field in an n-bit
    computer is atomic, we can avoid races in the above way. 
    This printing is needed at least in SHOW PROCESSLIST and SHOW INNODB
    STATUS.
  */
  char *query;
  uint32 query_length;                          // current query length
  Server_side_cursor *cursor;

public:

  /* This constructor is called for backup statements */
  Statement() { clear_alloc_root(&main_mem_root); }

  Statement(enum enum_state state_arg, ulong id_arg,
            ulong alloc_block_size, ulong prealloc_size);
  virtual ~Statement();

  /* Assign execution context (note: not all members) of given stmt to self */
  void set_statement(Statement *stmt);
  void set_n_backup_statement(Statement *stmt, Statement *backup);
  void restore_backup_statement(Statement *stmt, Statement *backup);
  /* return class type */
  virtual Type type() const;
};


/*
  Container for all statements created/used in a connection.
  Statements in Statement_map have unique Statement::id (guaranteed by id
  assignment in Statement::Statement)
  Non-empty statement names are unique too: attempt to insert a new statement
  with duplicate name causes older statement to be deleted

  Statements are auto-deleted when they are removed from the map and when the
  map is deleted.
*/

class Statement_map
{
public:
  Statement_map();

  int insert(Statement *statement);

  Statement *find_by_name(LEX_STRING *name)
  {
    Statement *stmt;
    stmt= (Statement*)hash_search(&names_hash, (byte*)name->str,
                                  name->length);
    return stmt;
  }

  Statement *find(ulong id)
  {
    if (last_found_statement == 0 || id != last_found_statement->id)
    {
      Statement *stmt;
      stmt= (Statement *) hash_search(&st_hash, (byte *) &id, sizeof(id));
      if (stmt && stmt->name.str)
        return NULL;
      last_found_statement= stmt;
    }
    return last_found_statement;
  }
  void erase(Statement *statement)
  {
    if (statement == last_found_statement)
      last_found_statement= 0;
    if (statement->name.str)
    {
      hash_delete(&names_hash, (byte *) statement);  
    }
    hash_delete(&st_hash, (byte *) statement);
  }
  /*
    Close all cursors of this connection that use tables of a storage
    engine that has transaction-specific state and therefore can not
    survive COMMIT or ROLLBACK. Currently all but MyISAM cursors are closed.
  */
  void close_transient_cursors();
  /* Erase all statements (calls Statement destructor) */
  void reset()
  {
    my_hash_reset(&names_hash);
    my_hash_reset(&st_hash);
    transient_cursor_list.empty();
    last_found_statement= 0;
  }

  void destroy()
  {
    hash_free(&names_hash);
    hash_free(&st_hash);
  }
private:
  HASH st_hash;
  HASH names_hash;
  I_List<Statement> transient_cursor_list;
  Statement *last_found_statement;
};

struct st_savepoint {
  struct st_savepoint *prev;
  char                *name;
  uint                 length, nht;
};

enum xa_states {XA_NOTR=0, XA_ACTIVE, XA_IDLE, XA_PREPARED};
extern const char *xa_state_names[];

typedef struct st_xid_state {
  /* For now, this is only used to catch duplicated external xids */
  XID  xid;                           // transaction identifier
  enum xa_states xa_state;            // used by external XA only
  bool in_thd;
} XID_STATE;

extern pthread_mutex_t LOCK_xid_cache;
extern HASH xid_cache;
bool xid_cache_init(void);
void xid_cache_free(void);
XID_STATE *xid_cache_search(XID *xid);
bool xid_cache_insert(XID *xid, enum xa_states xa_state);
bool xid_cache_insert(XID_STATE *xid_state);
void xid_cache_delete(XID_STATE *xid_state);


class Security_context {
public:
  /*
    host - host of the client
    user - user of the client, set to NULL until the user has been read from
    the connection
    priv_user - The user privilege we are using. May be "" for anonymous user.
    ip - client IP
  */
  char   *host, *user, *priv_user, *ip;
  /* The host privilege we are using */
  char   priv_host[MAX_HOSTNAME];
  /* points to host if host is available, otherwise points to ip */
  const char *host_or_ip;
  ulong master_access;                 /* Global privileges from mysql.user */
  ulong db_access;                     /* Privileges for current db */

  void init();
  void destroy();
  void skip_grants();
  inline char *priv_host_name()
  {
    return (*priv_host ? priv_host : (char *)"%");
  }
};


/*
  A registry for item tree transformations performed during
  query optimization. We register only those changes which require
  a rollback to re-execute a prepared statement or stored procedure
  yet another time.
*/

struct Item_change_record;
typedef I_List<Item_change_record> Item_change_list;


/*
  Type of prelocked mode.
  See comment for THD::prelocked_mode for complete description.
*/

enum prelocked_mode_type {NON_PRELOCKED= 0, PRELOCKED= 1,
                          PRELOCKED_UNDER_LOCK_TABLES= 2};


/*
  Class that holds information about tables which were opened and locked
  by the thread. It is also used to save/restore this information in
  push_open_tables_state()/pop_open_tables_state().
*/

class Open_tables_state
{
public:
  /*
    open_tables - list of regular tables in use by this thread
    temporary_tables - list of temp tables in use by this thread
    handler_tables - list of tables that were opened with HANDLER OPEN
     and are still in use by this thread
  */
  TABLE *open_tables, *temporary_tables, *handler_tables, *derived_tables;
  /*
    During a MySQL session, one can lock tables in two modes: automatic
    or manual. In automatic mode all necessary tables are locked just before
    statement execution, and all acquired locks are stored in 'lock'
    member. Unlocking takes place automatically as well, when the
    statement ends.
    Manual mode comes into play when a user issues a 'LOCK TABLES'
    statement. In this mode the user can only use the locked tables.
    Trying to use any other tables will give an error. The locked tables are
    stored in 'locked_tables' member.  Manual locking is described in
    the 'LOCK_TABLES' chapter of the MySQL manual.
    See also lock_tables() for details.
  */
  MYSQL_LOCK *lock;
  /*
    Tables that were locked with explicit or implicit LOCK TABLES.
    (Implicit LOCK TABLES happens when we are prelocking tables for
     execution of statement which uses stored routines. See description
     THD::prelocked_mode for more info.)
  */
  MYSQL_LOCK *locked_tables;
  /*
    prelocked_mode_type enum and prelocked_mode member are used for
    indicating whenever "prelocked mode" is on, and what type of
    "prelocked mode" is it.

    Prelocked mode is used for execution of queries which explicitly
    or implicitly (via views or triggers) use functions, thus may need
    some additional tables (mentioned in query table list) for their
    execution.

    First open_tables() call for such query will analyse all functions
    used by it and add all additional tables to table its list. It will
    also mark this query as requiring prelocking. After that lock_tables()
    will issue implicit LOCK TABLES for the whole table list and change
    thd::prelocked_mode to non-0. All queries called in functions invoked
    by the main query will use prelocked tables. Non-0 prelocked_mode
    will also surpress mentioned analysys in those queries thus saving
    cycles. Prelocked mode will be turned off once close_thread_tables()
    for the main query will be called.

    Note: Since not all "tables" present in table list are really locked
    thd::prelocked_mode does not imply thd::locked_tables.
  */
  prelocked_mode_type prelocked_mode;
  ulong	version;
  uint current_tablenr;

  /*
    This constructor serves for creation of Open_tables_state instances
    which are used as backup storage.
  */
  Open_tables_state() {};

  Open_tables_state(ulong version_arg);

  void set_open_tables_state(Open_tables_state *state)
  {
    *this= *state;
  }

  void reset_open_tables_state()
  {
    open_tables= temporary_tables= handler_tables= derived_tables= 0;
    lock= locked_tables= 0;
    prelocked_mode= NON_PRELOCKED;
  }
};


/* class to save context when executing a function or trigger */

/* Defines used for Sub_statement_state::in_sub_stmt */

#define SUB_STMT_TRIGGER 1
#define SUB_STMT_FUNCTION 2

class Sub_statement_state
{
public:
  ulonglong options;
  ulonglong last_insert_id, next_insert_id;
  ulonglong limit_found_rows;
  ha_rows    cuted_fields, sent_row_count, examined_row_count;
  ulong client_capabilities;
  uint in_sub_stmt;
  bool enable_slow_log, insert_id_used;
  my_bool no_send_ok;
};


/*
  For each client connection we create a separate thread with THD serving as
  a thread/connection descriptor
*/

class THD :public Statement,
           public Open_tables_state
{
public:
#ifdef EMBEDDED_LIBRARY
  struct st_mysql  *mysql;
  struct st_mysql_data *data;
  unsigned long	 client_stmt_id;
  unsigned long  client_param_count;
  struct st_mysql_bind *client_params;
  char *extra_data;
  ulong extra_length;
  String query_rest;
#endif
  NET	  net;				// client connection descriptor
  MEM_ROOT warn_root;			// For warnings and errors
  Protocol *protocol;			// Current protocol
  Protocol_simple protocol_simple;	// Normal protocol
  Protocol_prep protocol_prep;		// Binary protocol
  HASH    user_vars;			// hash for user variables
  String  packet;			// dynamic buffer for network I/O
  String  convert_buffer;               // buffer for charset conversions
  struct  sockaddr_in remote;		// client socket address
  struct  rand_struct rand;		// used for authentication
  struct  system_variables variables;	// Changeable local variables
  struct  system_status_var status_var; // Per thread statistic vars
  THR_LOCK_INFO lock_info;              // Locking info of this thread
  THR_LOCK_OWNER main_lock_id;          // To use for conventional queries
  THR_LOCK_OWNER *lock_id;              // If not main_lock_id, points to
                                        // the lock_id of a cursor.
  pthread_mutex_t LOCK_delete;		// Locked before thd is deleted
  /* all prepared statements and cursors of this connection */
  Statement_map stmt_map;
  /*
    A pointer to the stack frame of handle_one_connection(),
    which is called first in the thread for handling a client
  */
  char	  *thread_stack;

  /*
    db - currently selected database
    catalog - currently selected catalog
    WARNING: some members of THD (currently 'db', 'catalog' and 'query')  are
    set and alloced by the slave SQL thread (for the THD of that thread); that
    thread is (and must remain, for now) the only responsible for freeing these
    3 members. If you add members here, and you add code to set them in
    replication, don't forget to free_them_and_set_them_to_0 in replication
    properly. For details see the 'err:' label of the handle_slave_sql()
    in sql/slave.cc.
   */
  char   *db, *catalog;
  Security_context main_security_ctx;
  Security_context *security_ctx;

  /* remote (peer) port */
  uint16 peer_port;
  /*
    Points to info-string that we show in SHOW PROCESSLIST
    You are supposed to update thd->proc_info only if you have coded
    a time-consuming piece that MySQL can get stuck in for a long time.
  */
  const char *proc_info;

  ulong client_capabilities;		/* What the client supports */
  ulong max_client_packet_length;

  HASH		handler_tables_hash;
  /*
    One thread can hold up to one named user-level lock. This variable
    points to a lock object if the lock is present. See item_func.cc and
    chapter 'Miscellaneous functions', for functions GET_LOCK, RELEASE_LOCK. 
  */
  User_level_lock *ull;
#ifndef DBUG_OFF
  uint dbug_sentry; // watch out for memory corruption
#endif
  struct st_my_thread_var *mysys_var;
  /*
    Type of current query: COM_STMT_PREPARE, COM_QUERY, etc. Set from
    first byte of the packet in do_command()
  */
  enum enum_server_command command;
  uint32     server_id;
  uint32     file_id;			// for LOAD DATA INFILE
  /*
    Used in error messages to tell user in what part of MySQL we found an
    error. E. g. when where= "having clause", if fix_fields() fails, user
    will know that the error was in having clause.
  */
  const char *where;
  time_t     start_time,time_after_lock,user_time;
  time_t     connect_time,thr_create_time; // track down slow pthread_create
  thr_lock_type update_lock_default;
  delayed_insert *di;

  /* <> 0 if we are inside of trigger or stored function. */
  uint in_sub_stmt;

  /* container for handler's private per-connection data */
  void *ha_data[MAX_HA];
  struct st_transactions {
    SAVEPOINT *savepoints;
    THD_TRANS all;			// Trans since BEGIN WORK
    THD_TRANS stmt;			// Trans for current statement
    bool on;                            // see ha_enable_transaction()
    XID_STATE xid_state;
    /*
       Tables changed in transaction (that must be invalidated in query cache).
       List contain only transactional tables, that not invalidated in query
       cache (instead of full list of changed in transaction tables).
    */
    CHANGED_TABLE_LIST* changed_tables;
    MEM_ROOT mem_root; // Transaction-life memory allocation pool
    void cleanup()
    {
      changed_tables= 0;
      savepoints= 0;
#ifdef USING_TRANSACTIONS
      free_root(&mem_root,MYF(MY_KEEP_PREALLOC));
#endif
    }
#ifdef USING_TRANSACTIONS
    st_transactions()
    {
      bzero((char*)this, sizeof(*this));
      xid_state.xid.null();
      init_sql_alloc(&mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
    }
#endif
  } transaction;
  Field      *dupp_field;
#ifndef __WIN__
  sigset_t signals,block_signals;
#endif
#ifdef SIGNAL_WITH_VIO_CLOSE
  Vio* active_vio;
#endif
  /*
    This is to track items changed during execution of a prepared
    statement/stored procedure. It's created by
    register_item_tree_change() in memory root of THD, and freed in
    rollback_item_tree_changes(). For conventional execution it's always
    empty.
  */
  Item_change_list change_list;

  /*
    A permanent memory area of the statement. For conventional
    execution, the parsed tree and execution runtime reside in the same
    memory root. In this case stmt_arena points to THD. In case of
    a prepared statement or a stored procedure statement, thd->mem_root
    conventionally points to runtime memory, and thd->stmt_arena
    points to the memory of the PS/SP, where the parsed tree of the
    statement resides. Whenever you need to perform a permanent
    transformation of a parsed tree, you should allocate new memory in
    stmt_arena, to allow correct re-execution of PS/SP.
    Note: in the parser, stmt_arena == thd, even for PS/SP.
  */
  Query_arena *stmt_arena;
  /*
    next_insert_id is set on SET INSERT_ID= #. This is used as the next
    generated auto_increment value in handler.cc
  */
  ulonglong  next_insert_id;
  /* Remember last next_insert_id to reset it if something went wrong */
  ulonglong  prev_insert_id;
  /*
    The insert_id used for the last statement or set by SET LAST_INSERT_ID=#
    or SELECT LAST_INSERT_ID(#).  Used for binary log and returned by
    LAST_INSERT_ID()
  */
  ulonglong  last_insert_id;
  /*
    Set to the first value that LAST_INSERT_ID() returned for the last
    statement.  When this is set, last_insert_id_used is set to true.
  */
  ulonglong  current_insert_id;
  ulonglong  limit_found_rows;
  ulonglong  options;           /* Bitmap of states */
  longlong   row_count_func;	/* For the ROW_COUNT() function */
  ha_rows    cuted_fields,
             sent_row_count, examined_row_count;
  /*
    The set of those tables whose fields are referenced in all subqueries
    of the query.
    TODO: possibly this it is incorrect to have used tables in THD because
    with more than one subquery, it is not clear what does the field mean.
  */
  table_map  used_tables;
  USER_CONN *user_connect;
  CHARSET_INFO *db_charset;
  /*
    FIXME: this, and some other variables like 'count_cuted_fields'
    maybe should be statement/cursor local, that is, moved to Statement
    class. With current implementation warnings produced in each prepared
    statement/cursor settle here.
  */
  List	     <MYSQL_ERROR> warn_list;
  uint	     warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_END];
  uint	     total_warn_count;
  /*
    Id of current query. Statement can be reused to execute several queries
    query_id is global in context of the whole MySQL server.
    ID is automatically generated from mutex-protected counter.
    It's used in handler code for various purposes: to check which columns
    from table are necessary for this select, to check if it's necessary to
    update auto-updatable fields (like auto_increment and timestamp).
  */
  query_id_t query_id, warn_id;
  ulong      thread_id, col_access;

  /* Statement id is thread-wide. This counter is used to generate ids */
  ulong      statement_id_counter;
  ulong	     rand_saved_seed1, rand_saved_seed2;
  ulong      row_count;  // Row counter, mainly for errors and warnings
  long	     dbug_thread_id;
  pthread_t  real_id;
  uint	     tmp_table, global_read_lock;
  uint	     server_status,open_options,system_thread;
  uint32     db_length;
  uint       select_number;             //number of select (used for EXPLAIN)
  /* variables.transaction_isolation is reset to this after each commit */
  enum_tx_isolation session_tx_isolation;
  enum_check_fields count_cuted_fields;

  DYNAMIC_ARRAY user_var_events;        /* For user variables replication */
  MEM_ROOT      *user_var_events_alloc; /* Allocate above array elements here */

  enum killed_state { NOT_KILLED=0, KILL_BAD_DATA=1, KILL_CONNECTION=ER_SERVER_SHUTDOWN, KILL_QUERY=ER_QUERY_INTERRUPTED };
  killed_state volatile killed;

  /* scramble - random string sent to client on handshake */
  char	     scramble[SCRAMBLE_LENGTH+1];

  bool       slave_thread, one_shot_set;
  bool	     locked, some_tables_deleted;
  bool       last_cuted_field;
  bool	     no_errors, password, is_fatal_error;
  bool	     query_start_used, rand_used, time_zone_used;
  bool	     last_insert_id_used,insert_id_used, clear_next_insert_id;
  bool	     in_lock_tables;
  bool       query_error, bootstrap, cleanup_done;
  bool	     tmp_table_used;
  bool	     charset_is_system_charset, charset_is_collation_connection;
  bool       enable_slow_log;   /* enable slow log for current statement */
  bool	     no_trans_update, abort_on_warning;
  bool 	     got_warning;       /* Set on call to push_warning() */
  bool	     no_warnings_for_error; /* no warnings on call to my_error() */
  /* set during loop of derived table processing */
  bool       derived_tables_processing;
  my_bool    tablespace_op;	/* This is TRUE in DISCARD/IMPORT TABLESPACE */

  sp_rcontext *spcont;		// SP runtime context
  sp_cache   *sp_proc_cache;
  sp_cache   *sp_func_cache;

  /*
    If we do a purge of binary logs, log index info of the threads
    that are currently reading it needs to be adjusted. To do that
    each thread that is using LOG_INFO needs to adjust the pointer to it
  */
  LOG_INFO*  current_linfo;
  NET*       slave_net;			// network connection from slave -> m.
  /* Used by the sys_var class to store temporary values */
  union
  {
    my_bool my_bool_value;
    long    long_value;
  } sys_var_tmp;
  
  struct {
    /* 
      If true, mysql_bin_log::write(Log_event) call will not write events to 
      binlog, and maintain 2 below variables instead (use
      mysql_bin_log.start_union_events to turn this on)
    */
    bool do_union;
    /*
      If TRUE, at least one mysql_bin_log::write(Log_event) call has been
      made after last mysql_bin_log.start_union_events() call.
    */
    bool unioned_events;
    /*
      If TRUE, at least one mysql_bin_log::write(Log_event e), where 
      e.cache_stmt == TRUE call has been made after last 
      mysql_bin_log.start_union_events() call.
    */
    bool unioned_events_trans;
    
    /* 
      'queries' (actually SP statements) that run under inside this binlog
      union have thd->query_id >= first_query_id.
    */
    query_id_t first_query_id;
  } binlog_evt_union;
  
  THD();
  ~THD();

  void init(void);
  /*
    Initialize memory roots necessary for query processing and (!)
    pre-allocate memory for it. We can't do that in THD constructor because
    there are use cases (acl_init, delayed inserts, watcher threads,
    killing mysqld) where it's vital to not allocate excessive and not used
    memory. Note, that we still don't return error from init_for_queries():
    if preallocation fails, we should notice that at the first call to
    alloc_root. 
  */
  void init_for_queries();
  void change_user(void);
  void cleanup(void);
  void cleanup_after_query();
  bool store_globals();
#ifdef SIGNAL_WITH_VIO_CLOSE
  inline void set_active_vio(Vio* vio)
  {
    pthread_mutex_lock(&LOCK_delete);
    active_vio = vio;
    pthread_mutex_unlock(&LOCK_delete);
  }
  inline void clear_active_vio()
  {
    pthread_mutex_lock(&LOCK_delete);
    active_vio = 0;
    pthread_mutex_unlock(&LOCK_delete);
  }
  void close_active_vio();
#endif
  void awake(THD::killed_state state_to_set);
  /*
    For enter_cond() / exit_cond() to work the mutex must be got before
    enter_cond(); this mutex is then released by exit_cond().
    Usage must be: lock mutex; enter_cond(); your code; exit_cond().
  */
  inline const char* enter_cond(pthread_cond_t *cond, pthread_mutex_t* mutex,
			  const char* msg)
  {
    const char* old_msg = proc_info;
    safe_mutex_assert_owner(mutex);
    mysys_var->current_mutex = mutex;
    mysys_var->current_cond = cond;
    proc_info = msg;
    return old_msg;
  }
  inline void exit_cond(const char* old_msg)
  {
    /*
      Putting the mutex unlock in exit_cond() ensures that
      mysys_var->current_mutex is always unlocked _before_ mysys_var->mutex is
      locked (if that would not be the case, you'll get a deadlock if someone
      does a THD::awake() on you).
    */
    pthread_mutex_unlock(mysys_var->current_mutex);
    pthread_mutex_lock(&mysys_var->mutex);
    mysys_var->current_mutex = 0;
    mysys_var->current_cond = 0;
    proc_info = old_msg;
    pthread_mutex_unlock(&mysys_var->mutex);
  }
  inline time_t query_start() { query_start_used=1; return start_time; }
  inline void	set_time()    { if (user_time) start_time=time_after_lock=user_time; else time_after_lock=time(&start_time); }
  inline void	end_time()    { time(&start_time); }
  inline void	set_time(time_t t) { time_after_lock=start_time=user_time=t; }
  inline void	lock_time()   { time(&time_after_lock); }
  inline void	insert_id(ulonglong id_arg)
  {
    last_insert_id= id_arg;
    insert_id_used=1;
  }
  inline ulonglong insert_id(void)
  {
    if (!last_insert_id_used)
    {
      last_insert_id_used=1;
      current_insert_id=last_insert_id;
    }
    return last_insert_id;
  }
  inline ulonglong found_rows(void)
  {
    return limit_found_rows;
  }
  inline bool active_transaction()
  {
#ifdef USING_TRANSACTIONS
    return server_status & SERVER_STATUS_IN_TRANS;
#else
    return 0;
#endif
  }
  inline bool fill_derived_tables()
  {
    return !stmt_arena->is_stmt_prepare() && !lex->only_view_structure();
  }
  inline gptr trans_alloc(unsigned int size)
  {
    return alloc_root(&transaction.mem_root,size);
  }

  bool convert_string(LEX_STRING *to, CHARSET_INFO *to_cs,
		      const char *from, uint from_length,
		      CHARSET_INFO *from_cs);

  bool convert_string(String *s, CHARSET_INFO *from_cs, CHARSET_INFO *to_cs);

  void add_changed_table(TABLE *table);
  void add_changed_table(const char *key, long key_length);
  CHANGED_TABLE_LIST * changed_table_dup(const char *key, long key_length);
  int send_explain_fields(select_result *result);
#ifndef EMBEDDED_LIBRARY
  inline void clear_error()
  {
    net.last_error[0]= 0;
    net.last_errno= 0;
    net.report_error= 0;
    query_error= 0;
  }
  inline bool vio_ok() const { return net.vio != 0; }
#else
  void clear_error();
  inline bool vio_ok() const { return true; }
#endif
  inline void fatal_error()
  {
    is_fatal_error= 1;
    net.report_error= 1;
    DBUG_PRINT("error",("Fatal error set"));
  }
  inline CHARSET_INFO *charset() { return variables.character_set_client; }
  void update_charset();

  inline Query_arena *activate_stmt_arena_if_needed(Query_arena *backup)
  {
    /*
      Use the persistent arena if we are in a prepared statement or a stored
      procedure statement and we have not already changed to use this arena.
    */
    if (!stmt_arena->is_conventional() && mem_root != stmt_arena->mem_root)
    {
      set_n_backup_active_arena(stmt_arena, backup);
      return stmt_arena;
    }
    return 0;
  }

  void change_item_tree(Item **place, Item *new_value)
  {
    /* TODO: check for OOM condition here */
    if (!stmt_arena->is_conventional())
      nocheck_register_item_tree_change(place, *place, mem_root);
    *place= new_value;
  }
  void nocheck_register_item_tree_change(Item **place, Item *old_value,
                                         MEM_ROOT *runtime_memroot);
  void rollback_item_tree_changes();

  /*
    Cleanup statement parse state (parse tree, lex) and execution
    state after execution of a non-prepared SQL statement.
  */
  void end_statement();
  inline int killed_errno() const
  {
    return killed != KILL_BAD_DATA ? killed : 0;
  }
  inline void send_kill_message() const
  {
    int err= killed_errno();
    if (err)
      my_message(err, ER(err), MYF(0));
  }
  /* return TRUE if we will abort query if we make a warning now */
  inline bool really_abort_on_warning()
  {
    return (abort_on_warning &&
            (!no_trans_update ||
             (variables.sql_mode & MODE_STRICT_ALL_TABLES)));
  }
  void set_status_var_init();
  bool is_context_analysis_only()
    { return stmt_arena->is_stmt_prepare() || lex->view_prepare_mode; }
  void reset_n_backup_open_tables_state(Open_tables_state *backup);
  void restore_backup_open_tables_state(Open_tables_state *backup);
  void reset_sub_statement_state(Sub_statement_state *backup, uint new_state);
  void restore_sub_statement_state(Sub_statement_state *backup);
  void set_n_backup_active_arena(Query_arena *set, Query_arena *backup);
  void restore_active_arena(Query_arena *set, Query_arena *backup);
};


#define tmp_disable_binlog(A)       \
  {ulonglong tmp_disable_binlog__save_options= (A)->options; \
  (A)->options&= ~OPTION_BIN_LOG

#define reenable_binlog(A)   (A)->options= tmp_disable_binlog__save_options;}

/* Flags for the THD::system_thread (bitmap) variable */
#define SYSTEM_THREAD_DELAYED_INSERT 1
#define SYSTEM_THREAD_SLAVE_IO 2
#define SYSTEM_THREAD_SLAVE_SQL 4

/*
  Used to hold information about file and file structure in exchainge 
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class sql_exchange :public Sql_alloc
{
public:
  char *file_name;
  String *field_term,*enclosed,*line_term,*line_start,*escaped;
  bool opt_enclosed;
  bool dumpfile;
  ulong skip_lines;
  sql_exchange(char *name,bool dumpfile_flag);
};

#include "log_event.h"

/*
  This is used to get result from a select
*/

class JOIN;

class select_result :public Sql_alloc {
protected:
  THD *thd;
  SELECT_LEX_UNIT *unit;
public:
  select_result();
  virtual ~select_result() {};
  virtual int prepare(List<Item> &list, SELECT_LEX_UNIT *u)
  {
    unit= u;
    return 0;
  }
  virtual int prepare2(void) { return 0; }
  /*
    Because of peculiarities of prepared statements protocol
    we need to know number of columns in the result set (if
    there is a result set) apart from sending columns metadata.
  */
  virtual uint field_count(List<Item> &fields) const
  { return fields.elements; }
  virtual bool send_fields(List<Item> &list, uint flags)=0;
  virtual bool send_data(List<Item> &items)=0;
  virtual bool initialize_tables (JOIN *join=0) { return 0; }
  virtual void send_error(uint errcode,const char *err);
  virtual bool send_eof()=0;
  virtual bool simple_select() { return 0; }
  virtual void abort() {}
  /*
    Cleanup instance of this class for next execution of a prepared
    statement/stored procedure.
  */
  virtual void cleanup();
  void set_thd(THD *thd_arg) { thd= thd_arg; }
};


/*
  Base class for select_result descendands which intercept and
  transform result set rows. As the rows are not sent to the client,
  sending of result set metadata should be suppressed as well.
*/

class select_result_interceptor: public select_result
{
public:
  uint field_count(List<Item> &fields) const { return 0; }
  bool send_fields(List<Item> &fields, uint flag) { return FALSE; }
};


class select_send :public select_result {
  int status;
public:
  select_send() :status(0) {}
  bool send_fields(List<Item> &list, uint flags);
  bool send_data(List<Item> &items);
  bool send_eof();
  bool simple_select() { return 1; }
  void abort();
};


class select_to_file :public select_result_interceptor {
protected:
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  char path[FN_REFLEN];

public:
  select_to_file(sql_exchange *ex) :exchange(ex), file(-1),row_count(0L)
  { path[0]=0; }
  ~select_to_file();
  void send_error(uint errcode,const char *err);
  bool send_eof();
  void cleanup();
};


class select_export :public select_to_file {
  uint field_term_length;
  int field_sep_char,escape_char,line_sep_char;
  bool fixed_row_size;
public:
  select_export(sql_exchange *ex) :select_to_file(ex) {}
  ~select_export();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class select_dump :public select_to_file {
public:
  select_dump(sql_exchange *ex) :select_to_file(ex) {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class select_insert :public select_result_interceptor {
 public:
  TABLE_LIST *table_list;
  TABLE *table;
  List<Item> *fields;
  ulonglong last_insert_id;
  COPY_INFO info;
  bool insert_into_view;

  select_insert(TABLE_LIST *table_list_par,
		TABLE *table_par, List<Item> *fields_par,
		List<Item> *update_fields, List<Item> *update_values,
		enum_duplicates duplic, bool ignore);
  ~select_insert();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  int prepare2(void);
  bool send_data(List<Item> &items);
  virtual void store_values(List<Item> &values);
  void send_error(uint errcode,const char *err);
  bool send_eof();
  /* not implemented: select_insert is never re-used in prepared statements */
  void cleanup();
};


class select_create: public select_insert {
  ORDER *group;
  TABLE_LIST *create_table;
  List<create_field> *extra_fields;
  List<Key> *keys;
  HA_CREATE_INFO *create_info;
  MYSQL_LOCK *lock;
  Field **field;
public:
  select_create (TABLE_LIST *table,
		 HA_CREATE_INFO *create_info_par,
		 List<create_field> &fields_par,
		 List<Key> &keys_par,
		 List<Item> &select_fields,enum_duplicates duplic, bool ignore)
    :select_insert (NULL, NULL, &select_fields, 0, 0, duplic, ignore), create_table(table),
    extra_fields(&fields_par),keys(&keys_par), create_info(create_info_par),
    lock(0)
    {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  void store_values(List<Item> &values);
  void send_error(uint errcode,const char *err);
  bool send_eof();
  void abort();
};

#include <myisam.h>

/* 
  Param to create temporary tables when doing SELECT:s 
  NOTE
    This structure is copied using memcpy as a part of JOIN.
*/

class TMP_TABLE_PARAM :public Sql_alloc
{
private:
  /* Prevent use of these (not safe because of lists and copy_field) */
  TMP_TABLE_PARAM(const TMP_TABLE_PARAM &);
  void operator=(TMP_TABLE_PARAM &);

public:
  List<Item> copy_funcs;
  List<Item> save_copy_funcs;
  Copy_field *copy_field, *copy_field_end;
  Copy_field *save_copy_field, *save_copy_field_end;
  byte	    *group_buff;
  Item	    **items_to_copy;			/* Fields in tmp table */
  MI_COLUMNDEF *recinfo,*start_recinfo;
  KEY *keyinfo;
  ha_rows end_write_records;
  uint	field_count,sum_func_count,func_count;
  uint  hidden_field_count;
  uint	group_parts,group_length,group_null_parts;
  uint	quick_group;
  bool  using_indirect_summary_function;
  /* If >0 convert all blob fields to varchar(convert_blob_length) */
  uint  convert_blob_length; 
  CHARSET_INFO *table_charset; 
  bool schema_table;

  TMP_TABLE_PARAM()
    :copy_field(0), group_parts(0),
     group_length(0), group_null_parts(0), convert_blob_length(0),
     schema_table(0)
  {}
  ~TMP_TABLE_PARAM()
  {
    cleanup();
  }
  void init(void);
  inline void cleanup(void)
  {
    if (copy_field)				/* Fix for Intel compiler */
    {
      delete [] copy_field;
      save_copy_field= copy_field= 0;
    }
  }
};

class select_union :public select_result_interceptor
{
  TMP_TABLE_PARAM tmp_table_param;
public:
  TABLE *table;

  select_union() :table(0) {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  bool flush();

  bool create_result_table(THD *thd, List<Item> *column_types,
                           bool is_distinct, ulonglong options,
                           const char *alias);
};

/* Base subselect interface class */
class select_subselect :public select_result_interceptor
{
protected:
  Item_subselect *item;
public:
  select_subselect(Item_subselect *item);
  bool send_data(List<Item> &items)=0;
  bool send_eof() { return 0; };
};

/* Single value subselect interface class */
class select_singlerow_subselect :public select_subselect
{
public:
  select_singlerow_subselect(Item_subselect *item):select_subselect(item){}
  bool send_data(List<Item> &items);
};

/* used in independent ALL/ANY optimisation */
class select_max_min_finder_subselect :public select_subselect
{
  Item_cache *cache;
  bool (select_max_min_finder_subselect::*op)();
  bool fmax;
public:
  select_max_min_finder_subselect(Item_subselect *item, bool mx)
    :select_subselect(item), cache(0), fmax(mx)
  {}
  void cleanup();
  bool send_data(List<Item> &items);
  bool cmp_real();
  bool cmp_int();
  bool cmp_decimal();
  bool cmp_str();
};

/* EXISTS subselect interface class */
class select_exists_subselect :public select_subselect
{
public:
  select_exists_subselect(Item_subselect *item):select_subselect(item){}
  bool send_data(List<Item> &items);
};

/* Structs used when sorting */

typedef struct st_sort_field {
  Field *field;				/* Field to sort */
  Item	*item;				/* Item if not sorting fields */
  uint	 length;			/* Length of sort field */
  Item_result result_type;		/* Type of item */
  bool reverse;				/* if descending sort */
  bool need_strxnfrm;			/* If we have to use strxnfrm() */
} SORT_FIELD;


typedef struct st_sort_buffer {
  uint index;					/* 0 or 1 */
  uint sort_orders;
  uint change_pos;				/* If sort-fields changed */
  char **buff;
  SORT_FIELD *sortorder;
} SORT_BUFFER;

/* Structure for db & table in sql_yacc */

class Table_ident :public Sql_alloc
{
 public:
  LEX_STRING db;
  LEX_STRING table;
  SELECT_LEX_UNIT *sel;
  inline Table_ident(THD *thd, LEX_STRING db_arg, LEX_STRING table_arg,
		     bool force)
    :table(table_arg), sel((SELECT_LEX_UNIT *)0)
  {
    if (!force && (thd->client_capabilities & CLIENT_NO_SCHEMA))
      db.str=0;
    else
      db= db_arg;
  }
  inline Table_ident(LEX_STRING table_arg) 
    :table(table_arg), sel((SELECT_LEX_UNIT *)0)
  {
    db.str=0;
  }
  inline Table_ident(SELECT_LEX_UNIT *s) : sel(s) 
  {
    /* We must have a table name here as this is used with add_table_to_list */
    db.str=0; table.str= internal_table_name; table.length=1;
  }
  inline void change_db(char *db_name)
  {
    db.str= db_name; db.length= (uint) strlen(db_name);
  }
};

// this is needed for user_vars hash
class user_var_entry
{
 public:
  LEX_STRING name;
  char *value;
  ulong length;
  query_id_t update_query_id, used_query_id;
  Item_result type;

  double val_real(my_bool *null_value);
  longlong val_int(my_bool *null_value);
  String *val_str(my_bool *null_value, String *str, uint decimals);
  my_decimal *val_decimal(my_bool *null_value, my_decimal *result);
  DTCollation collation;
};

/*
   Unique -- class for unique (removing of duplicates). 
   Puts all values to the TREE. If the tree becomes too big,
   it's dumped to the file. User can request sorted values, or
   just iterate through them. In the last case tree merging is performed in
   memory simultaneously with iteration, so it should be ~2-3x faster.
 */

class Unique :public Sql_alloc
{
  DYNAMIC_ARRAY file_ptrs;
  ulong max_elements, max_in_memory_size;
  IO_CACHE file;
  TREE tree;
  byte *record_pointers;
  bool flush();
  uint size;

public:
  ulong elements;
  Unique(qsort_cmp2 comp_func, void *comp_func_fixed_arg,
	 uint size_arg, ulong max_in_memory_size_arg);
  ~Unique();
  ulong elements_in_tree() { return tree.elements_in_tree; }
  inline bool unique_add(void *ptr)
  {
    DBUG_ENTER("unique_add");
    DBUG_PRINT("info", ("tree %u - %u", tree.elements_in_tree, max_elements));
    if (tree.elements_in_tree > max_elements && flush())
      DBUG_RETURN(1);
    DBUG_RETURN(!tree_insert(&tree, ptr, 0, tree.custom_arg));
  }

  bool get(TABLE *table);
  static double get_use_cost(uint *buffer, uint nkeys, uint key_size, 
                             ulong max_in_memory_size);
  inline static int get_cost_calc_buff_size(ulong nkeys, uint key_size, 
                                            ulong max_in_memory_size)
  {
    register ulong max_elems_in_tree= 
      (1 + max_in_memory_size / ALIGN_SIZE(sizeof(TREE_ELEMENT)+key_size));
    return sizeof(uint)*(1 + nkeys/max_elems_in_tree);
  }

  void reset();
  bool walk(tree_walk_action action, void *walk_action_arg);

  friend int unique_write_to_file(gptr key, element_count count, Unique *unique);
  friend int unique_write_to_ptrs(gptr key, element_count count, Unique *unique);
};


class multi_delete :public select_result_interceptor
{
  TABLE_LIST *delete_tables, *table_being_deleted;
  Unique **tempfiles;
  ha_rows deleted, found;
  uint num_of_tables;
  int error;
  bool do_delete;
  /* True if at least one table we delete from is transactional */
  bool transactional_tables;
  /* True if at least one table we delete from is not transactional */
  bool normal_tables;
  bool delete_while_scanning;

public:
  multi_delete(TABLE_LIST *dt, uint num_of_tables);
  ~multi_delete();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_deletes();
  bool send_eof();
};


class multi_update :public select_result_interceptor
{
  TABLE_LIST *all_tables; /* query/update command tables */
  TABLE_LIST *leaves;     /* list of leves of join table tree */
  TABLE_LIST *update_tables, *table_being_updated;
  TABLE **tmp_tables, *main_table, *table_to_update;
  TMP_TABLE_PARAM *tmp_table_param;
  ha_rows updated, found;
  List <Item> *fields, *values;
  List <Item> **fields_for_table, **values_for_table;
  uint table_count;
  Copy_field *copy_field;
  enum enum_duplicates handle_duplicates;
  bool do_update, trans_safe;
  /* True if the update operation has made a change in a transactional table */
  bool transactional_tables;
  bool ignore;

public:
  multi_update(TABLE_LIST *ut, TABLE_LIST *leaves_list,
	       List<Item> *fields, List<Item> *values,
	       enum_duplicates handle_duplicates, bool ignore);
  ~multi_update();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_updates (bool from_send_error);
  bool send_eof();
};

class my_var : public Sql_alloc  {
public:
  LEX_STRING s;
  bool local;
  uint offset;
  enum_field_types type;
  my_var (LEX_STRING& j, bool i, uint o, enum_field_types t)
    :s(j), local(i), offset(o), type(t)
  {}
  ~my_var() {}
};

class select_dumpvar :public select_result_interceptor {
  ha_rows row_count;
public:
  List<my_var> var_list;
  List<Item_func_set_user_var> vars;
  List<Item_splocal> local_vars;
  select_dumpvar(void)  { var_list.empty(); local_vars.empty(); vars.empty(); row_count=0;}
  ~select_dumpvar() {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  void cleanup();
};

/* Functions in sql_class.cc */

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var);
