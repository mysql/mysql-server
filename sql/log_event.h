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


#ifndef _LOG_EVENT_H
#define _LOG_EVENT_H

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
#define BINLOG_VERSION    2

/* we could have used SERVER_VERSION_LENGTH, but this introduces an
   obscure dependency - if somebody decided to change SERVER_VERSION_LENGTH
   this would have broke the replication protocol
*/
#define ST_SERVER_VER_LEN 50

#define DUMPFILE_FLAG 0x1
#define OPT_ENCLOSED_FLAG 0x2
#define REPLACE_FLAG  0x4
#define IGNORE_FLAG   0x8

#define FIELD_TERM_EMPTY 0x1
#define ENCLOSED_EMPTY   0x2
#define LINE_TERM_EMPTY  0x4
#define LINE_START_EMPTY 0x8
#define ESCAPED_EMPTY    0x10

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


struct sql_ex_info
  {
    char* field_term;
    char* enclosed;
    char* line_term;
    char* line_start;
    char* escaped;
    uint8 field_term_len,enclosed_len,line_term_len,line_start_len,
      escaped_len;
    char opt_flags; 
    char empty_flags;
    int cached_new_format;
    
    // store in new format even if old is possible
    void force_new_format() { cached_new_format = 1;} 
    int data_size() { return new_format() ?
			field_term_len + enclosed_len + line_term_len +
			line_start_len + escaped_len + 6 : 7;}
    int write_data(IO_CACHE* file);
    char* init(char* buf,char* buf_end,bool use_new_format);
    bool new_format()
      {
	return (cached_new_format != -1) ? cached_new_format :
	  (cached_new_format=(field_term_len > 1 ||
			      enclosed_len > 1 ||
			      line_term_len > 1 || line_start_len > 1 ||
			      escaped_len > 1));
      }
  } ;

/* Binary log consists of events. Each event has a fixed length header,
   followed by possibly variable ( depending on the type of event) length
   data body. The data body consists of an optional fixed length segment
   (post-header), and an optional variable length segment. See #defines and
   comments below for the format specifics
*/


/* event-specific post-header sizes */
#define LOG_EVENT_HEADER_LEN 19
#define QUERY_HEADER_LEN     (4 + 4 + 1 + 2)
#define LOAD_HEADER_LEN      (4 + 4 + 4 + 1 +1 + 4)
#define START_HEADER_LEN     (2 + ST_SERVER_VER_LEN + 4)
#define ROTATE_HEADER_LEN    8
#define CREATE_FILE_HEADER_LEN 4
#define APPEND_BLOCK_HEADER_LEN 4
#define EXEC_LOAD_HEADER_LEN   4
#define DELETE_FILE_HEADER_LEN 4

/* event header offsets */

#define EVENT_TYPE_OFFSET    4
#define SERVER_ID_OFFSET     5
#define EVENT_LEN_OFFSET     9
#define LOG_SEQ_OFFSET       13
#define FLAGS_OFFSET         17

/* start event post-header */

#define ST_BINLOG_VER_OFFSET  0
#define ST_SERVER_VER_OFFSET  2
#define ST_CREATED_OFFSET     (ST_SERVER_VER_OFFSET + ST_SERVER_VER_LEN)

/* slave event post-header */

#define SL_MASTER_PORT_OFFSET   8
#define SL_MASTER_POS_OFFSET    0
#define SL_MASTER_HOST_OFFSET   10

/* query event post-header */

#define Q_THREAD_ID_OFFSET   0
#define Q_EXEC_TIME_OFFSET   4
#define Q_DB_LEN_OFFSET      8
#define Q_ERR_CODE_OFFSET    9
#define Q_DATA_OFFSET    QUERY_HEADER_LEN

/* Intvar event post-header */

#define I_TYPE_OFFSET        0
#define I_VAL_OFFSET         1

/* Load event post-header */

#define L_THREAD_ID_OFFSET   0
#define L_EXEC_TIME_OFFSET   4
#define L_SKIP_LINES_OFFSET  8
#define L_DB_LEN_OFFSET      12
#define L_TBL_LEN_OFFSET     13
#define L_NUM_FIELDS_OFFSET  14
#define L_SQL_EX_OFFSET      18
#define L_DATA_OFFSET    LOAD_HEADER_LEN

/* Rotate event post-header */

#define R_POS_OFFSET       0
#define R_IDENT_OFFSET     8

#define CF_FILE_ID_OFFSET  0
#define CF_DATA_OFFSET     CREATE_FILE_HEADER_LEN

