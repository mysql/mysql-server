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


#ifndef _log_event_h
#define _log_event_h

#ifdef __EMX__
#undef write  // remove pthread.h macro definition, conflict with write() class member
#endif

#if defined(__GNUC__) && !defined(MYSQL_CLIENT)
#pragma interface			/* gcc class implementation */
#endif

#define LOG_READ_EOF    -1
#define LOG_READ_BOGUS  -2
#define LOG_READ_IO     -3
#define LOG_READ_MEM    -5
#define LOG_READ_TRUNC  -6
#define LOG_READ_TOO_LARGE -7

#define LOG_EVENT_OFFSET 4

#define BINLOG_VERSION    3

/*
 We could have used SERVER_VERSION_LENGTH, but this introduces an
 obscure dependency - if somebody decided to change SERVER_VERSION_LENGTH
 this would have broken the replication protocol
*/
#define ST_SERVER_VER_LEN 50

/*
  These are flags and structs to handle all the LOAD DATA INFILE options (LINES
  TERMINATED etc).
*/

#define DUMPFILE_FLAG		0x1
#define OPT_ENCLOSED_FLAG	0x2
#define REPLACE_FLAG		0x4
#define IGNORE_FLAG		0x8

#define FIELD_TERM_EMPTY	0x1
#define ENCLOSED_EMPTY		0x2
#define LINE_TERM_EMPTY		0x4
#define LINE_START_EMPTY	0x8
#define ESCAPED_EMPTY		0x10

/*****************************************************************************

  old_sql_ex struct

 ****************************************************************************/
struct old_sql_ex
{
  char field_term;
  char enclosed;
  char line_term;
  char line_start;
  char escaped;
  char opt_flags;
  char empty_flags;
};

#define NUM_LOAD_DELIM_STRS 5

/*****************************************************************************

  sql_ex_info struct

 ****************************************************************************/
struct sql_ex_info
{
  char* field_term;
  char* enclosed;
  char* line_term;
  char* line_start;
  char* escaped;
  int cached_new_format;
  uint8 field_term_len,enclosed_len,line_term_len,line_start_len, escaped_len;
  char opt_flags; 
  char empty_flags;
    
  // store in new format even if old is possible
  void force_new_format() { cached_new_format = 1;} 
  int data_size()
  {
    return (new_format() ?
	    field_term_len + enclosed_len + line_term_len +
	    line_start_len + escaped_len + 6 : 7);
  }
  int write_data(IO_CACHE* file);
  char* init(char* buf,char* buf_end,bool use_new_format);
  bool new_format()
  {
    return ((cached_new_format != -1) ? cached_new_format :
	    (cached_new_format=(field_term_len > 1 ||
				enclosed_len > 1 ||
				line_term_len > 1 || line_start_len > 1 ||
				escaped_len > 1)));
  }
};

/*****************************************************************************

  MySQL Binary Log

  This log consists of events.  Each event has a fixed-length header,
  possibly followed by a variable length data body.

  The data body consists of an optional fixed length segment (post-header)
  and  an optional variable length segment.

  See the #defines below for the format specifics.

  The events which really update data are Query_log_event and
  Load_log_event/Create_file_log_event/Execute_load_log_event (these 3 act
  together to replicate LOAD DATA INFILE, with the help of
  Append_block_log_event which prepares temporary files to load into the table).

 ****************************************************************************/

#define LOG_EVENT_HEADER_LEN 19     /* the fixed header length */
#define OLD_HEADER_LEN       13     /* the fixed header length in 3.23 */

/* event-specific post-header sizes */
#define QUERY_HEADER_LEN     (4 + 4 + 1 + 2)
#define LOAD_HEADER_LEN      (4 + 4 + 4 + 1 +1 + 4)
#define START_HEADER_LEN     (2 + ST_SERVER_VER_LEN + 4)
#define ROTATE_HEADER_LEN    8
#define CREATE_FILE_HEADER_LEN 4
#define APPEND_BLOCK_HEADER_LEN 4
#define EXEC_LOAD_HEADER_LEN   4
#define DELETE_FILE_HEADER_LEN 4

