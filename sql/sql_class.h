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

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

// TODO: create log.h and move all the log header stuff there

class Query_log_event;
class Load_log_event;
class Slave_log_event;

enum enum_enable_or_disable { LEAVE_AS_IS, ENABLE, DISABLE };
enum enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY, RNEXT_SAME };
enum enum_duplicates { DUP_ERROR, DUP_REPLACE, DUP_IGNORE };
enum enum_log_type { LOG_CLOSED, LOG_TO_BE_OPENED, LOG_NORMAL, LOG_NEW, LOG_BIN};
enum enum_delay_key_write { DELAY_KEY_WRITE_NONE, DELAY_KEY_WRITE_ON,
			    DELAY_KEY_WRITE_ALL };

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
  /*
    For binlog - if log name can never change we should not try to rotate it
    or write any rotation events. The user should use FLUSH MASTER instead
    of FLUSH LOGS for purging.
  */
  volatile enum_log_type log_type;
  enum cache_type io_cache_type;
  bool write_error, inited;
  bool need_start_event;
  bool no_auto_events; // for relay binlog
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
  void signal_update() { pthread_cond_broadcast(&update_cond);}
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
  int purge_logs(THD* thd, const char* to_log);
  int purge_first_log(struct st_relay_log_info* rli); 
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
  inline pthread_mutex_t* get_log_lock() { return &LOCK_log; }
  inline IO_CACHE* get_log_file() { return &log_file; }

  inline void lock_index() { pthread_mutex_lock(&LOCK_index);}
  inline void unlock_index() { pthread_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32 get_open_count() { return open_count; }
};

/* character conversion tables */

class CONVERT;
CONVERT *get_convert_set(const char *name_ptr);

class CONVERT
{
  const uchar *from_map,*to_map;
  void convert_array(const uchar *mapping,uchar *buff,uint length);
public:
  const char *name;
  uint numb;
  CONVERT(const char *name_par,uchar *from_par,uchar *to_par, uint number)
    :from_map(from_par),to_map(to_par),name(name_par),numb(number) {}
  friend CONVERT *get_convert_set(const char *name_ptr);
  inline void convert(char *a,uint length)
  {
    convert_array(from_map, (uchar*) a,length);
  }
  bool store(String *, const char *,uint);
  inline uint number() { return numb; }
};

typedef struct st_copy_info {
  ha_rows records;
  ha_rows deleted;
  ha_rows copied;
  ha_rows error_count;
  enum enum_duplicates handle_duplicates;
  int escape_char, last_errno;
} COPY_INFO;


class key_part_spec :public Sql_alloc {
public:
  const char *field_name;
  uint length;
  key_part_spec(const char *name,uint len=0) :field_name(name), length(len) {}
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
  enum Keytype { PRIMARY, UNIQUE, MULTIPLE, FULLTEXT };
  enum Keytype type;
  enum ha_key_alg algorithm;
  List<key_part_spec> columns;
  const char *Name;

  Key(enum Keytype type_par,const char *name_arg,List<key_part_spec> &cols)
    :type(type_par), algorithm(HA_KEY_ALG_UNDEF), columns(cols), Name(name_arg)
  {}
  ~Key() {}
  const char *name() { return Name; }
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

// needed to be able to have an I_List of char* strings.in mysqld.cc where we cannot use String
// because it is Sql_alloc'ed
class i_string: public ilink
{
public:
  char* ptr;
  i_string():ptr(0) { }
  i_string(char* s) : ptr(s) {}
};

//needed for linked list of two strings for replicate-rewrite-db
class i_string_pair: public ilink
{
public:
  char* key;
  char* val;
  i_string_pair():key(0),val(0) { }
  i_string_pair(char* key_arg, char* val_arg) : key(key_arg),val(val_arg) {}
};


class delayed_insert;

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
  ulong max_heap_table_size;
  ulong max_sort_length;
  ulong max_tmp_tables;
  ulong max_insert_delayed_threads;
  ulong myisam_repair_threads;
  ulong myisam_sort_buff_size;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong net_retry_count;
  ulong query_cache_type;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong sortbuff_size;
  ulong tmp_table_size;
  ulong tx_isolation;
  ulong table_type;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong log_warnings;