#define AB_FILE_ID_OFFSET  0
#define AB_DATA_OFFSET     APPEND_BLOCK_HEADER_LEN

#define EL_FILE_ID_OFFSET  0

#define DF_FILE_ID_OFFSET  0

#define QUERY_EVENT_OVERHEAD  (LOG_EVENT_HEADER_LEN+QUERY_HEADER_LEN)
#define QUERY_DATA_OFFSET (LOG_EVENT_HEADER_LEN+QUERY_HEADER_LEN)
#define ROTATE_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+ROTATE_HEADER_LEN)
#define LOAD_EVENT_OVERHEAD   (LOG_EVENT_HEADER_LEN+LOAD_HEADER_LEN)
#define CREATE_FILE_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+\
 +LOAD_HEADER_LEN+CREATE_FILE_HEADER_LEN)
#define DELETE_FILE_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+DELETE_FILE_HEADER_LEN)
#define EXEC_LOAD_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+EXEC_LOAD_HEADER_LEN)
#define APPEND_BLOCK_EVENT_OVERHEAD (LOG_EVENT_HEADER_LEN+APPEND_BLOCK_HEADER_LEN)


#define BINLOG_MAGIC        "\xfe\x62\x69\x6e"

#define LOG_EVENT_TIME_F           0x1
#define LOG_EVENT_FORCED_ROTATE_F  0x2

enum Log_event_type { START_EVENT = 1, QUERY_EVENT =2,
		      STOP_EVENT=3, ROTATE_EVENT = 4, INTVAR_EVENT=5,
                      LOAD_EVENT=6, SLAVE_EVENT=7, CREATE_FILE_EVENT=8,
 APPEND_BLOCK_EVENT=9, EXEC_LOAD_EVENT=10, DELETE_FILE_EVENT=11,
 NEW_LOAD_EVENT=12};
enum Int_event_type { INVALID_INT_EVENT = 0, LAST_INSERT_ID_EVENT = 1, INSERT_ID_EVENT = 2
 };

#ifndef MYSQL_CLIENT
class String;
class MYSQL_LOG;
class THD;
#endif

extern uint32 server_id;

struct st_master_info;

class Log_event
{
public:
  time_t when;
  ulong exec_time;
  uint32 server_id;
  uint32 log_seq;
  uint16 flags;
  int cached_event_len;
  char* temp_buf;
#ifndef MYSQL_CLIENT  
  THD* thd;
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
  virtual bool get_cache_stmt() { return 0; }
  Log_event(const char* buf);
#ifndef MYSQL_CLIENT  
  Log_event(THD* thd_arg, uint16 flags_arg = 0);
#endif  
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
  int get_event_len() { return cached_event_len ? cached_event_len :
    (cached_event_len = LOG_EVENT_HEADER_LEN + get_data_size()); }
#ifdef MYSQL_CLIENT  
  virtual void print(FILE* file, bool short_form = 0, char* last_db = 0) = 0;
  void print_timestamp(FILE* file, time_t *ts = 0);
  void print_header(FILE* file);
#endif
  
#ifndef MYSQL_CLIENT  
  // if mutex is 0, the read will proceed without mutex
  static Log_event* read_log_event(IO_CACHE* file, pthread_mutex_t* log_lock);
#else // avoid having to link mysqlbinlog against libpthread
  static Log_event* read_log_event(IO_CACHE* file);
#endif  
  static Log_event* read_log_event(const char* buf, int event_len);
  const char* get_type_str();

#ifndef MYSQL_CLIENT
  static int read_log_event(IO_CACHE* file, String* packet,
			    pthread_mutex_t* log_lock);
  void set_log_seq(THD* thd, MYSQL_LOG* log);
  virtual void pack_info(String* packet);
  int net_send(THD* thd, const char* log_name, my_off_t pos);
  static void init_show_field_list(List<Item>* field_list);
  virtual int exec_event(struct st_master_info* mi);
  virtual const char* get_db()
    {
      return thd ? thd->db : 0;
    }
#endif
  
};


class Query_log_event: public Log_event
{
protected:
  char* data_buf;
public:
  const char* query;
  const char* db;
  uint32 q_len; // if we already know the length of the query string
  // we pass it here, so we would not have to call strlen()
  // otherwise, set it to 0, in which case, we compute it with strlen()
  uint32 db_len;
  uint16 error_code;
  ulong thread_id;
#if !defined(MYSQL_CLIENT)
  bool cache_stmt;

