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

enum enum_enable_or_disable { LEAVE_AS_IS, ENABLE, DISABLE };
enum enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY, RNEXT_SAME };
enum enum_duplicates { DUP_ERROR, DUP_REPLACE, DUP_UPDATE };
enum enum_log_type { LOG_CLOSED, LOG_TO_BE_OPENED, LOG_NORMAL, LOG_NEW, LOG_BIN};
enum enum_delay_key_write { DELAY_KEY_WRITE_NONE, DELAY_KEY_WRITE_ON,
			    DELAY_KEY_WRITE_ALL };

enum enum_check_fields { CHECK_FIELD_IGNORE, CHECK_FIELD_WARN,
			 CHECK_FIELD_ERROR_FOR_NULL };

extern char internal_table_name[2];

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

class Log_event;

class MYSQL_LOG
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
  bool no_auto_events;				// For relay binlog
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
  friend class Log_event;

public:
  MYSQL_LOG();
  ~MYSQL_LOG();
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
  bool open(const char *log_name,enum_log_type log_type,
	    const char *new_name, const char *index_file_name_arg,
	    enum cache_type io_cache_type_arg,
	    bool no_auto_events_arg, ulong max_size);
  void new_file(bool need_lock= 1);
  bool write(THD *thd, enum enum_server_command command,
	     const char *format,...);
  bool write(THD *thd, const char *query, uint query_length,
	     time_t query_start=0);
  bool write(Log_event* event_info); // binary log write
  bool write(THD *thd, IO_CACHE *cache, bool commit_or_rollback);

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
  int purge_logs(const char *to_log, bool included, 
                 bool need_mutex, bool need_update_threads,
                 ulonglong *decrease_log_space);
  int purge_logs_before_date(time_t purge_time);
  int purge_first_log(struct st_relay_log_info* rli, bool included); 
  bool reset_logs(THD* thd);
  void close(uint exiting);
  bool cut_spurious_tail();
  void report_pos_in_innodb();

  // iterating through the log index file
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
		   bool need_mutex);
  int find_next_log(LOG_INFO* linfo, bool need_mutex);
  int get_current_log(LOG_INFO* linfo);
  int raw_get_current_log(LOG_INFO* linfo);
  uint next_file_id();
  inline bool is_open() { return log_type != LOG_CLOSED; }
  inline char* get_index_fname() { return index_file_name;}
  inline char* get_log_fname() { return log_file_name; }
  inline pthread_mutex_t* get_log_lock() { return &LOCK_log; }
  inline IO_CACHE* get_log_file() { return &log_file; }

  inline void lock_index() { pthread_mutex_lock(&LOCK_index);}
  inline void unlock_index() { pthread_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32 get_open_count() { return open_count; }
};

/* character conversion tables */


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
  char* ptr;
  i_string():ptr(0) { }
  i_string(char* s) : ptr(s) {}
};

/* needed for linked list of two strings for replicate-rewrite-db */
class i_string_pair: public ilink
{
public:
  char* key;
  char* val;
  i_string_pair():key(0),val(0) { }
  i_string_pair(char* key_arg, char* val_arg) : key(key_arg),val(val_arg) {}
};


class MYSQL_ERROR: public Sql_alloc
{
public:
  enum enum_warning_level
  { WARN_LEVEL_NOTE, WARN_LEVEL_WARN, WARN_LEVEL_ERROR, WARN_LEVEL_END};

  uint code;
  enum_warning_level level;
  char *msg;
  
  MYSQL_ERROR(THD *thd, uint code_arg, enum_warning_level level_arg,
	      const char *msg_arg)
    :code(code_arg), level(level_arg)
  {
    if (msg_arg)
      set_msg(thd, msg_arg);
  }
  void set_msg(THD *thd, const char *msg_arg);
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
  ulong myisam_repair_threads;
  ulong myisam_sort_buff_size;
  ulong myisam_stats_method;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong preload_buff_size;
  ulong query_cache_type;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong sortbuff_size;
  ulong table_type;
  ulong tmp_table_size;
  ulong tx_isolation;
  /* Determines which non-standard SQL behaviour should be enabled */
  ulong sql_mode;
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
#ifdef HAVE_REPLICATION
  ulong sync_replication;
  ulong sync_replication_slave_id;
  ulong sync_replication_timeout;
#endif /* HAVE_REPLICATION */
#ifdef HAVE_INNOBASE_DB
  my_bool innodb_table_locks;
#endif /* HAVE_INNOBASE_DB */
#ifdef HAVE_NDBCLUSTER_DB
  ulong ndb_autoincrement_prefetch_sz;
  my_bool ndb_force_send;
  my_bool ndb_use_exact_count;
  my_bool ndb_use_transactions;
#endif /* HAVE_NDBCLUSTER_DB */
  my_bool old_passwords;
  
