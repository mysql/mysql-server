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

#if defined(__GNUC__) && !defined(MYSQL_CLIENT)
#pragma interface			/* gcc class implementation */
#endif

#define LOG_READ_EOF    -1
#define LOG_READ_BOGUS  -2
#define LOG_READ_IO     -3
#define LOG_READ_MEM    -5

#define LOG_EVENT_OFFSET 4


enum Log_event_type { START_EVENT = 1, QUERY_EVENT =2,
		      STOP_EVENT=3, ROTATE_EVENT = 4, INTVAR_EVENT=5,
                      LOAD_EVENT=6};
enum Int_event_type { INVALID_INT_EVENT = 0, LAST_INSERT_ID_EVENT = 1, INSERT_ID_EVENT = 2
 };

#ifndef MYSQL_CLIENT
class String;
#endif

class Log_event
{
public:
  time_t when;
  ulong exec_time;
  int valid_exec_time; // if false, the exec time setting is bogus and needs

  int write(FILE* file);
  int write_header(FILE* file);
  virtual int write_data(FILE* file __attribute__((unused))) { return 0; }
  virtual Log_event_type get_type_code() = 0;
  Log_event(time_t when_arg, ulong exec_time_arg = 0,
	    int valid_exec_time_arg = 0): when(when_arg),
    exec_time(exec_time_arg), valid_exec_time(valid_exec_time_arg) {}

  Log_event(const char* buf): valid_exec_time(1)
  {
   when = uint4korr(buf);
   exec_time = uint4korr(buf + 5);
  }

  virtual ~Log_event() {}

  virtual int get_data_size() { return 0;}
  virtual void print(FILE* file, bool short_form = 0) = 0;

  void print_timestamp(FILE* file);

  static Log_event* read_log_event(FILE* file);
  static Log_event* read_log_event(const char* buf, int max_buf);

#ifndef MYSQL_CLIENT
  static int read_log_event(FILE* file, String* packet);
#endif
  
};


class Query_log_event: public Log_event
{
protected:
  char* data_buf;
public:
  const char* query;
  const char* db;
  uint q_len; // if we already know the length of the query string
  // we pass it here, so we would not have to call strlen()
  // otherwise, set it to 0, in which case, we compute it with strlen()
  uint db_len;
  int thread_id;
#if !defined(MYSQL_CLIENT)
  THD* thd;
  Query_log_event(THD* thd_arg, const char* query_arg):
    Log_event(thd_arg->start_time), data_buf(0),
    query(query_arg),  db(thd_arg->db), q_len(thd_arg->query_length),
    thread_id(thd_arg->thread_id), thd(thd_arg)
  {
    time_t end_time;
    time(&end_time);
    exec_time = end_time  - thd->start_time;
    valid_exec_time = 1;
    db_len = (db) ? strlen(db) : 0;
  }
#endif

  Query_log_event(FILE* file, time_t when);
  Query_log_event(const char* buf, int max_buf);
  ~Query_log_event()
  {
    if (data_buf)
    {
      my_free((gptr)data_buf, MYF(0));
    }
  }
  Log_event_type get_type_code() { return QUERY_EVENT; }
  int write(FILE* file);
  int write_data(FILE* file); // returns 0 on success, -1 on error
  int get_data_size()
  {
    return q_len + db_len + 2 +
      sizeof(uint) // thread_id
      + sizeof(uint) // exec_time
      ;
  }

  void print(FILE* file, bool short_form = 0);
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
public:
  int thread_id;
  uint table_name_len;
  uint db_len;
  uint fname_len;
  uint num_fields;
  const char* fields;
  const uchar* field_lens;
  uint field_block_len;
  

