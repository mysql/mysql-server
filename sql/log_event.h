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
#define L_DATA_OFFSET    LOAD_HEADER_LEN


#define QUERY_EVENT_OVERHEAD  (LOG_EVENT_HEADER_LEN+QUERY_HEADER_LEN)
#define QUERY_DATA_OFFSET (LOG_EVENT_HEADER_LEN+QUERY_HEADER_LEN)
#define ROTATE_EVENT_OVERHEAD LOG_EVENT_HEADER_LEN
#define LOAD_EVENT_OVERHEAD   (LOG_EVENT_HEADER_LEN+LOAD_HEADER_LEN+sizeof(sql_ex_info))

#define BINLOG_MAGIC        "\xfe\x62\x69\x6e"

#define LOG_EVENT_TIME_F           0x1

enum Log_event_type { START_EVENT = 1, QUERY_EVENT =2,
		      STOP_EVENT=3, ROTATE_EVENT = 4, INTVAR_EVENT=5,
                      LOAD_EVENT=6, SLAVE_EVENT=7};
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
  virtual int write_data(IO_CACHE* file __attribute__((unused))) { return 0; }
  virtual Log_event_type get_type_code() = 0;
  Log_event(time_t when_arg, ulong exec_time_arg = 0,
	    int valid_exec_time = 0, uint32 server_id_arg = 0,
	    uint32 log_seq_arg = 0, uint16 flags_arg = 0):
    when(when_arg), exec_time(exec_time_arg),
    log_seq(log_seq_arg),flags(0)
  {
    server_id = server_id_arg ? server_id_arg : (::server_id);
    if(valid_exec_time)
      flags |= LOG_EVENT_TIME_F;
  }

  Log_event(const char* buf)
  {
   when = uint4korr(buf);
   server_id = uint4korr(buf + SERVER_ID_OFFSET);
   log_seq = uint4korr(buf + LOG_SEQ_OFFSET);
   flags = uint2korr(buf + FLAGS_OFFSET);
  }

  virtual ~Log_event() {}

  virtual int get_data_size() { return 0;}
  virtual void print(FILE* file, bool short_form = 0, char* last_db = 0) = 0;

  void print_timestamp(FILE* file, time_t *ts = 0);
  void print_header(FILE* file);

  // if mutex is 0, the read will proceed without mutex
  static Log_event* read_log_event(IO_CACHE* file, pthread_mutex_t* log_lock);
  static Log_event* read_log_event(const char* buf, int event_len);
  const char* get_type_str();

#ifndef MYSQL_CLIENT
  static int read_log_event(IO_CACHE* file, String* packet,
			    pthread_mutex_t* log_lock);
  void set_log_seq(THD* thd, MYSQL_LOG* log);
  virtual void pack_info(String* packet);
  int net_send(THD* thd, const char* log_name, ulong pos);
  static void init_show_field_list(List<Item>* field_list);
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
  THD* thd;
  bool cache_stmt;
  Query_log_event(THD* thd_arg, const char* query_arg, bool using_trans=0):
    Log_event(thd_arg->start_time,0,1,thd_arg->server_id,thd_arg->log_seq),
    data_buf(0),
    query(query_arg),  db(thd_arg->db), q_len(thd_arg->query_length),
    error_code(thd_arg->killed ? ER_SERVER_SHUTDOWN: thd_arg->net.last_errno),
    thread_id(thd_arg->thread_id), thd(thd_arg),
    cache_stmt(using_trans &&
	       (thd_arg->options & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN)))
  {
    time_t end_time;
    time(&end_time);
    exec_time = (ulong) (end_time  - thd->start_time);
    db_len = (db) ? (uint32) strlen(db) : 0;
  }
  
  void pack_info(String* packet);
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
  int get_data_size()
  {
    return q_len + db_len + 2 +
      sizeof(uint32) // thread_id
      + sizeof(uint32) // exec_time
      + sizeof(uint16) // error_code
      ;
  }

  void print(FILE* file, bool short_form = 0, char* last_db = 0);
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
#endif
  
  Slave_log_event(const char* buf, int event_len);
  ~Slave_log_event();
  int get_data_size();
  Log_event_type get_type_code() { return SLAVE_EVENT; }
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
  int write_data(IO_CACHE* file );

};

#define DUMPFILE_FLAG 0x1
#define OPT_ENCLOSED_FLAG 0x2
#define REPLACE_FLAG  0x4
#define IGNORE_FLAG   0x8

#define FIELD_TERM_EMPTY 0x1
#define ENCLOSED_EMPTY   0x2
#define LINE_TERM_EMPTY  0x4
#define LINE_START_EMPTY 0x8
#define ESCAPED_EMPTY    0x10


struct sql_ex_info
  {
    char field_term;
    char enclosed;
    char line_term;
    char line_start;
    char escaped;
    char opt_flags; // flags for the options
    char empty_flags; // flags to indicate which of the terminating charact
  } ;