  my_bool low_priority_updates;
  my_bool new_mode;
  my_bool query_cache_wlock_invalidate;

  CONVERT *convert_set;
};


/*
  For each client connection we create a separate thread with THD serving as
  a thread/connection descriptor
*/

class THD :public ilink
{
public:
  NET	  net;				// client connection descriptor
  LEX	  lex;				// parse tree descriptor
  MEM_ROOT mem_root;			// 1 command-life memory pool
  HASH    user_vars;			// hash for user variables
  String  packet;			// dynamic buffer for network I/O
  struct  sockaddr_in remote;		// client socket address
  struct  rand_struct rand;		// used for authentication
  struct  system_variables variables;	// Changeable local variables
  pthread_mutex_t LOCK_delete;		// Locked before thd is deleted
  /* 
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
  char	  *query;			// Points to the current query,
  /*
    A pointer to the stack frame of handle_one_connection(),
    which is called first in the thread for handling a client
  */
  char	  *thread_stack;

  /*
    host - host of the client
    user - user of the client, set to NULL until the user has been read from
     the connection
    priv_user - not sure why we have it, but it is set to "boot" when we run
     with --bootstrap
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

  uint client_capabilities;		/* What the client supports */
  /* Determines if which non-standard SQL behaviour should be enabled */
  uint sql_mode;
  ulong max_client_packet_length;
  ulong master_access;			/* Global privileges from mysql.user */
  ulong db_access;			/* Privileges for current db */

  /*
    open_tables - list of regular tables in use by this thread
    temporary_tables - list of temp tables in use by this thread
    handler_tables - list of tables that were opened with HANDLER OPEN
     and are still in use by this thread
  */
  TABLE   *open_tables,*temporary_tables, *handler_tables;
  // TODO: document the variables below
  MYSQL_LOCK *lock,*locked_tables;
  ULL	  *ull;
#ifndef DBUG_OFF
  uint dbug_sentry; // watch out for memory corruption
#endif  
  struct st_my_thread_var *mysys_var;
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
  struct st_transactions {
    IO_CACHE trans_log;
    THD_TRANS all;			// Trans since BEGIN WORK
    THD_TRANS stmt;			// Trans for current statement
    uint bdb_lock_count;

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
  Item	     *free_list, *handler_items;
  Field      *dupp_field;
#ifndef __WIN__
  sigset_t signals,block_signals;
#endif
#ifdef SIGNAL_WITH_VIO_CLOSE
  Vio* active_vio;
#endif  
  ulonglong  next_insert_id,last_insert_id,current_insert_id,
             limit_found_rows;
  ha_rows    select_limit, offset_limit, cuted_fields,
             sent_row_count, examined_row_count;
  table_map  used_tables;
  USER_CONN *user_connect;
  ulong	     query_id,version, options,thread_id, col_access;
  ulong	     rand_saved_seed1, rand_saved_seed2;
  long	     dbug_thread_id;
  pthread_t  real_id;
  uint	     current_tablenr,tmp_table,cond_count,global_read_lock;
  uint	     server_status,open_options,system_thread;
  uint32     query_length;
  uint32     db_length;
  /* variables.transaction_isolation is reset to this after each commit */
  enum_tx_isolation session_tx_isolation;
  char	     scramble[9];
  bool       slave_thread;
  bool	     set_query_id,locked,count_cuted_fields,some_tables_deleted;
  bool	     no_errors, allow_sum_func, password, fatal_error;
  bool	     query_start_used,last_insert_id_used,insert_id_used,rand_used;
  bool	     in_lock_tables;
  bool       query_error, bootstrap, cleanup_done;
  bool	     safe_to_cache_query;
  bool	     volatile killed;
  /*
    If we do a purge of binary logs, log index info of the threads
    that are currently reading it needs to be adjusted. To do that
    each thread that is using LOG_INFO needs to adjust the pointer to it
  */
  LOG_INFO*  current_linfo;
  /*
    In slave thread we need to know in behalf of which
    thread the query is being run to replicate temp tables properly
  */
  ulong	     slave_proxy_id;
  NET*       slave_net;			// network connection from slave -> m.
   