/* 
   Event header offsets; 
   these point to places inside the fixed header.
*/

#define EVENT_TYPE_OFFSET    4
#define SERVER_ID_OFFSET     5
#define EVENT_LEN_OFFSET     9
#define LOG_POS_OFFSET       13
#define FLAGS_OFFSET         17

/* start event post-header */

#define ST_BINLOG_VER_OFFSET  0
#define ST_SERVER_VER_OFFSET  2
#define ST_CREATED_OFFSET     (ST_SERVER_VER_OFFSET + ST_SERVER_VER_LEN)

/* slave event post-header (this event is never written) */

#define SL_MASTER_PORT_OFFSET   8
#define SL_MASTER_POS_OFFSET    0
#define SL_MASTER_HOST_OFFSET   10

/* query event post-header */

#define Q_THREAD_ID_OFFSET	0
#define Q_EXEC_TIME_OFFSET	4
#define Q_DB_LEN_OFFSET		8
#define Q_ERR_CODE_OFFSET	9
#define Q_DATA_OFFSET		QUERY_HEADER_LEN

/* Intvar event post-header */

#define I_TYPE_OFFSET        0
#define I_VAL_OFFSET         1

/* Rand event post-header */

#define RAND_SEED1_OFFSET 0
#define RAND_SEED2_OFFSET 8

/* User_var event post-header */

#define UV_VAL_LEN_SIZE        4
#define UV_VAL_IS_NULL         1
#define UV_VAL_TYPE_SIZE       1
#define UV_NAME_LEN_SIZE       4
#define UV_CHARSET_NUMBER_SIZE 4

/* Load event post-header */

#define L_THREAD_ID_OFFSET   0
#define L_EXEC_TIME_OFFSET   4
#define L_SKIP_LINES_OFFSET  8
#define L_TBL_LEN_OFFSET     12
#define L_DB_LEN_OFFSET      13
#define L_NUM_FIELDS_OFFSET  14
#define L_SQL_EX_OFFSET      18
#define L_DATA_OFFSET        LOAD_HEADER_LEN

/* Rotate event post-header */

#define R_POS_OFFSET       0
#define R_IDENT_OFFSET     8

/* CF to DF handle LOAD DATA INFILE */

/* CF = "Create File" */
#define CF_FILE_ID_OFFSET  0
#define CF_DATA_OFFSET     CREATE_FILE_HEADER_LEN

/* AB = "Append Block" */
#define AB_FILE_ID_OFFSET  0
#define AB_DATA_OFFSET     APPEND_BLOCK_HEADER_LEN

/* EL = "Execute Load" */
#define EL_FILE_ID_OFFSET  0

/* DF = "Delete File" */
#define DF_FILE_ID_OFFSET  0

#define QUERY_EVENT_OVERHEAD	(LOG_EVENT_HEADER_LEN+QUERY_HEADER_LEN)
#define QUERY_DATA_OFFSET	(LOG_EVENT_HEADER_LEN+QUERY_HEADER_LEN)
#define ROTATE_EVENT_OVERHEAD	(LOG_EVENT_HEADER_LEN+ROTATE_HEADER_LEN)
#define LOAD_EVENT_OVERHEAD	(LOG_EVENT_HEADER_LEN+LOAD_HEADER_LEN)
#define CREATE_FILE_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+\
 +LOAD_HEADER_LEN+CREATE_FILE_HEADER_LEN)
#define DELETE_FILE_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+DELETE_FILE_HEADER_LEN)
#define EXEC_LOAD_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+EXEC_LOAD_HEADER_LEN)
#define APPEND_BLOCK_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+APPEND_BLOCK_HEADER_LEN)

/* 4 bytes which all binlogs should begin with */
#define BINLOG_MAGIC        "\xfe\x62\x69\x6e"