  /* Only charset part of these variables is sensible */
  CHARSET_INFO 	*character_set_client;
  CHARSET_INFO  *character_set_results;
  
  /* Both charset and collation parts of these variables are important */
  CHARSET_INFO	*collation_server;
  CHARSET_INFO	*collation_database;
  CHARSET_INFO  *collation_connection;

  /* Locale Support */
  MY_LOCALE *lc_time_names;

  Time_zone *time_zone;

  /* DATE, DATETIME and TIME formats */
  DATE_TIME_FORMAT *date_format;
  DATE_TIME_FORMAT *datetime_format;
  DATE_TIME_FORMAT *time_format;
};

void free_tmp_table(THD *thd, TABLE *entry);


class Item_arena
{
public:
  /*
    List of items created in the parser for this query. Every item puts
    itself to the list on creation (see Item::Item() for details))
  */
  Item *free_list;
  MEM_ROOT main_mem_root;
  MEM_ROOT *mem_root;                   // Pointer to current memroot
  enum enum_state 
  {
    INITIALIZED= 0, PREPARED= 1, EXECUTED= 3, CONVENTIONAL_EXECUTION= 2, 
    ERROR= -1
  };
  
  enum_state state;

  /* We build without RTTI, so dynamic_cast can't be used. */
  enum Type
  {
    STATEMENT, PREPARED_STATEMENT, STORED_PROCEDURE
  };

  /*
    This constructor is used only when Item_arena is created as
    backup storage for another instance of Item_arena.
  */
  Item_arena() {};
  /*
    Create arena for already constructed THD using its variables as
    parameters for memory root initialization.
  */
  Item_arena(THD *thd);
  /*
    Create arena and optionally init memory root with minimal values.
    Particularly used if Item_arena is part of Statement.
  */
  Item_arena(bool init_mem_root);
  virtual Type type() const;
  virtual ~Item_arena() {};

  inline bool is_stmt_prepare() const { return (int)state < (int)PREPARED; }
  inline bool is_first_stmt_execute() const { return state == PREPARED; }
  inline bool is_stmt_execute() const
  { return state == PREPARED || state == EXECUTED; }
  inline bool is_conventional_execution() const
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

  void set_n_backup_item_arena(Item_arena *set, Item_arena *backup);
  void restore_backup_item_arena(Item_arena *set, Item_arena *backup);
  void set_item_arena(Item_arena *set);
};


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

class Statement: public Item_arena
{
  Statement(const Statement &rhs);              /* not implemented: */
  Statement &operator=(const Statement &rhs);   /* non-copyable */
public:
  /* FIXME: must be private */
  LEX     main_lex;

  /*
    Uniquely identifies each statement object in thread scope; change during
    statement lifetime. FIXME: must be const
  */
   ulong id;

  /*
    - if set_query_id=1, we set field->query_id for all fields. In that case 
    field list can not contain duplicates.
  */
  bool set_query_id;
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

public:

  /*
    This constructor is called when statement is a subobject of THD:
    some variables are initialized in THD::init due to locking problems
  */
  Statement();

  Statement(THD *thd);
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

  int insert(THD *thd, Statement *statement);

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
  void erase(Statement *statement);
  /* Erase all statements (calls Statement destructor) */
  void reset();
  ~Statement_map();
private:
  HASH st_hash;
  HASH names_hash;
  Statement *last_found_statement;
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
  For each client connection we create a separate thread with THD serving as
  a thread/connection descriptor
*/

class THD :public ilink, 
           public Statement
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
  pthread_mutex_t LOCK_delete;		// Locked before thd is deleted
  /* all prepared statements and cursors of this connection */
  Statement_map stmt_map; 
  /*
    keeps THD state while it is used for active statement
    Note: we perform special cleanup for it in THD destructor.
  */
  Statement stmt_backup;
  /*
    A pointer to the stack frame of handle_one_connection(),
    which is called first in the thread for handling a client
  */
  char	  *thread_stack;