  /* Used by the sys_var class to store temporary values */
  union
  {
    my_bool my_bool_value;
    long    long_value;
  } sys_var_tmp;

  THD();
  ~THD();
  void init(void);
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
  inline void	insert_id(ulonglong id)
  { last_insert_id=id; insert_id_used=1; }
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
	    transaction.all.innodb_active_trans != 0);
#else
    return 0;
#endif
  }
  inline gptr alloc(unsigned int size) { return alloc_root(&mem_root,size); }
  inline gptr calloc(unsigned int size)
  {
    gptr ptr;
    if ((ptr=alloc_root(&mem_root,size)))
      bzero((char*) ptr,size);
    return ptr;
  }
  inline char *strdup(const char *str)
  { return strdup_root(&mem_root,str); }
  inline char *strmake(const char *str, uint size)
  { return strmake_root(&mem_root,str,size); }
  inline char *memdup(const char *str, uint size)
  { return memdup_root(&mem_root,str,size); }
  inline char *memdup_w_gap(const char *str, uint size, uint gap)
  {
    gptr ptr;
    if ((ptr=alloc_root(&mem_root,size+gap)))
      memcpy(ptr,str,size);
    return ptr;
  }
  inline gptr trans_alloc(unsigned int size) 
  { 
    return alloc_root(&transaction.mem_root,size);
  }
  void add_changed_table(TABLE *table);
  void add_changed_table(const char *key, long key_length);
  CHANGED_TABLE_LIST * changed_table_dup(const char *key, long key_length);
#ifndef EMBEDDED_LIBRARY
  inline void clear_error()
  {
    net.last_error[0]= 0;
    net.last_errno= 0;
  }
#else
  void clear_error();
#endif
};

/* Flags for the THD::system_thread (bitmap) variable */
#define SYSTEM_THREAD_DELAYED_INSERT 1
#define SYSTEM_THREAD_SLAVE_IO 2
#define SYSTEM_THREAD_SLAVE_SQL 4

/*
  Used to hold information about file and file structure in exchainge 
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
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
  ~sql_exchange() {}
};

#include "log_event.h"

/*
** This is used to get result from a select
*/

class JOIN;

void send_error(NET *net,uint sql_errno=0, const char *err=0);

class select_result :public Sql_alloc {
protected:
  THD *thd;
public:
  select_result();
  virtual ~select_result() {};
  virtual int prepare(List<Item> &list) { return 0; }
  virtual bool send_fields(List<Item> &list,uint flag)=0;
  virtual bool send_data(List<Item> &items)=0;
  virtual bool initialize_tables (JOIN *join=0) { return 0; }
  virtual void send_error(uint errcode,const char *err)
  {
    ::send_error(&thd->net,errcode,err);
  }
  virtual bool send_eof()=0;
  virtual void abort() {}
};


class select_send :public select_result {
public:
  select_send() {}
  bool send_fields(List<Item> &list,uint flag);
  bool send_data(List<Item> &items);
  bool send_eof();
};


class select_export :public select_result {
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  char path[FN_REFLEN];
  uint field_term_length;
  int field_sep_char,escape_char,line_sep_char;
  bool fixed_row_size;
public:
  select_export(sql_exchange *ex) :exchange(ex),file(-1),row_count(0L)
  { path[0]=0; }
  ~select_export();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list,
		   uint flag) { return 0; }
  bool send_data(List<Item> &items);
  void send_error(uint errcode,const char *err);
  bool send_eof();
};


class select_dump :public select_result {
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  char path[FN_REFLEN];
public:
  select_dump(sql_exchange *ex) :exchange(ex),file(-1),row_count(0L)
  { path[0]=0; }
  ~select_dump();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list,
		   uint flag) { return 0; }
  bool send_data(List<Item> &items);
  void send_error(uint errcode,const char *err);
  bool send_eof();
};