/*
  The 2 flags below were useless :
  - the first one was never set
  - the second one was set in all Rotate events on the master, but not used for
  anything useful.
  So they are now removed and their place may later be reused for other
  flags. Then one must remember that Rotate events in 4.x have
  LOG_EVENT_FORCED_ROTATE_F set, so one should not rely on the value of the
  replacing flag when reading a Rotate event. 
  I keep the defines here just to remember what they were.
*/
#ifdef TO_BE_REMOVED
#define LOG_EVENT_TIME_F            0x1
#define LOG_EVENT_FORCED_ROTATE_F   0x2 
#endif
/* 
   If the query depends on the thread (for example: TEMPORARY TABLE).
   Currently this is used by mysqlbinlog to know it must print
   SET @@PSEUDO_THREAD_ID=xx; before the query (it would not hurt to print it
   for every query but this would be slow).
*/
#define LOG_EVENT_THREAD_SPECIFIC_F 0x4 

enum Log_event_type
{
  UNKNOWN_EVENT= 0, START_EVENT= 1, QUERY_EVENT= 2, STOP_EVENT= 3,
  ROTATE_EVENT= 4, INTVAR_EVENT= 5, LOAD_EVENT=6, SLAVE_EVENT= 7, 
  CREATE_FILE_EVENT= 8, APPEND_BLOCK_EVENT= 9, EXEC_LOAD_EVENT= 10,
  DELETE_FILE_EVENT= 11, NEW_LOAD_EVENT= 12, RAND_EVENT= 13,
  USER_VAR_EVENT= 14
};

enum Int_event_type
{
  INVALID_INT_EVENT = 0, LAST_INSERT_ID_EVENT = 1, INSERT_ID_EVENT = 2
};


#ifndef MYSQL_CLIENT
class String;
class MYSQL_LOG;
class THD;
#endif

struct st_relay_log_info;

/*****************************************************************************

  Log_event class

  This is the abstract base class for binary log events.

 ****************************************************************************/
class Log_event
{
public:
  /* 
     The offset in the log where this event originally appeared (it is preserved
     in relay logs, making SHOW SLAVE STATUS able to print coordinates of the
     event in the master's binlog). Note: when a transaction is written by the
     master to its binlog (wrapped in BEGIN/COMMIT) the log_pos of all the
     queries it contains is the one of the BEGIN (this way, when one does SHOW
     SLAVE STATUS it sees the offset of the BEGIN, which is logical as rollback
     may occur), except the COMMIT query which has its real offset.
  */
  my_off_t log_pos;
  /* 
     A temp buffer for read_log_event; it is later analysed according to the
     event's type, and its content is distributed in the event-specific fields.
  */
  char *temp_buf; 
  /*
    Timestamp on the master(for debugging and replication of NOW()/TIMESTAMP). 
    It is important for queries and LOAD DATA INFILE. This is set at the event's
    creation time, except for Query and Load (et al.) events where this is set
    at the query's execution time, which guarantees good replication (otherwise,
    we could have a query and its event with different timestamps).
  */
  time_t when;
  /* The number of seconds the query took to run on the master. */
  ulong exec_time;
  /* 
     The master's server id (is preserved in the relay log; used to prevent from
     infinite loops in circular replication). 
  */
  uint32 server_id;
  uint cached_event_len;

  /*
    Some 16 flags. Only one is really used now; look above for
    LOG_EVENT_TIME_F, LOG_EVENT_FORCED_ROTATE_F, LOG_EVENT_THREAD_SPECIFIC_F
    for notes.
  */
  uint16 flags;

  bool cache_stmt;
#ifndef MYSQL_CLIENT
  THD* thd;