  /*
    host - host of the client
    user - user of the client, set to NULL until the user has been read from
     the connection
    priv_user - The user privilege we are using. May be '' for anonymous user.
    db - currently selected database
    ip - client IP
   */
  char	  *host,*user,*priv_user,*db,*ip;
  char	  priv_host[MAX_HOSTNAME];
  /* remote (peer) port */
  uint16 peer_port;
  /*
    Points to info-string that we show in SHOW PROCESSLIST
    You are supposed to update thd->proc_info only if you have coded
    a time-consuming piece that MySQL can get stuck in for a long time.
  */
  const char *proc_info;
  /* points to host if host is available, otherwise points to ip */
  const char *host_or_ip;

  ulong client_capabilities;		/* What the client supports */
  ulong max_client_packet_length;
  ulong master_access;			/* Global privileges from mysql.user */
  ulong db_access;			/* Privileges for current db */

  /*
    open_tables - list of regular tables in use by this thread
    temporary_tables - list of temp tables in use by this thread
    handler_tables - list of tables that were opened with HANDLER OPEN
     and are still in use by this thread
  */
  TABLE   *open_tables,*temporary_tables, *handler_tables, *derived_tables;
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
  MYSQL_LOCK	*lock;				/* Current locks */
  MYSQL_LOCK	*locked_tables;			/* Tables locked with LOCK */
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
    Type of current query: COM_PREPARE, COM_QUERY, etc. Set from 
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
  my_bool    tablespace_op;	/* This is TRUE in DISCARD/IMPORT TABLESPACE */
  struct st_transactions {
    IO_CACHE trans_log;                 // Inited ONLY if binlog is open !
    THD_TRANS all;			// Trans since BEGIN WORK
    THD_TRANS stmt;			// Trans for current statement
    uint bdb_lock_count;
#ifdef HAVE_NDBCLUSTER_DB
    void* thd_ndb;
#endif
    bool on;
    /*
       Tables changed in transaction (that must be invalidated in query cache).
       List contain only transactional tables, that not invalidated in query
       cache (instead of full list of changed in transaction tables).
    */
    CHANGED_TABLE_LIST* changed_tables;
    MEM_ROOT mem_root; // Transaction-life memory allocation pool
    void cleanup()
    {
      changed_tables = 0;
      free_root(&mem_root,MYF(MY_KEEP_PREALLOC));
    }
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
    rollback_item_tree_changes(). For conventional execution it's always 0.
  */
  Item_change_list change_list;

  /*
    Current prepared Item_arena if there one, or 0
  */
  Item_arena *current_arena;
  /*
    next_insert_id is set on SET INSERT_ID= #. This is used as the next
    generated auto_increment value in handler.cc
  */
  ulonglong  next_insert_id;
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
  ha_rows    cuted_fields,
             sent_row_count, examined_row_count;
  table_map  used_tables;
  USER_CONN *user_connect;
  CHARSET_INFO *db_charset;
  List<TABLE> temporary_tables_should_be_free; // list of temporary tables
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
  ulong	     query_id;
  ulong	     warn_id, version, options, thread_id, col_access;

  /* Statement id is thread-wide. This counter is used to generate ids */
  ulong      statement_id_counter;
  ulong	     rand_saved_seed1, rand_saved_seed2;
  ulong      row_count;  // Row counter, mainly for errors and warnings
  long	     dbug_thread_id;
  pthread_t  real_id;
  uint	     current_tablenr,tmp_table,global_read_lock;
  uint	     server_status,open_options,system_thread;
  uint32     db_length;
  uint       select_number;             //number of select (used for EXPLAIN)
  /* variables.transaction_isolation is reset to this after each commit */
  enum_tx_isolation session_tx_isolation;
  enum_check_fields count_cuted_fields;
  /* for user variables replication*/
  DYNAMIC_ARRAY user_var_events;