  Query_log_event(THD* thd_arg, const char* query_arg,
		 bool using_trans=0);
  const char* get_db() { return db; }
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
  bool get_cache_stmt() { return cache_stmt; }
#endif

  Query_log_event(const char* buf, int event_len);
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
    return q_len + db_len + 2 +
      4		// thread_id
      + 4	// exec_time
      + 2	// error_code
      ;
  }
#ifdef MYSQL_CLIENT
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
};

class Slave_log_event: public Log_event
{
protected:
  char* mem_pool;
  void init_from_mem_pool(int data_size);
public:
  char* master_host;
  int master_host_len;
  uint16 master_port;
  char* master_log;
  int master_log_len;
  ulonglong master_pos;

#ifndef MYSQL_CLIENT  
  Slave_log_event(THD* thd_arg, struct st_master_info* mi);
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
#endif
  
  Slave_log_event(const char* buf, int event_len);
  ~Slave_log_event();
  int get_data_size();
  bool is_valid() { return master_host != 0; }
  Log_event_type get_type_code() { return SLAVE_EVENT; }
#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
  int write_data(IO_CACHE* file );

};

class Load_log_event: public Log_event
{
protected:
  int copy_log_event(const char *buf, ulong event_len);

public:
  ulong thread_id;
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
  
#if !defined(MYSQL_CLIENT)
  String field_lens_buf;
  String fields_buf;
  
  Load_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
			       const char* table_name_arg,
		 List<Item>& fields_arg, enum enum_duplicates handle_dup);
  void set_fields(List<Item> &fields_arg);
  void pack_info(String* packet);
  const char* get_db() { return db; }
  int exec_event(struct st_master_info* mi)
    {
      return exec_event(thd->slave_net,mi);
    }
  int exec_event(NET* net, struct st_master_info* mi);
#endif

  Load_log_event(const char* buf, int event_len);
  ~Load_log_event()
  {
  }
  Log_event_type get_type_code() { return sql_ex.new_format() ?
				     NEW_LOAD_EVENT: LOAD_EVENT; }
  int write_data_header(IO_CACHE* file); 
  int write_data_body(IO_CACHE* file); 
  bool is_valid() { return table_name != 0; }
  int get_data_size()
  {
    return table_name_len + 2 + db_len + 2 + fname_len
      + 4 // thread_id
      + 4 // exec_time
      + 4 // skip_lines
      + 4 // field block len
      + sql_ex.data_size() + field_block_len + num_fields;
      ;
  }
  int get_data_body_offset() { return LOAD_EVENT_OVERHEAD; }
#ifdef MYSQL_CLIENT
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
};

extern char server_version[SERVER_VERSION_LENGTH];

class Start_log_event: public Log_event
{
public:
  uint32 created;
  uint16 binlog_version;
  char server_version[ST_SERVER_VER_LEN];
#ifndef MYSQL_CLIENT  
  Start_log_event() :Log_event((THD*)0),binlog_version(BINLOG_VERSION)
  {
    created = (uint32) when;
    memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
  }
#endif  
  Start_log_event(const char* buf);
  ~Start_log_event() {}
  Log_event_type get_type_code() { return START_EVENT;}
  int write_data(IO_CACHE* file);
  bool is_valid() { return 1; }
  int get_data_size()
  {
    return START_HEADER_LEN;
  }
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
#endif
#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
};

class Intvar_log_event: public Log_event
{
public:
  ulonglong val;
  uchar type;
#ifndef MYSQL_CLIENT  
  Intvar_log_event(THD* thd_arg,uchar type_arg, ulonglong val_arg)
    :Log_event(thd_arg),val(val_arg),type(type_arg)
  {}
#endif
  Intvar_log_event(const char* buf);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  const char* get_var_type_name();
  int get_data_size() { return  sizeof(type) + sizeof(val);}
  int write_data(IO_CACHE* file);
  bool is_valid() { return 1; }
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
#endif  
  
#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
};

class Stop_log_event: public Log_event
{
public:
#ifndef MYSQL_CLIENT  
  Stop_log_event() :Log_event((THD*)0)
  {}
#endif  
  Stop_log_event(const char* buf):Log_event(buf)
  {
  }
  ~Stop_log_event() {}
  Log_event_type get_type_code() { return STOP_EVENT;}
  bool is_valid() { return 1; }
#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif
#ifndef MYSQL_CLIENT
  int exec_event(struct st_master_info* mi);
#endif  
};