  Log_event(THD* thd_arg, uint16 flags_arg, bool cache_stmt);
  Log_event();
  /*
    read_log_event() functions read an event from a binlog or relay log; used by
    SHOW BINLOG EVENTS, the binlog_dump thread on the master (reads master's
    binlog), the slave IO thread (reads the event sent by binlog_dump), the
    slave SQL thread (reads the event from the relay log).
  */
  // if mutex is 0, the read will proceed without mutex
  static Log_event* read_log_event(IO_CACHE* file,
				   pthread_mutex_t* log_lock,
				   bool old_format);
  static int read_log_event(IO_CACHE* file, String* packet,
			    pthread_mutex_t* log_lock);
  /* set_log_pos() is used to fill log_pos with tell(log). */
  void set_log_pos(MYSQL_LOG* log);
  /*
    init_show_field_list() prepares the column names and types for the output of
    SHOW BINLOG EVENTS; it is used only by SHOW BINLOG EVENTS.
  */
  static void init_show_field_list(List<Item>* field_list);
#ifdef HAVE_REPLICATION
  int net_send(Protocol *protocol, const char* log_name, my_off_t pos);
  /*
    pack_info() is used by SHOW BINLOG EVENTS; as print() it prepares and sends
    a string to display to the user, so it resembles print().
  */
  virtual void pack_info(Protocol *protocol);
  /*
    The SQL slave thread calls exec_event() to execute the event; this is where
    the slave's data is modified.
  */
  virtual int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
  virtual const char* get_db()
  {
    return thd ? thd->db : 0;
  }
#else
 // avoid having to link mysqlbinlog against libpthread
  static Log_event* read_log_event(IO_CACHE* file, bool old_format);
  /* print*() functions are used by mysqlbinlog */
  virtual void print(FILE* file, bool short_form = 0, char* last_db = 0) = 0;
  void print_timestamp(FILE* file, time_t *ts = 0);
  void print_header(FILE* file);
#endif  

  static void *operator new(size_t size)
  {
    return (void*) my_malloc((uint)size, MYF(MY_WME|MY_FAE));
  }
  static void operator delete(void *ptr, size_t size)
  {
    my_free((gptr) ptr, MYF(MY_WME|MY_ALLOW_ZERO_PTR));
  }
  
  int write(IO_CACHE* file);
  int write_header(IO_CACHE* file);
  virtual int write_data(IO_CACHE* file)
  { return write_data_header(file) || write_data_body(file); }
  virtual int write_data_header(IO_CACHE* file __attribute__((unused)))
  { return 0; }
  virtual int write_data_body(IO_CACHE* file __attribute__((unused)))
  { return 0; }
  virtual Log_event_type get_type_code() = 0;
  virtual bool is_valid() = 0;
  inline bool get_cache_stmt() { return cache_stmt; }
  Log_event(const char* buf, bool old_format);
  virtual ~Log_event() { free_temp_buf();}
  void register_temp_buf(char* buf) { temp_buf = buf; }
  void free_temp_buf()
  {
    if (temp_buf)
    {
      my_free(temp_buf, MYF(0));
      temp_buf = 0;
    }
  }
  virtual int get_data_size() { return 0;}
  virtual int get_data_body_offset() { return 0; }
  int get_event_len()
  {
    return (cached_event_len ? cached_event_len :
	    (cached_event_len = LOG_EVENT_HEADER_LEN + get_data_size()));
  }
  static Log_event* read_log_event(const char* buf, int event_len,
				   const char **error, bool old_format);
  /* returns the human readable name of the event's type */
  const char* get_type_str();
};


/*****************************************************************************

  Query Log Event class

  Logs SQL queries

 ****************************************************************************/
class Query_log_event: public Log_event
{
protected:
  char* data_buf;
public:
  const char* query;
  const char* db;
  /*
    If we already know the length of the query string
    we pass it with q_len, so we would not have to call strlen()
    otherwise, set it to 0, in which case, we compute it with strlen()
  */
  uint32 q_len;
  uint32 db_len;
  uint16 error_code;
  ulong thread_id;
  /* 
     For events created by Query_log_event::exec_event (and
     Load_log_event::exec_event()) we need the *original* thread id, to be able
     to log the event with the original (=master's) thread id (fix for
     BUG#1686).
  */
  ulong slave_proxy_id;
#ifndef MYSQL_CLIENT