  /* scramble - random string sent to client on handshake */
  char	     scramble[SCRAMBLE_LENGTH+1];

  bool       slave_thread, one_shot_set;
  bool	     locked, some_tables_deleted;
  bool       last_cuted_field;
  bool	     no_errors, password, is_fatal_error;
  bool	     query_start_used,last_insert_id_used,insert_id_used,rand_used;
  /* for IS NULL => = last_insert_id() fix in remove_eq_conds() */
  bool       substitute_null_with_insert_id;
  bool	     time_zone_used;
  bool	     in_lock_tables;
  bool       query_error, bootstrap, cleanup_done;
  bool	     tmp_table_used;
  bool	     charset_is_system_charset, charset_is_collation_connection;
  bool       enable_slow_log;   /* enable slow log for current statement */
  my_bool    volatile killed;

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
    ulong   ulong_value;
  } sys_var_tmp;

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
  void awake(bool prepare_to_die);
  /*
    For enter_cond() / exit_cond() to work the mutex must be got before
    enter_cond() (in 4.1 an assertion will soon ensure this); this mutex is
    then released by exit_cond(). Use must be:
    lock mutex; enter_cond(); your code; exit_cond().
  */
  inline const char* enter_cond(pthread_cond_t *cond, pthread_mutex_t* mutex,
			  const char* msg)
  {
    const char* old_msg = proc_info;
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
    substitute_null_with_insert_id= TRUE;
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
    return (transaction.all.bdb_tid != 0 ||
	    transaction.all.innodb_active_trans != 0 ||
	    transaction.all.ndb_tid != 0);
#else
    return 0;
#endif
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

  inline Item_arena *change_arena_if_needed(Item_arena *backup)
  {
    /*
      use new arena if we are in a prepared statements and we have not
      already changed to use this arena.
    */
    if (current_arena->is_stmt_prepare() &&
        mem_root != &current_arena->main_mem_root)
    {
      set_n_backup_item_arena(current_arena, backup);
      return current_arena;
    }
    return 0;
  }

  void change_item_tree(Item **place, Item *new_value)
  {
    /* TODO: check for OOM condition here */
    if (!current_arena->is_conventional_execution())
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
};

#define tmp_disable_binlog(A)       \
  ulong save_options= (A)->options; \
  (A)->options&= ~OPTION_BIN_LOG;

#define reenable_binlog(A)          (A)->options= save_options;

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

void send_error(THD *thd, uint sql_errno=0, const char *err=0);

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
  /*
    Because of peculiarities of prepared statements protocol
    we need to know number of columns in the result set (if
    there is a result set) apart from sending columns metadata.
  */
  virtual uint field_count(List<Item> &fields) const
  { return fields.elements; }
  virtual bool send_fields(List<Item> &list,uint flag)=0;
  virtual bool send_data(List<Item> &items)=0;
  virtual bool initialize_tables (JOIN *join=0) { return 0; }
  virtual void send_error(uint errcode,const char *err);
  virtual bool send_eof()=0;
  virtual void abort() {}
  /*
    Cleanup instance of this class for next execution of a prepared
    statement/stored procedure.
  */
  virtual void cleanup();
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
public:
  select_send() {}
  bool send_fields(List<Item> &list,uint flag);
  bool send_data(List<Item> &items);
  bool send_eof();
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
  TABLE *table;
  List<Item> *fields;
  ulonglong last_insert_id;
  COPY_INFO info;
  TABLE_LIST *insert_table_list;
  TABLE_LIST *dup_table_list;

  select_insert(TABLE *table_par, List<Item> *fields_par,
		enum_duplicates duplic, bool ignore)
    :table(table_par), fields(fields_par), last_insert_id(0),
     insert_table_list(0), dup_table_list(0)
  {
    bzero((char*) &info,sizeof(info));
    info.ignore= ignore;
    info.handle_duplicates=duplic;
  }
  select_insert(TABLE *table_par,
		TABLE_LIST *insert_table_list_par,
		TABLE_LIST *dup_table_list_par,
		List<Item> *fields_par,
		List<Item> *update_fields, List<Item> *update_values,
		enum_duplicates duplic, bool ignore)
    :table(table_par), fields(fields_par), last_insert_id(0),
     insert_table_list(insert_table_list_par),
     dup_table_list(dup_table_list_par)
  {
    bzero((char*) &info,sizeof(info));
    info.ignore= ignore;
    info.handle_duplicates= duplic;
    info.update_fields= update_fields;
    info.update_values= update_values;
  }
  ~select_insert();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  virtual void store_values(List<Item> &values);
  void send_error(uint errcode,const char *err);
  bool send_eof();
  /* not implemented: select_insert is never re-used in prepared statements */
  void cleanup();
};


class select_create: public select_insert {
  ORDER *group;
  const char *db;
  const char *name;
  List<create_field> *extra_fields;
  List<Key> *keys;
  HA_CREATE_INFO *create_info;
  MYSQL_LOCK *lock;
  Field **field;
public:
  select_create(const char *db_name, const char *table_name,
		HA_CREATE_INFO *create_info_par,
		List<create_field> &fields_par,
		List<Key> &keys_par,
		List<Item> &select_fields,enum_duplicates duplic, bool ignore)
    :select_insert (NULL, &select_fields, duplic, ignore), db(db_name),
    name(table_name), extra_fields(&fields_par),keys(&keys_par),
    create_info(create_info_par), lock(0)
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
  bool force_copy_fields;
  TMP_TABLE_PARAM()
    :copy_field(0), group_parts(0),
    group_length(0), group_null_parts(0), convert_blob_length(0),
    force_copy_fields(0)
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

class select_union :public select_result_interceptor {
 public:
  TABLE *table;
  COPY_INFO info;
  TMP_TABLE_PARAM tmp_table_param;

