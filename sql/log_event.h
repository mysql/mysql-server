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
#define BINLOG_VERSION    1

#define LOG_EVENT_HEADER_LEN 13
#define QUERY_HEADER_LEN     (sizeof(uint32) + sizeof(uint32) + \
 sizeof(uchar) + sizeof(uint16))
#define LOAD_HEADER_LEN      (sizeof(uint32) + sizeof(uint32) + \
  + sizeof(uint32) + 2 + sizeof(uint32))
#define EVENT_LEN_OFFSET     9
#define EVENT_TYPE_OFFSET    4
#define QUERY_EVENT_OVERHEAD  (LOG_EVENT_HEADER_LEN+QUERY_HEADER_LEN)
#define ROTATE_EVENT_OVERHEAD LOG_EVENT_HEADER_LEN
#define LOAD_EVENT_OVERHEAD   (LOG_EVENT_HEADER_LEN+LOAD_HEADER_LEN+sizeof(sql_ex_info))

#define BINLOG_MAGIC        "\xfe\x62\x69\x6e"

enum Log_event_type { START_EVENT = 1, QUERY_EVENT =2,
		      STOP_EVENT=3, ROTATE_EVENT = 4, INTVAR_EVENT=5,
                      LOAD_EVENT=6};
enum Int_event_type { INVALID_INT_EVENT = 0, LAST_INSERT_ID_EVENT = 1, INSERT_ID_EVENT = 2
 };

#ifndef MYSQL_CLIENT
class String;
#endif

extern uint32 server_id;

class Log_event
{
public:
  time_t when;
  ulong exec_time;
  int valid_exec_time; // if false, the exec time setting is bogus 
  uint32 server_id;

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
	    int valid_exec_time_arg = 0, uint32 server_id_arg = 0):
    when(when_arg), exec_time(exec_time_arg),
    valid_exec_time(valid_exec_time_arg)
  {
    server_id = server_id_arg ? server_id_arg : (::server_id);
  }

  Log_event(const char* buf): valid_exec_time(0)
  {
   when = uint4korr(buf);
   server_id = uint4korr(buf + 5);
  }

  virtual ~Log_event() {}

  virtual int get_data_size() { return 0;}
  virtual void print(FILE* file, bool short_form = 0, char* last_db = 0) = 0;

  void print_timestamp(FILE* file, time_t *ts = 0);
  void print_header(FILE* file);

#ifndef MYSQL_CLIENT  
  // if mutex is 0, the read will proceed without mutex
  static Log_event* read_log_event(IO_CACHE* file, pthread_mutex_t* log_lock);
#else // avoid having to link mysqlbinlog against libpthread
  static Log_event* read_log_event(IO_CACHE* file);
#endif  
  static Log_event* read_log_event(const char* buf, int event_len);

#ifndef MYSQL_CLIENT
  static int read_log_event(IO_CACHE* file, String* packet,
			    pthread_mutex_t* log_lock);
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
    Log_event(thd_arg->start_time,0,1,thd_arg->server_id), data_buf(0),
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
    // do not log stray system errors such as EE_WRITE
    if (error_code < ERRMOD)
      error_code = 0; 
  }
#endif

  Query_log_event(IO_CACHE* file, time_t when, uint32 server_id_arg);
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
    valid_exec_time = 1;
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
#endif

  Load_log_event(IO_CACHE * file, time_t when, uint32 server_id_arg);
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
  char server_version[50];
  
  Start_log_event() :Log_event(time(NULL)),binlog_version(BINLOG_VERSION)
  {
    created = (uint32) when;
    memcpy(server_version, ::server_version, sizeof(server_version));
  }
  Start_log_event(IO_CACHE* file, time_t when_arg, uint32 server_id_arg) :
    Log_event(when_arg, 0, 0, server_id_arg)
  {
    char buf[sizeof(server_version) + 2 + 4 + 4];
    if (my_b_read(file, (byte*) buf, sizeof(buf)))
      return;				
    binlog_version = uint2korr(buf+4);
    memcpy(server_version, buf + 6, sizeof(server_version));
    server_version[sizeof(server_version)-1]=0;
    created = uint4korr(buf + 6 + sizeof(server_version));
  }
  Start_log_event(const char* buf);
  
  ~Start_log_event() {}
  Log_event_type get_type_code() { return START_EVENT;}
  int write_data(IO_CACHE* file);
  int get_data_size()
  {
    // sizeof(binlog_version) + sizeof(server_version) sizeof(created)
    return 2 + sizeof(server_version) + 4;
  }
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
  Intvar_log_event(IO_CACHE* file, time_t when, uint32 server_id_arg);
  Intvar_log_event(const char* buf);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  int get_data_size() { return  sizeof(type) + sizeof(val);}
  int write_data(IO_CACHE* file);
  
  
  void print(FILE* file, bool short_form = 0, char* last_db = 0);
};

class Stop_log_event: public Log_event
{
public:
  Stop_log_event() :Log_event(time(NULL))
  {}
  Stop_log_event(IO_CACHE* file, time_t when_arg, uint32 server_id_arg):
    Log_event(when_arg,0,0,server_id_arg)
  {
    byte skip[4];
    my_b_read(file, skip, sizeof(skip));	// skip the event length
  }
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
  
  Rotate_log_event(IO_CACHE* file, time_t when, uint32 server_id_arg) ;
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
};

#endif