  Query_log_event(THD* thd_arg, const char* query_arg, ulong query_length,
		  bool using_trans);
  const char* get_db() { return db; }
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif

  Query_log_event(const char* buf, int event_len, bool old_format);
  ~Query_log_event()
  {
    if (data_buf)
    {
      my_free((gptr) data_buf, MYF(0));
    }
  }
  Log_event_type get_type_code() { return QUERY_EVENT; }
  int write(IO_CACHE* file);
  int write_data(IO_CACHE* file); // returns 0 on success, -1 on error
  bool is_valid() { return query != 0; }
  int get_data_size()
  {
    return (q_len + db_len + 2
	    + 4	// thread_id
	    + 4	// exec_time
	    + 2	// error_code
	    );
  }
};

#ifdef HAVE_REPLICATION

/*****************************************************************************

  Slave Log Event class
  Note that this class is currently not used at all; no code writes a
  Slave_log_event (though some code in repl_failsafe.cc reads Slave_log_event).

 ****************************************************************************/
class Slave_log_event: public Log_event
{
protected:
  char* mem_pool;
  void init_from_mem_pool(int data_size);
public:
  my_off_t master_pos;
  char* master_host;
  char* master_log;
  int master_host_len;
  int master_log_len;
  uint16 master_port;

#ifndef MYSQL_CLIENT  
  Slave_log_event(THD* thd_arg, struct st_relay_log_info* rli);
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  

  Slave_log_event(const char* buf, int event_len);
  ~Slave_log_event();
  int get_data_size();
  bool is_valid() { return master_host != 0; }
  Log_event_type get_type_code() { return SLAVE_EVENT; }
  int write_data(IO_CACHE* file );
};

#endif /* HAVE_REPLICATION */


/*****************************************************************************

  Load Log Event class

 ****************************************************************************/
class Load_log_event: public Log_event
{
protected:
  int copy_log_event(const char *buf, ulong event_len, bool old_format);

public:
  ulong thread_id;
  ulong slave_proxy_id;
  uint32 table_name_len;
  uint32 db_len;
  uint32 fname_len;
  uint32 num_fields;
  const char* fields;
  const uchar* field_lens;
  uint32 field_block_len;

  const char* table_name;
  const char* db;
  const char* fname;
  uint32 skip_lines;
  sql_ex_info sql_ex;
  bool local_fname;

  /* fname doesn't point to memory inside Log_event::temp_buf  */
  void set_fname_outside_temp_buf(const char *afname, uint alen)
  {
    fname= afname;
    fname_len= alen;
    local_fname= TRUE;
  }
  /* fname doesn't point to memory inside Log_event::temp_buf  */
  int  check_fname_outside_temp_buf()
  {
    return local_fname;
  }

#ifndef MYSQL_CLIENT
  String field_lens_buf;
  String fields_buf;
  
  Load_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
		 const char* table_name_arg,
		 List<Item>& fields_arg, enum enum_duplicates handle_dup,
		 bool using_trans);
  void set_fields(const char* db, List<Item> &fields_arg);
  const char* get_db() { return db; }
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli)
  {
    return exec_event(thd->slave_net,rli,0);
  }
  int exec_event(NET* net, struct st_relay_log_info* rli, 
		 bool use_rli_only_for_errors);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
  void print(FILE* file, bool short_form, char* last_db, bool commented);
#endif

  Load_log_event(const char* buf, int event_len, bool old_format);
  ~Load_log_event()
  {}
  Log_event_type get_type_code()
  {
    return sql_ex.new_format() ? NEW_LOAD_EVENT: LOAD_EVENT;
  }
  int write_data_header(IO_CACHE* file); 
  int write_data_body(IO_CACHE* file); 
  bool is_valid() { return table_name != 0; }
  int get_data_size()
  {
    return (table_name_len + 2 + db_len + 2 + fname_len
	    + 4 // thread_id
	    + 4 // exec_time
	    + 4 // skip_lines
	    + 4 // field block len
	    + sql_ex.data_size() + field_block_len + num_fields);
  }
  int get_data_body_offset() { return LOAD_EVENT_OVERHEAD; }
};