  select_union(TABLE *table_par);
  ~select_union();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  bool flush();
  void set_table(TABLE *tbl) { table= tbl; }
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
  ulong length, update_query_id, used_query_id;
  Item_result type;

  double val(my_bool *null_value);
  longlong val_int(my_bool *null_value);
  String *val_str(my_bool *null_value, String *str, uint decimals);
  DTCollation collation;
};


/* Class for unique (removing of duplicates) */

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
  Unique(qsort_cmp2 comp_func, void * comp_func_fixed_arg,
	 uint size_arg, ulong max_in_memory_size_arg);
  ~Unique();
  inline bool unique_add(gptr ptr)
  {
    if (tree.elements_in_tree > max_elements && flush())
      return 1;
    return !tree_insert(&tree, ptr, 0, tree.custom_arg);
  }

  bool get(TABLE *table);

  friend int unique_write_to_file(gptr key, element_count count, Unique *unique);
  friend int unique_write_to_ptrs(gptr key, element_count count, Unique *unique);
};


class multi_delete :public select_result_interceptor
{
  TABLE_LIST *delete_tables, *table_being_deleted;
  Unique **tempfiles;
  THD *thd;
  ha_rows deleted, found;
  uint num_of_tables;
  int error;
  bool do_delete, transactional_tables, log_delayed, normal_tables;
public:
  multi_delete(THD *thd, TABLE_LIST *dt, uint num_of_tables);
  ~multi_delete();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_deletes (bool from_send_error);
  bool send_eof();
};


class multi_update :public select_result_interceptor
{
  TABLE_LIST *all_tables, *update_tables, *table_being_updated;
  THD *thd;
  TABLE **tmp_tables, *main_table, *table_to_update;
  TMP_TABLE_PARAM *tmp_table_param;
  ha_rows updated, found;
  List <Item> *fields, *values;
  List <Item> **fields_for_table, **values_for_table;
  uint table_count;
  Copy_field *copy_field;
  enum enum_duplicates handle_duplicates;
  bool do_update, trans_safe, transactional_tables, log_delayed, ignore;

public:
  multi_update(THD *thd_arg, TABLE_LIST *ut, List<Item> *fields,
	       List<Item> *values, enum_duplicates handle_duplicates, bool ignore);
  ~multi_update();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_updates (bool from_send_error);
  bool send_eof();
};


class select_dumpvar :public select_result_interceptor {
  ha_rows row_count;
public:
  List<LEX_STRING> var_list;
  List<Item_func_set_user_var> vars;
  select_dumpvar(void)  { var_list.empty(); vars.empty(); row_count=0;}
  ~select_dumpvar() {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  void cleanup();
};