class Load_log_event: public Log_event
{
protected:
  char* data_buf;
  void copy_log_event(const char *buf, ulong data_len);

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
  THD* thd;
  String field_lens_buf;
  String fields_buf;
  Load_log_event(THD* thd, sql_exchange* ex, const char* table_name_arg,
		 List<Item>& fields_arg, enum enum_duplicates handle_dup ):
    Log_event(thd->start_time),data_buf(0),thread_id(thd->thread_id),
    num_fields(0),fields(0),field_lens(0),field_block_len(0),
    table_name(table_name_arg),
    db(thd->db),
    fname(ex->file_name),
    thd(thd)
  {
    time_t end_time;
    time(&end_time);
    exec_time = (ulong) (end_time  - thd->start_time);
    db_len = (db) ? (uint32) strlen(db) : 0;
    table_name_len = (table_name) ? (uint32) strlen(table_name) : 0;
    fname_len = (fname) ? (uint) strlen(fname) : 0;
    sql_ex.field_term = (*ex->field_term)[0];
    sql_ex.enclosed = (*ex->enclosed)[0];
    sql_ex.line_term = (*ex->line_term)[0];
    sql_ex.line_start = (*ex->line_start)[0];
    sql_ex.escaped = (*ex->escaped)[0];
    sql_ex.opt_flags = 0;
    if(ex->dumpfile)
      sql_ex.opt_flags |= DUMPFILE_FLAG;
    if(ex->opt_enclosed)
      sql_ex.opt_flags |= OPT_ENCLOSED_FLAG;

    sql_ex.empty_flags = 0;

    switch(handle_dup)
      {
      case DUP_IGNORE: sql_ex.opt_flags |= IGNORE_FLAG; break;
      case DUP_REPLACE: sql_ex.opt_flags |= REPLACE_FLAG; break;
      case DUP_ERROR: break;	
      }

    if(!ex->field_term->length())
      sql_ex.empty_flags |= FIELD_TERM_EMPTY;
    if(!ex->enclosed->length())
      sql_ex.empty_flags |= ENCLOSED_EMPTY;
    if(!ex->line_term->length())
      sql_ex.empty_flags |= LINE_TERM_EMPTY;
    if(!ex->line_start->length())
      sql_ex.empty_flags |= LINE_START_EMPTY;
    if(!ex->escaped->length())
      sql_ex.empty_flags |= ESCAPED_EMPTY;
    
    skip_lines = ex->skip_lines;

    List_iterator<Item> li(fields_arg);
    field_lens_buf.length(0);
    fields_buf.length(0);
    Item* item;
    while((item = li++))
      {
	num_fields++;
	uchar len = (uchar) strlen(item->name);
	field_block_len += len + 1;
	fields_buf.append(item->name, len + 1);
	field_lens_buf.append((char*)&len, 1);
      }

    field_lens = (const uchar*)field_lens_buf.ptr();
    fields = fields_buf.ptr();
  }
  void set_fields(List<Item> &fields_arg);
  void pack_info(String* packet);
#endif

  Load_log_event(const char* buf, int event_len);
  ~Load_log_event()
  {
    if (data_buf)
    {
      my_free((gptr) data_buf, MYF(0));
    }
  }
  Log_event_type get_type_code() { return LOAD_EVENT; }
  int write_data(IO_CACHE* file); // returns 0 on success, -1 on error
  int get_data_size()
  {
    return table_name_len + 2 + db_len + 2 + fname_len
      + 4 // thread_id
      + 4 // exec_time
      + 4 // skip_lines
      + 4 // field block len
      + sizeof(sql_ex) + field_block_len + num_fields*sizeof(uchar) ;
      ;
  }

  void print(FILE* file, bool short_form = 0, char* last_db = 0);
};

extern char server_version[SERVER_VERSION_LENGTH];

class Start_log_event: public Log_event
{
public:
  uint32 created;
  uint16 binlog_version;
  char server_version[ST_SERVER_VER_LEN];
  
  Start_log_event() :Log_event(time(NULL)),binlog_version(BINLOG_VERSION)
  {
    created = (uint32) when;
    memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
  }
  Start_log_event(const char* buf);
  
  ~Start_log_event() {}
  Log_event_type get_type_code() { return START_EVENT;}
  int write_data(IO_CACHE* file);
  int get_data_size()
  {
    return START_HEADER_LEN;
  }
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
#endif  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
};

class Intvar_log_event: public Log_event
{
public:
  ulonglong val;
  uchar type;
  Intvar_log_event(uchar type_arg, ulonglong val_arg)
    :Log_event(time(NULL)),val(val_arg),type(type_arg)
  {}
  Intvar_log_event(const char* buf);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  const char* get_var_type_name();
  int get_data_size() { return  sizeof(type) + sizeof(val);}
  int write_data(IO_CACHE* file);
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
#endif  
  
  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
};

class Stop_log_event: public Log_event
{
public:
  Stop_log_event() :Log_event(time(NULL))
  {}
  Stop_log_event(const char* buf):Log_event(buf)
  {
  }
  ~Stop_log_event() {}
  Log_event_type get_type_code() { return STOP_EVENT;}
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
};

class Rotate_log_event: public Log_event
{
public:
  const char* new_log_ident;
  uchar ident_len;
  bool alloced;
  
  Rotate_log_event(const char* new_log_ident_arg, uint ident_len_arg = 0) :
    Log_event(time(NULL)),
    new_log_ident(new_log_ident_arg),
    ident_len(ident_len_arg ? ident_len_arg : (uint) strlen(new_log_ident_arg)),
    alloced(0)
  {}
  
  Rotate_log_event(const char* buf, int event_len);
  ~Rotate_log_event()
  {
    if (alloced)
      my_free((gptr) new_log_ident, MYF(0));
  }
  Log_event_type get_type_code() { return ROTATE_EVENT;}
  int get_data_size() { return  ident_len;}
  int write_data(IO_CACHE* file);
  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
#ifndef MYSQL_CLIENT
  void pack_info(String* packet);
#endif  
};

#endif