extern char server_version[SERVER_VERSION_LENGTH];

/*****************************************************************************

  Start Log Event class

 ****************************************************************************/
class Start_log_event: public Log_event
{
public:
  /* 
     If this event is at the start of the first binary log since server startup
     'created' should be the timestamp when the event (and the binary log) was
     created. 
     In the other case (i.e. this event is at the start of a binary log created
     by FLUSH LOGS or automatic rotation), 'created' should be 0.
     This "trick" is used by MySQL >=4.0.14 slaves to know if they must drop the
     stale temporary tables or not.
     Note that when 'created'!=0, it is always equal to the event's timestamp;
     indeed Start_log_event is written only in log.cc where the first
     constructor below is called, in which 'created' is set to 'when'. 
     So in fact 'created' is a useless variable. When it is 0
     we can read the actual value from timestamp ('when') and when it is
     non-zero we can read the same value from timestamp ('when'). Conclusion:
     - we use timestamp to print when the binlog was created.
     - we use 'created' only to know if this is a first binlog or not.
     In 3.23.57 we did not pay attention to this identity, so mysqlbinlog in
     3.23.57 does not print 'created the_date' if created was zero. This is now
     fixed.
  */
  time_t created;
  uint16 binlog_version;
  char server_version[ST_SERVER_VER_LEN];

#ifndef MYSQL_CLIENT
  Start_log_event() :Log_event(), binlog_version(BINLOG_VERSION)
  {
    created = (time_t) when;
    memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
  }
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  

  Start_log_event(const char* buf, bool old_format);
  ~Start_log_event() {}
  Log_event_type get_type_code() { return START_EVENT;}
  int write_data(IO_CACHE* file);
  bool is_valid() { return 1; }
  int get_data_size()
  {
    return START_HEADER_LEN;
  }
};


/*****************************************************************************

  Intvar Log Event class

  Logs special variables such as auto_increment values

 ****************************************************************************/
class Intvar_log_event: public Log_event
{
public:
  ulonglong val;
  uchar type;

#ifndef MYSQL_CLIENT  
  Intvar_log_event(THD* thd_arg,uchar type_arg, ulonglong val_arg)
    :Log_event(thd_arg,0,0),val(val_arg),type(type_arg)
  {}
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  

  Intvar_log_event(const char* buf, bool old_format);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  const char* get_var_type_name();
  int get_data_size() { return  9; /* sizeof(type) + sizeof(val) */;}
  int write_data(IO_CACHE* file);
  bool is_valid() { return 1; }
};

/*****************************************************************************

  Rand Log Event class

  Logs random seed used by the next RAND(), and by PASSWORD() in 4.1.

 ****************************************************************************/
class Rand_log_event: public Log_event
{
 public:
  ulonglong seed1;
  ulonglong seed2;

#ifndef MYSQL_CLIENT
  Rand_log_event(THD* thd_arg, ulonglong seed1_arg, ulonglong seed2_arg)
    :Log_event(thd_arg,0,0),seed1(seed1_arg),seed2(seed2_arg)
  {}
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif

  Rand_log_event(const char* buf, bool old_format);
  ~Rand_log_event() {}
  Log_event_type get_type_code() { return RAND_EVENT;}
  int get_data_size() { return 16; /* sizeof(ulonglong) * 2*/ }
  int write_data(IO_CACHE* file);
  bool is_valid() { return 1; }
};

/*****************************************************************************

  User var Log Event class

  Every time a query uses the value of a user variable, a User_var_log_event is
  written before the Query_log_event, to set the user variable.

 ****************************************************************************/
class User_var_log_event: public Log_event
{
public:
  char *name;
  uint name_len;
  char *val;
  ulong val_len;
  Item_result type;
  uint charset_number;
  bool is_null;
#ifndef MYSQL_CLIENT
  User_var_log_event(THD* thd_arg, char *name_arg, uint name_len_arg,
                     char *val_arg, ulong val_len_arg, Item_result type_arg,
		     uint charset_number_arg)
    :Log_event(), name(name_arg), name_len(name_len_arg), val(val_arg),
    val_len(val_len_arg), type(type_arg), charset_number(charset_number_arg)
    { is_null= !val; }
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif

