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
enum enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY };
enum enum_duplicates { DUP_ERROR, DUP_REPLACE, DUP_IGNORE };
enum enum_log_type { LOG_CLOSED, LOG_NORMAL, LOG_NEW, LOG_BIN };

// log info errors 
#define LOG_INFO_EOF -1
#define LOG_INFO_IO  -2
#define LOG_INFO_INVALID -3
#define LOG_INFO_SEEK -4
#define LOG_INFO_PURGE_NO_ROTATE -5
#define LOG_INFO_MEM -6
#define LOG_INFO_FATAL -7
#define LOG_INFO_IN_USE -8

struct st_relay_log_info;

typedef struct st_log_info
{
  char log_file_name[FN_REFLEN];
  my_off_t index_file_offset;
  my_off_t pos;
  bool fatal; // if the purge happens to give us a negative offset
  pthread_mutex_t lock;
  st_log_info():fatal(0) { pthread_mutex_init(&lock, MY_MUTEX_INIT_FAST);}
  ~st_log_info() { pthread_mutex_destroy(&lock);}
} LOG_INFO;

class Log_event;

class MYSQL_LOG {
 private:
  pthread_mutex_t LOCK_log, LOCK_index;
  time_t last_time,query_start;
  IO_CACHE log_file;
  File index_file;
  char *name;
  volatile enum_log_type log_type;
  char time_buff[20],db[NAME_LEN+1];
  char log_file_name[FN_REFLEN],index_file_name[FN_REFLEN];
  bool write_error,inited;
  uint file_id; // current file sequence number for load data infile
  // binary logging
  bool no_rotate; // for binlog - if log name can never change
  // we should not try to rotate it or write any rotation events
  // the user should use FLUSH MASTER instead of FLUSH LOGS for
  // purging
  enum cache_type io_cache_type;
  bool need_start_event;
  pthread_cond_t update_cond;
  bool no_auto_events; // for relay binlog
  friend class Log_event;

public:
  MYSQL_LOG();
  ~MYSQL_LOG();
  pthread_mutex_t* get_log_lock() { return &LOCK_log; }
  IO_CACHE* get_log_file() { return &log_file; }
  void signal_update() { pthread_cond_broadcast(&update_cond);}
  void wait_for_update(THD* thd);
  void set_need_start_event() { need_start_event = 1; }
  void set_index_file_name(const char* index_file_name = 0);
  void init(enum_log_type log_type_arg,
	    enum cache_type io_cache_type_arg = WRITE_CACHE,
	    bool no_auto_events_arg = 0);
  void open(const char *log_name,enum_log_type log_type,
	    const char *new_name, enum cache_type io_cache_type_arg,
	    bool no_auto_events_arg);
  void new_file(bool inside_mutex = 0);
  bool open_index(int options);
  void close_index();
  bool write(THD *thd, enum enum_server_command command,
	     const char *format,...);
  bool write(THD *thd, const char *query, uint query_length,
	     time_t query_start=0);
  bool write(Log_event* event_info); // binary log write
  bool write(IO_CACHE *cache);

  //v stands for vector
  //invoked as appendv(buf1,len1,buf2,len2,...,bufn,lenn,0)
  bool appendv(const char* buf,uint len,...);
  bool append(Log_event* ev);
  
  int generate_new_name(char *new_name,const char *old_name);
  void make_log_name(char* buf, const char* log_ident);
  bool is_active(const char* log_file_name);
  int purge_logs(THD* thd, const char* to_log);
  int purge_first_log(struct st_relay_log_info* rli); 
  int reset_logs(THD* thd);
  void close(bool exiting = 0); // if we are exiting, we also want to close the
  // index file

  // iterating through the log index file
  int find_first_log(LOG_INFO* linfo, const char* log_name,
		     bool need_mutex=1);
  int find_next_log(LOG_INFO* linfo, bool need_mutex=1);
  int get_current_log(LOG_INFO* linfo);
  uint next_file_id();