class Rotate_log_event: public Log_event
{
public:
  const char* new_log_ident;
  uchar ident_len;
  ulonglong pos;
  bool alloced;
#ifndef MYSQL_CLIENT  
  Rotate_log_event(THD* thd_arg, const char* new_log_ident_arg,
		   uint ident_len_arg = 0,ulonglong pos_arg = 4) :
    Log_event(thd_arg),
    new_log_ident(new_log_ident_arg),
    ident_len(ident_len_arg ? ident_len_arg :
	      (uint) strlen(new_log_ident_arg)), pos(pos_arg),
    alloced(0)
  {}
#endif  
  Rotate_log_event(const char* buf, int event_len);
  ~Rotate_log_event()
  {
    if (alloced)
      my_free((gptr) new_log_ident, MYF(0));
  }
  Log_event_type get_type_code() { return ROTATE_EVENT;}
  int get_data_size() { return  ident_len + ROTATE_HEADER_LEN;}
  bool is_valid() { return new_log_ident != 0; }
  int write_data(IO_CACHE* file);
#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
#endif  
};

/* the classes below are for the new LOAD DATA INFILE logging */

class Create_file_log_event: public Load_log_event
{
protected:
  // pretend we are Load event, so we can write out just
  // our Load part - used on the slave when writing event out to
  // SQL_LOAD-*.info file
  bool fake_base; 
public:
  char* block;
  uint block_len;
  uint file_id;
#ifndef MYSQL_CLIENT
  Create_file_log_event(THD* thd, sql_exchange* ex, const char* db_arg,
			       const char* table_name_arg,
		 List<Item>& fields_arg, enum enum_duplicates handle_dup,
		       char* block_arg, uint block_len_arg);
#endif  
  
  Create_file_log_event(const char* buf, int event_len);
  ~Create_file_log_event()
  {
  }
  Log_event_type get_type_code()
    {
     return fake_base ? Load_log_event::get_type_code() : CREATE_FILE_EVENT;
    }
  int get_data_size() { return  fake_base ? Load_log_event::get_data_size() :
			  Load_log_event::get_data_size() +
			  4 + 1 + block_len;}
  int get_data_body_offset() { return fake_base ? LOAD_EVENT_OVERHEAD:
    LOAD_EVENT_OVERHEAD + CREATE_FILE_HEADER_LEN; }
  bool is_valid() { return block != 0; }
  int write_data_header(IO_CACHE* file);
  int write_data_body(IO_CACHE* file);
  int write_base(IO_CACHE* file); // cut out Create_file extentions and
  // write it as Load event - used on the slave

#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
#endif  
};

class Append_block_log_event: public Log_event
{
public:
  char* block;
  uint block_len;
  uint file_id;
  
#ifndef MYSQL_CLIENT
  Append_block_log_event(THD* thd, char* block_arg,
		       uint block_len_arg);
  int exec_event(struct st_master_info* mi);
#endif  
  
  Append_block_log_event(const char* buf, int event_len);
  ~Append_block_log_event()
  {
  }
  Log_event_type get_type_code() { return APPEND_BLOCK_EVENT;}
  int get_data_size() { return  block_len + APPEND_BLOCK_HEADER_LEN ;}
  bool is_valid() { return block != 0; }
  int write_data(IO_CACHE* file);

#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
#endif  
};

class Delete_file_log_event: public Log_event
{
public:
  uint file_id;
  
#ifndef MYSQL_CLIENT
  Delete_file_log_event(THD* thd);
#endif  
  
  Delete_file_log_event(const char* buf, int event_len);
  ~Delete_file_log_event()
  {
  }
  Log_event_type get_type_code() { return DELETE_FILE_EVENT;}
  int get_data_size() { return DELETE_FILE_HEADER_LEN ;}
  bool is_valid() { return file_id != 0; }
  int write_data(IO_CACHE* file);

#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
#endif  
};

class Execute_load_log_event: public Log_event
{
public:
  uint file_id;
  
#ifndef MYSQL_CLIENT
  Execute_load_log_event(THD* thd);
#endif  
  
  Execute_load_log_event(const char* buf, int event_len);
  ~Execute_load_log_event()
  {
  }
  Log_event_type get_type_code() { return EXEC_LOAD_EVENT;}
  int get_data_size() { return  EXEC_LOAD_HEADER_LEN ;}
  bool is_valid() { return file_id != 0; }
  int write_data(IO_CACHE* file);

#ifdef MYSQL_CLIENT  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#endif  
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
  int exec_event(struct st_master_info* mi);
#endif  
};


#endif