  User_var_log_event(const char* buf, bool old_format);
  ~User_var_log_event() {}
  Log_event_type get_type_code() { return USER_VAR_EVENT;}
  int get_data_size()
    {
      return (is_null ? UV_NAME_LEN_SIZE + name_len + UV_VAL_IS_NULL :
	UV_NAME_LEN_SIZE + name_len + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
	UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE + val_len);
    }
  int write_data(IO_CACHE* file);
  bool is_valid() { return 1; }
};

/*****************************************************************************

  Stop Log Event class

 ****************************************************************************/
#ifdef HAVE_REPLICATION

class Stop_log_event: public Log_event
{
public:
#ifndef MYSQL_CLIENT
  Stop_log_event() :Log_event()
  {}
  int exec_event(struct st_relay_log_info* rli);
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  

  Stop_log_event(const char* buf, bool old_format):
    Log_event(buf, old_format)
  {}
  ~Stop_log_event() {}
  Log_event_type get_type_code() { return STOP_EVENT;}
  bool is_valid() { return 1; }
};

#endif /* HAVE_REPLICATION */


/*****************************************************************************

  Rotate Log Event class

  This will be depricated when we move to using sequence ids.

 ****************************************************************************/
class Rotate_log_event: public Log_event
{
public:
  const char* new_log_ident;
  ulonglong pos;
  uint ident_len;
  bool alloced;
#ifndef MYSQL_CLIENT  
  Rotate_log_event(THD* thd_arg, const char* new_log_ident_arg,
		   uint ident_len_arg = 0,
		   ulonglong pos_arg = LOG_EVENT_OFFSET)
    :Log_event(), new_log_ident(new_log_ident_arg),
    pos(pos_arg),ident_len(ident_len_arg ? ident_len_arg :
			   (uint) strlen(new_log_ident_arg)), alloced(0)
  {}
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif

  Rotate_log_event(const char* buf, int event_len, bool old_format);
  ~Rotate_log_event()
  {
    if (alloced)
      my_free((gptr) new_log_ident, MYF(0));
  }
  Log_event_type get_type_code() { return ROTATE_EVENT;}
  int get_data_size() { return  ident_len + ROTATE_HEADER_LEN;}
  bool is_valid() { return new_log_ident != 0; }
  int write_data(IO_CACHE* file);
};

/* the classes below are for the new LOAD DATA INFILE logging */

/*****************************************************************************

  Create File Log Event class

 ****************************************************************************/
class Create_file_log_event: public Load_log_event
{
protected:
  /*
    Pretend we are Load event, so we can write out just
    our Load part - used on the slave when writing event out to
    SQL_LOAD-*.info file
  */
  bool fake_base; 
public:
  char* block;
  const char *event_buf;
  uint block_len;
  uint file_id;
  bool inited_from_old;

#ifndef MYSQL_CLIENT
  Create_file_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
			const char* table_name_arg,
			List<Item>& fields_arg,
			enum enum_duplicates handle_dup,
			char* block_arg, uint block_len_arg,
			bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
  void print(FILE* file, bool short_form, char* last_db, bool enable_local);
#endif  
  
  Create_file_log_event(const char* buf, int event_len, bool old_format);
  ~Create_file_log_event()
  {
    my_free((char*) event_buf, MYF(MY_ALLOW_ZERO_PTR));
  }