  const char* table_name;
  const char* db;
  const char* fname;
  uint skip_lines;
  sql_ex_info sql_ex;
  
#if !defined(MYSQL_CLIENT)
  THD* thd;
  String field_lens_buf;
  String fields_buf;
  Load_log_event(THD* thd, sql_exchange* ex, const char* table_name,
		 List<Item>& fields, enum enum_duplicates handle_dup ):
    Log_event(thd->start_time),data_buf(0),thread_id(thd->thread_id),
    num_fields(0),fields(0),field_lens(0),field_block_len(0),
    table_name(table_name),
    db(thd->db),
    fname(ex->file_name),
    thd(thd)
  {
    time_t end_time;
    time(&end_time);
    exec_time = end_time  - thd->start_time;
    valid_exec_time = 1;
    db_len = (db) ? strlen(db) : 0;
    table_name_len = (table_name) ? strlen(table_name) : 0;
    fname_len = (fname) ? strlen(fname) : 0;
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

    List_iterator<Item> li(fields);
    field_lens_buf.length(0);
    fields_buf.length(0);
    Item* item;
    while((item = li++))
      {
	num_fields++;
	uchar len = (uchar)strlen(item->name);
	field_block_len += len + 1;
	fields_buf.append(item->name, len + 1);
	field_lens_buf.append((char*)&len, 1);
      }

    field_lens = (const uchar*)field_lens_buf.ptr();
    this->fields = fields_buf.ptr();
  }
  void set_fields(List<Item> &fields);
#endif

  Load_log_event(FILE* file, time_t when);
  Load_log_event(const char* buf, int max_buf);
  ~Load_log_event()
  {
    if (data_buf)
    {
      my_free((gptr)data_buf, MYF(0));
    }
  }
  Log_event_type get_type_code() { return LOAD_EVENT; }
  int write_data(FILE* file); // returns 0 on success, -1 on error
  int get_data_size()
  {
    return table_name_len + 2 + db_len + 2 + fname_len
      + sizeof(thread_id) // thread_id
      + sizeof(exec_time) // exec_time
      + sizeof(skip_lines)
      + sizeof(field_block_len)
      + sizeof(sql_ex) + field_block_len + num_fields*sizeof(uchar) ;
      ;
  }

  void print(FILE* file, bool short_form = 0);
};


class Start_log_event: public Log_event
{
public:
  Start_log_event() :Log_event(time(NULL))
  {}
  Start_log_event(FILE* file, time_t when_arg) :Log_event(when_arg)
  {
    my_fseek(file, 4L, MY_SEEK_CUR, MYF(MY_WME)); // skip the event length
  }
  Start_log_event(const char* buf) :Log_event(buf)
  {
  }
  ~Start_log_event() {}
  Log_event_type get_type_code() { return START_EVENT;}
  void print(FILE* file, bool short_form = 0);
};

class Intvar_log_event: public Log_event
{
public:
  ulonglong val;
  uchar type;
  Intvar_log_event(uchar type_arg, ulonglong val_arg)
    :Log_event(time(NULL)),val(val_arg),type(type_arg)
  {}
  Intvar_log_event(FILE* file, time_t when);
  Intvar_log_event(const char* buf);
  ~Intvar_log_event() {}
  Log_event_type get_type_code() { return INTVAR_EVENT;}
  int get_data_size() { return  sizeof(type) + sizeof(val);}
  int write_data(FILE* file);
  
  
  void print(FILE* file, bool short_form = 0);
};

class Stop_log_event: public Log_event
{
public:
  Stop_log_event() :Log_event(time(NULL))
  {}
  Stop_log_event(FILE* file, time_t when_arg): Log_event(when_arg)
  {
    my_fseek(file, 4L, MY_SEEK_CUR, MYF(MY_WME)); // skip the event length
  }
  Stop_log_event(const char* buf):Log_event(buf)
  {
  }
  ~Stop_log_event() {}
  Log_event_type get_type_code() { return STOP_EVENT;}
  void print(FILE* file, bool short_form = 0);
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
    ident_len(ident_len_arg ? ident_len_arg : strlen(new_log_ident_arg)),
    alloced(0)
  {}
  
  Rotate_log_event(FILE* file, time_t when) ;
  Rotate_log_event(const char* buf, int max_buf);
  ~Rotate_log_event()
  {
    if (alloced)
      my_free((gptr) new_log_ident, MYF(0));
  }
  Log_event_type get_type_code() { return ROTATE_EVENT;}
  int get_data_size() { return  ident_len;}
  int write_data(FILE* file);
  
  void print(FILE* file, bool short_form = 0);
};

#endif