  inline bool is_open() { return log_type != LOG_CLOSED; }
  char* get_index_fname() { return index_file_name;}
  char* get_log_fname() { return log_file_name; }
  void lock_index() { pthread_mutex_lock(&LOCK_index);}
  void unlock_index() { pthread_mutex_unlock(&LOCK_index);}
  File get_index_file() { return index_file;}
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
  ha_rows error;
  enum enum_duplicates handle_duplicates;
  int escape_char;
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
  List<key_part_spec> columns;
  const char *Name;

  Key(enum Keytype type_par,const char *name_arg,List<key_part_spec> &cols)
    :type(type_par), columns(cols),Name(name_arg) {}
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

/* For each client connection we create a separate thread with THD serving as
   a thread/connection descriptor */

class THD :public ilink {
public:
  NET	  net; // client connection descriptor
  LEX	  lex; // parse tree descriptor
  MEM_ROOT mem_root; // memory allocation pool
  HASH     user_vars; // hash for user variables
  String  packet; // dynamic string buffer used for network I/O		
  struct  sockaddr_in remote; // client socket address
  struct  rand_struct rand; // used for authentication
  
  /* query points to the current query,
     thread_stack is a pointer to the stack frame of handle_one_connection(),
     which is called first in the thread for handling a client 
   */
  char	  *query,*thread_stack;
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
  /* proc_info points to a string that will show in the Info column of
     SHOW PROCESSLIST output
     host_or_ip points to host if host is available, otherwise points to ip   
   */
  const   char *proc_info, *host_or_ip;
  
  /*
    client_capabilities has flags describing what the client can do
    sql_mode determines if certain non-standard SQL behaviour should be
     enabled
    max_packet_length - supposed to be maximum packet length the client
     can handle, but it currently appears to be assigned but never used
     except for one debugging statement
   */
  uint	  client_capabilities,sql_mode,max_packet_length;

  /*
    master_access - privillege descriptor mask for system threads
    db_access - privillege descriptor mask for regular threads
  */
  uint	  master_access,db_access;
  
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
  struct st_my_thread_var *mysys_var;
  enum enum_server_command command;
  uint32 server_id;
  uint32     file_id;			// for LOAD DATA INFILE
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
  } transaction;
  Item	     *free_list, *handler_items;
  CONVERT    *convert_set;
  Field      *dupp_field;
#ifndef __WIN__
  sigset_t signals,block_signals;
#endif
#ifdef SIGNAL_WITH_VIO_CLOSE
  Vio* active_vio;
  pthread_mutex_t active_vio_lock;
#endif  
  ulonglong  next_insert_id,last_insert_id,current_insert_id,
             limit_found_rows;
  ha_rows    select_limit,offset_limit,default_select_limit,cuted_fields,
             max_join_size, sent_row_count, examined_row_count;
  table_map  used_tables;
  ulong	     query_id,version, inactive_timeout,options,thread_id;
  long	     dbug_thread_id;
  pthread_t  real_id;
  uint	     current_tablenr,tmp_table,cond_count,col_access;
  uint	     server_status,open_options;
  uint32     query_length;
  uint32     db_length;
  enum_tx_isolation tx_isolation, session_tx_isolation;
  char	     scramble[9];
  uint8	     query_cache_type;		// type of query cache processing
  bool       slave_thread;
  bool	     set_query_id,locked,count_cuted_fields,some_tables_deleted;
  bool	     no_errors, allow_sum_func, password, fatal_error;
  bool	     query_start_used,last_insert_id_used,insert_id_used;
  bool	     system_thread,in_lock_tables,global_read_lock;
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
  uint32 log_pos;