  Log_event_type get_type_code()
  {
    return fake_base ? Load_log_event::get_type_code() : CREATE_FILE_EVENT;
  }
  int get_data_size()
  {
    return (fake_base ? Load_log_event::get_data_size() :
	    Load_log_event::get_data_size() +
	    4 + 1 + block_len);
  }
  int get_data_body_offset()
  {
    return (fake_base ? LOAD_EVENT_OVERHEAD:
	    LOAD_EVENT_OVERHEAD + CREATE_FILE_HEADER_LEN);
  }
  bool is_valid() { return inited_from_old || block != 0; }
  int write_data_header(IO_CACHE* file);
  int write_data_body(IO_CACHE* file);
  /*
    Cut out Create_file extentions and
    write it as Load event - used on the slave
  */
  int write_base(IO_CACHE* file);
};


/*****************************************************************************

  Append Block Log Event class

 ****************************************************************************/
class Append_block_log_event: public Log_event
{
public:
  char* block;
  uint block_len;
  uint file_id;
  /*
    'db' is filled when the event is created in mysql_load() (the event needs to
    have a 'db' member to be well filtered by binlog-*-db rules). 'db' is not
    written to the binlog (it's not used by Append_block_log_event::write()), so
    it can't be read in the Append_block_log_event(const char* buf, int
    event_len) constructor.
    In other words, 'db' is used only for filtering by binlog-*-db rules.
    Create_file_log_event is different: its 'db' (which is inherited from
    Load_log_event) is written to the binlog and can be re-read.
  */
  const char* db;

#ifndef MYSQL_CLIENT
  Append_block_log_event(THD* thd, const char* db_arg, char* block_arg,
			 uint block_len_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  int exec_event(struct st_relay_log_info* rli);
  void pack_info(Protocol* protocol);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif
  
  Append_block_log_event(const char* buf, int event_len);
  ~Append_block_log_event() {}
  Log_event_type get_type_code() { return APPEND_BLOCK_EVENT;}
  int get_data_size() { return  block_len + APPEND_BLOCK_HEADER_LEN ;}
  bool is_valid() { return block != 0; }
  int write_data(IO_CACHE* file);
  const char* get_db() { return db; }
};

/*****************************************************************************

  Delete File Log Event class

 ****************************************************************************/
class Delete_file_log_event: public Log_event
{
public:
  uint file_id;
  const char* db; /* see comment in Append_block_log_event */
  
#ifndef MYSQL_CLIENT
  Delete_file_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
  void print(FILE* file, bool short_form, char* last_db, bool enable_local);
#endif  
  
  Delete_file_log_event(const char* buf, int event_len);
  ~Delete_file_log_event() {}
  Log_event_type get_type_code() { return DELETE_FILE_EVENT;}
  int get_data_size() { return DELETE_FILE_HEADER_LEN ;}
  bool is_valid() { return file_id != 0; }
  int write_data(IO_CACHE* file);
  const char* get_db() { return db; }
};

/*****************************************************************************

  Execute Load Log Event class

 ****************************************************************************/
class Execute_load_log_event: public Log_event
{
public:
  uint file_id;
  const char* db; /* see comment in Append_block_log_event */ 

#ifndef MYSQL_CLIENT
  Execute_load_log_event(THD* thd, const char* db_arg, bool using_trans);
#ifdef HAVE_REPLICATION
  void pack_info(Protocol* protocol);
  int exec_event(struct st_relay_log_info* rli);
#endif /* HAVE_REPLICATION */
#else
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
  
  Execute_load_log_event(const char* buf, int event_len);
  ~Execute_load_log_event() {}
  Log_event_type get_type_code() { return EXEC_LOAD_EVENT;}
  int get_data_size() { return  EXEC_LOAD_HEADER_LEN ;}
  bool is_valid() { return file_id != 0; }
  int write_data(IO_CACHE* file);
  const char* get_db() { return db; }
};

#ifdef MYSQL_CLIENT
class Unknown_log_event: public Log_event
{
public:
  Unknown_log_event(const char* buf, bool old_format):
    Log_event(buf, old_format)
  {}
  ~Unknown_log_event() {}
  void print(FILE* file, bool short_form= 0, char* last_db= 0);
  Log_event_type get_type_code() { return UNKNOWN_EVENT;}
  bool is_valid() { return 1; }
};
#endif  

#endif /* _log_event_h */