class select_insert :public select_result {
 public:
  TABLE *table;
  List<Item> *fields;
  ulonglong last_insert_id;
  COPY_INFO info;

  select_insert(TABLE *table_par,List<Item> *fields_par,enum_duplicates duplic)
    :table(table_par),fields(fields_par), last_insert_id(0)
  {
    bzero((char*) &info,sizeof(info));
    info.handle_duplicates=duplic;
  }
  ~select_insert();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list, uint flag)
  { return 0; }
  bool send_data(List<Item> &items);
  void send_error(uint errcode,const char *err);
  bool send_eof();
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
		List<Item> &select_fields,enum_duplicates duplic)
    :select_insert (NULL, &select_fields, duplic), db(db_name),
    name(table_name), extra_fields(&fields_par),keys(&keys_par),
    create_info(create_info_par), lock(0)
    {}
  int prepare(List<Item> &list);
  bool send_data(List<Item> &values);
  bool send_eof();
  void abort();
};


class select_union :public select_result {
 public:
  TABLE *table;
  COPY_INFO info;
  TMP_TABLE_PARAM *tmp_table_param;
  bool not_describe;

  select_union(TABLE *table_par);
  ~select_union();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list, uint flag)
  { return 0; }
  bool send_data(List<Item> &items);
  bool send_eof();
  bool flush();
};

/* Structs used when sorting */

typedef struct st_sort_field {
  Field *field;				/* Field to sort */
  Item	*item;				/* Item if not sorting fields */
  uint	 length;			/* Length of sort field */
  my_bool reverse;			/* if descending sort */
  Item_result result_type;		/* Type of item */
} SORT_FIELD;


typedef struct st_sort_buffer {
  uint index;					/* 0 or 1 */
  uint sort_orders;
  uint change_pos;				/* If sort-fields changed */
  char **buff;
  SORT_FIELD *sortorder;
} SORT_BUFFER;

/* Structure for db & table in sql_yacc */

class Table_ident :public Sql_alloc {
 public:
  LEX_STRING db;
  LEX_STRING table;
  inline Table_ident(LEX_STRING db_arg,LEX_STRING table_arg,bool force)
    :table(table_arg)
  {
    if (!force && (current_thd->client_capabilities & CLIENT_NO_SCHEMA))
      db.str=0;
    else
      db= db_arg;
  }
  inline Table_ident(LEX_STRING table_arg) :table(table_arg) {db.str=0;}
  inline void change_db(char *db_name)
  { db.str= db_name; db.length=(uint) strlen(db_name); }
};

// this is needed for user_vars hash
class user_var_entry
{
 public:
  LEX_STRING name;
  char *value;
  ulong length, update_query_id;
  Item_result type;

  double val(my_bool *null_value);
  longlong val_int(my_bool *null_value);
  String *val_str(my_bool *null_value, String *str, uint decimals);
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
    return !tree_insert(&tree,ptr,0);
  }

  bool get(TABLE *table);

  friend int unique_write_to_file(gptr key, element_count count, Unique *unique);
  friend int unique_write_to_ptrs(gptr key, element_count count, Unique *unique);
};

class multi_delete : public select_result
{
  TABLE_LIST *delete_tables, *table_being_deleted;
  Unique  **tempfiles;
  THD *thd;
  ha_rows deleted, found;
  uint num_of_tables;
  int error;
  bool do_delete, transactional_tables, log_delayed, normal_tables;

public:
  multi_delete(THD *thd, TABLE_LIST *dt, uint num_of_tables);
  ~multi_delete();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list,
 		   uint flag) { return 0; }
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_deletes (bool from_send_error);
  bool send_eof();
};

class multi_update : public select_result
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
  bool do_update, trans_safe, transactional_tables, log_delayed;

public:
  multi_update(THD *thd_arg, TABLE_LIST *ut, List<Item> *fields,
	       List<Item> *values, enum_duplicates handle_duplicates);
  ~multi_update();
  int prepare(List<Item> &list);
  bool send_fields(List<Item> &list, uint flag) { return 0; }
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_updates (bool from_send_error);
  bool send_eof();
};