  THD();
  ~THD();
  void cleanup(void);
  bool store_globals();
#ifdef SIGNAL_WITH_VIO_CLOSE
  inline void set_active_vio(Vio* vio)
  {
    pthread_mutex_lock(&active_vio_lock);
    active_vio = vio;
    pthread_mutex_unlock(&active_vio_lock);
  }
  inline void clear_active_vio()
  {
    pthread_mutex_lock(&active_vio_lock);
    active_vio = 0;
    pthread_mutex_unlock(&active_vio_lock);
  }
  inline void close_active_vio()
  {
    pthread_mutex_lock(&active_vio_lock);
    if(active_vio)
      {
	vio_close(active_vio);
        active_vio = 0;
      }
    pthread_mutex_unlock(&active_vio_lock);
  }
#endif  
  void awake(bool prepare_to_die);
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
};


class sql_exchange :public Sql_alloc
{
public:
  char *file_name;
  String *field_term,*enclosed,*line_term,*line_start,*escaped;
  bool opt_enclosed;
  bool dumpfile;
  uint skip_lines;
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
  virtual void initialize_tables (JOIN *join=0) {}
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
  uint field_term_length;
  int field_sep_char,escape_char,line_sep_char;
  bool fixed_row_size;
public:
  select_export(sql_exchange *ex) :exchange(ex),file(-1),row_count(0L) {}
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
  uint save_time_stamp;

  select_insert(TABLE *table_par,List<Item> *fields_par,enum_duplicates duplic)
    :table(table_par),fields(fields_par), last_insert_id(0), save_time_stamp(0)  {
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
  select_create (const char *db_name, const char *table_name,
		 HA_CREATE_INFO *create_info_par,
		 List<create_field> &fields_par,
		 List<Key> &keys_par,
		 List<Item> &select_fields,enum_duplicates duplic)
    :select_insert (NULL, &select_fields, duplic), db(db_name),
    name(table_name), extra_fields(&fields_par),keys(&keys_par),
    create_info(create_info_par),
    lock(0)
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
  uint save_time_stamp;

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

public:
  ulong elements;
  Unique(qsort_cmp2 comp_func, void * comp_func_fixed_arg,
	 uint size, ulong max_in_memory_size_arg);
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

 class multi_delete : public select_result {
   TABLE_LIST *delete_tables, *table_being_deleted;
#ifdef SINISAS_STRIP
   IO_CACHE **tempfiles;
   byte *memory_lane;
#else
   Unique  **tempfiles;
#endif
   THD *thd;
   ha_rows deleted;
   uint num_of_tables;
   int error;
   thr_lock_type lock_option;
   bool do_delete, not_trans_safe;
 public:
   multi_delete(THD *thd, TABLE_LIST *dt, thr_lock_type lock_option_arg,
		uint num_of_tables);
   ~multi_delete();
   int prepare(List<Item> &list);
   bool send_fields(List<Item> &list,
 		   uint flag) { return 0; }
   bool send_data(List<Item> &items);
   void initialize_tables (JOIN *join);
   void send_error(uint errcode,const char *err);
   int  do_deletes (bool from_send_error);
   bool send_eof();
 };

 class multi_update : public select_result {
   TABLE_LIST *update_tables, *table_being_updated;
//   Unique  **tempfiles;
   COPY_INFO *infos;
   TABLE **tmp_tables;
   THD *thd;
   ha_rows updated, found;
   List<Item> fields;
   List <Item> **fields_by_tables;
   thr_lock_type lock_option;
   enum enum_duplicates dupl;
   uint num_of_tables, num_fields, num_updated, *save_time_stamps, *field_sequence;
   int error;
   bool do_update, not_trans_safe;
 public:
   multi_update(THD *thd_arg, TABLE_LIST *ut, List<Item> &fs, 		 
		enum enum_duplicates handle_duplicates,  
		thr_lock_type lock_option_arg, uint num);
   ~multi_update();
   int prepare(List<Item> &list);
   bool send_fields(List<Item> &list,
 		   uint flag) { return 0; }
   bool send_data(List<Item> &items);
   void initialize_tables (JOIN *join);
   void send_error(uint errcode,const char *err);
   int  do_updates (bool from_send_error);
   bool send_eof();
 };

