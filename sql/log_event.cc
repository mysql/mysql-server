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


#ifndef MYSQL_CLIENT
#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif
#include  "mysql_priv.h"
#endif /* MYSQL_CLIENT */


static void pretty_print_char(FILE* file, int c)
{
  fputc('\'', file);
  switch(c) {
  case '\n': fprintf(file, "\\n"); break;
  case '\r': fprintf(file, "\\r"); break;
  case '\\': fprintf(file, "\\\\"); break;
  case '\b': fprintf(file, "\\b"); break;
  case '\'': fprintf(file, "\\'"); break;
  case 0   : fprintf(file, "\\0"); break;
  default:
    fputc(c, file);
    break;
  }
  fputc('\'', file);
}

int Query_log_event::write(IO_CACHE* file)
{
  return query ? Log_event::write(file) : -1; 
}

int Log_event::write(IO_CACHE* file)
{
  return (write_header(file) || write_data(file)) ? -1 : 0;
}

int Log_event::write_header(IO_CACHE* file)
{
  // make sure to change this when the header gets bigger
  char buf[LOG_EVENT_HEADER_LEN];
  char* pos = buf;
  int4store(pos, when); // timestamp
  pos += 4;
  *pos++ = get_type_code(); // event type code
  int4store(pos, server_id);
  pos += 4;
  long tmp=get_data_size() + LOG_EVENT_HEADER_LEN;
  int4store(pos, tmp);
  pos += 4;
  return (my_b_write(file, (byte*) buf, (uint) (pos - buf)));
}

#ifndef MYSQL_CLIENT

int Log_event::read_log_event(IO_CACHE* file, String* packet,
			      pthread_mutex_t* log_lock)
{
  ulong data_len;
  char buf[LOG_EVENT_HEADER_LEN];
  if (log_lock)
    pthread_mutex_lock(log_lock);
  if (my_b_read(file, (byte*) buf, sizeof(buf)))
  {
    if (log_lock) pthread_mutex_unlock(log_lock);
    // if the read hits eof, we must report it as eof
    // so the caller will know it can go into cond_wait to be woken up
    // on the next update to the log
    if(!file->error) return LOG_READ_EOF;
    return file->error > 0 ? LOG_READ_TRUNC: LOG_READ_IO;
  }
  data_len = uint4korr(buf + EVENT_LEN_OFFSET);
  if (data_len < LOG_EVENT_HEADER_LEN || data_len > max_allowed_packet)
  {
    if (log_lock) pthread_mutex_unlock(log_lock);
    return (data_len < LOG_EVENT_HEADER_LEN) ? LOG_READ_BOGUS :
      LOG_READ_TOO_LARGE;
  }
  packet->append(buf, sizeof(buf));
  data_len -= LOG_EVENT_HEADER_LEN;
  if (data_len)
  {
    if (packet->append(file, data_len))
    {
      if(log_lock)
	pthread_mutex_unlock(log_lock);
      // here we should never hit eof in a non-error condtion
      // eof means we are reading the event partially, which should
      // never happen
      return file->error >= 0 ? LOG_READ_TRUNC: LOG_READ_IO;
    }
  }
  if (log_lock) pthread_mutex_unlock(log_lock);
  return 0;
}

#endif // MYSQL_CLIENT

// allocates memory - the caller is responsible for clean-up

Log_event* Log_event::read_log_event(IO_CACHE* file, pthread_mutex_t* log_lock)
{
  time_t timestamp;
  uint32 server_id;
  
  char buf[LOG_EVENT_HEADER_LEN-4];
  if(log_lock) pthread_mutex_lock(log_lock);
  if (my_b_read(file, (byte *) buf, sizeof(buf)))
  {
    if (log_lock) pthread_mutex_unlock(log_lock);
    return NULL;
  }
  timestamp = uint4korr(buf);
  server_id = uint4korr(buf + 5);
  
  switch(buf[EVENT_TYPE_OFFSET])
  {
  case QUERY_EVENT:
  {
    Query_log_event* q = new Query_log_event(file, timestamp, server_id);
    if(log_lock) pthread_mutex_unlock(log_lock);
    if (!q->query)
    {
      delete q;
      q=NULL;
    }
    return q;
  }
  
  case LOAD_EVENT:
  {
    Load_log_event* l = new Load_log_event(file, timestamp, server_id);
    if(log_lock) pthread_mutex_unlock(log_lock);
    if (!l->table_name)
    {
      delete l;
      l=NULL;
    }
    return l;
  }


  case ROTATE_EVENT:
  {
    Rotate_log_event* r = new Rotate_log_event(file, timestamp, server_id);
    if(log_lock) pthread_mutex_unlock(log_lock);
    
    if (!r->new_log_ident)
    {
      delete r;
      r=NULL;
    }
    return r;
  }

  case INTVAR_EVENT:
  {
    Intvar_log_event* e = new Intvar_log_event(file, timestamp, server_id);
    if(log_lock) pthread_mutex_unlock(log_lock);
    
    if (e->type == INVALID_INT_EVENT)
    {
      delete e;
      e=NULL;
    }
    return e;
  }
  
  case START_EVENT:
    {
      Start_log_event* e = new Start_log_event(file, timestamp, server_id);
      if(log_lock) pthread_mutex_unlock(log_lock);
      return e;
    }	  
  case STOP_EVENT:
    {
      Stop_log_event* e = new Stop_log_event(file, timestamp, server_id);
      if(log_lock) pthread_mutex_unlock(log_lock);
      return e;
    }
  default:
    break;
  }

  // default
  if (log_lock) pthread_mutex_unlock(log_lock);
  return NULL;
}

Log_event* Log_event::read_log_event(const char* buf, int event_len)
{
  if(event_len < EVENT_LEN_OFFSET ||
     (uint)event_len != uint4korr(buf+EVENT_LEN_OFFSET))
    return NULL; // general sanity check - will fail on a partial read
  
  switch(buf[EVENT_TYPE_OFFSET])
  {
  case QUERY_EVENT:
  {
    Query_log_event* q = new Query_log_event(buf, event_len);
    if (!q->query)
    {
      delete q;
      return NULL;
    }

    return q;
  }

  case LOAD_EVENT:
  {
    Load_log_event* l = new Load_log_event(buf, event_len);
    if (!l->table_name)
    {
      delete l;
      return NULL;
    }

    return l;
  }

  case ROTATE_EVENT:
  {
    Rotate_log_event* r = new Rotate_log_event(buf, event_len);
    if (!r->new_log_ident)
    {
      delete r;
      return NULL;
    }

    return r;
  }
  case START_EVENT:  return  new Start_log_event(buf);
  case STOP_EVENT:  return  new Stop_log_event(buf);
  case INTVAR_EVENT:  return  new Intvar_log_event(buf);
  default:
    break;
  }
  return NULL;  // default value
}

void Log_event::print_header(FILE* file)
{
  fputc('#', file);
  print_timestamp(file);
  fprintf(file, " server id  %d ", server_id); 
}

void Log_event::print_timestamp(FILE* file, time_t* ts)
{
  struct tm tm_tmp;
  if (!ts)
  {
    ts = &when;
  }
  localtime_r(ts,&tm_tmp);

  fprintf(file,"%02d%02d%02d %2d:%02d:%02d",
	  tm_tmp.tm_year % 100,
	  tm_tmp.tm_mon+1,
	  tm_tmp.tm_mday,
	  tm_tmp.tm_hour,
	  tm_tmp.tm_min,
	  tm_tmp.tm_sec);
}


void Start_log_event::print(FILE* file, bool short_form, char* last_db)
{
  if (short_form)
    return;

  print_header(file);
  fprintf(file, "\tStart: binlog v %d, server v %s created ", binlog_version,
	  server_version);
  print_timestamp(file, (time_t*)&created);
  fputc('\n', file);
  fflush(file);
}

void Stop_log_event::print(FILE* file, bool short_form, char* last_db)
{
  if (short_form)
    return;

  print_header(file);
  fprintf(file, "\tStop\n");
  fflush(file);
}

void Rotate_log_event::print(FILE* file, bool short_form, char* last_db)
{
  if (short_form)
    return;

  print_header(file);
  fprintf(file, "\tRotate to ");
  if (new_log_ident)
    my_fwrite(file, (byte*) new_log_ident, (uint)ident_len, 
	      MYF(MY_NABP | MY_WME));
  fprintf(file, "\n");
  fflush(file);
}

Rotate_log_event::Rotate_log_event(IO_CACHE* file, time_t when_arg,
				   uint32 server_id):
  Log_event(when_arg, 0, 0, server_id),new_log_ident(NULL),alloced(0)
{
  char *tmp_ident;
  char buf[4];

  if (my_b_read(file, (byte*) buf, sizeof(buf)))
    return;
  ulong event_len;
  event_len = uint4korr(buf);
  if (event_len < ROTATE_EVENT_OVERHEAD)
    return;

  ident_len = (uchar)(event_len - ROTATE_EVENT_OVERHEAD);
  if (!(tmp_ident = (char*) my_malloc((uint)ident_len, MYF(MY_WME))))
    return;
  if (my_b_read( file, (byte*) tmp_ident, (uint) ident_len))
  {
    my_free((gptr) tmp_ident, MYF(0));
    return;
  }

  new_log_ident = tmp_ident;
  alloced = 1;
}

Start_log_event::Start_log_event(const char* buf) :Log_event(buf)
{
  buf += EVENT_LEN_OFFSET + 4; // skip even length
  binlog_version = uint2korr(buf);
  memcpy(server_version, buf + 2, sizeof(server_version));
  created = uint4korr(buf + 2 + sizeof(server_version));
}

int Start_log_event::write_data(IO_CACHE* file)
{
  char buff[sizeof(server_version)+2+4];
  int2store(buff,binlog_version);
  memcpy(buff+2,server_version,sizeof(server_version));
  int4store(buff+2+sizeof(server_version),created);
  return (my_b_write(file, (byte*) buff, sizeof(buff)) ? -1 : 0);
}

Rotate_log_event::Rotate_log_event(const char* buf, int event_len):
  Log_event(buf),new_log_ident(NULL),alloced(0)
{
  // the caller will ensure that event_len is what we have at
  // EVENT_LEN_OFFSET
  if(event_len < ROTATE_EVENT_OVERHEAD)
    return;

  ident_len = (uchar)(event_len - ROTATE_EVENT_OVERHEAD);
  if (!(new_log_ident = (char*) my_memdup((byte*) buf + LOG_EVENT_HEADER_LEN,
					  (uint) ident_len, MYF(MY_WME))))
    return;

  alloced = 1;
}

int Rotate_log_event::write_data(IO_CACHE* file)
{
  return my_b_write(file, (byte*) new_log_ident, (uint) ident_len) ? -1 :0;
}

Query_log_event::Query_log_event(IO_CACHE* file, time_t when_arg,
				 uint32 server_id):
  Log_event(when_arg,0,0,server_id),data_buf(0),query(NULL),db(NULL)
{
  char buf[QUERY_HEADER_LEN + 4];
  ulong data_len;
  if (my_b_read(file, (byte*) buf, sizeof(buf)))
    return;				// query == NULL will tell the
					// caller there was a problem
  data_len = uint4korr(buf);
  if (data_len < QUERY_EVENT_OVERHEAD)
    return;				// tear-drop attack protection :)

  data_len -= QUERY_EVENT_OVERHEAD;
  exec_time = uint4korr(buf + 8);
  db_len = (uint)buf[12];
  error_code = uint2korr(buf + 13);
  
  /* Allocate one byte extra for end \0 */
  if (!(data_buf = (char*) my_malloc(data_len+1, MYF(MY_WME))))
    return;
  if (my_b_read( file, (byte*) data_buf, data_len))
  {
    my_free((gptr) data_buf, MYF(0));
    data_buf = 0;
    return;
  }

  thread_id = uint4korr(buf + 4);
  db = data_buf;
  query=data_buf + db_len + 1;
  q_len = data_len - 1 - db_len;
  *((char*) query + q_len) = 0;			// Safety
}

Query_log_event::Query_log_event(const char* buf, int event_len):
  Log_event(buf),data_buf(0), query(NULL), db(NULL)
{
  if ((uint)event_len < QUERY_EVENT_OVERHEAD)
    return;				
  ulong data_len;
  buf += EVENT_LEN_OFFSET;
  data_len = event_len - QUERY_EVENT_OVERHEAD;

  exec_time = uint4korr(buf + 8);
  error_code = uint2korr(buf + 13);

  if (!(data_buf = (char*) my_malloc(data_len + 1, MYF(MY_WME))))
    return;

  memcpy(data_buf, buf + QUERY_HEADER_LEN + 4, data_len);
  thread_id = uint4korr(buf + 4);
  db = data_buf;
  db_len = (uint)buf[12];
  query=data_buf + db_len + 1;
  q_len = data_len - 1 - db_len;
  *((char*)query+q_len) = 0;
}

void Query_log_event::print(FILE* file, bool short_form, char* last_db)
{
  char buff[40],*end;				// Enough for SET TIMESTAMP
  if (!short_form)
  {
    print_header(file);
    fprintf(file, "\tQuery\tthread_id=%lu\texec_time=%lu\terror_code=%d\n",
	    (ulong) thread_id, (ulong) exec_time, error_code);
  }

  bool same_db = 0;

  if(db && last_db)
    {
      if(!(same_db = !memcmp(last_db, db, db_len)))
        memcpy(last_db, db, db_len + 1);
    }
  
  if (db && db[0] && !same_db)
    fprintf(file, "use %s;\n", db);
  end=int10_to_str((long) when, strmov(buff,"SET TIMESTAMP="),10);
  *end++=';';
  *end++='\n';
  my_fwrite(file, (byte*) buff, (uint) (end-buff),MYF(MY_NABP | MY_WME));
  my_fwrite(file, (byte*) query, q_len, MYF(MY_NABP | MY_WME));
  fprintf(file, ";\n");
}

int Query_log_event::write_data(IO_CACHE* file)
{
  if (!query) return -1;
  
  char buf[QUERY_HEADER_LEN]; 
  char* pos = buf;
  int4store(pos, thread_id);
  pos += 4;
  int4store(pos, exec_time);
  pos += 4;
  *pos++ = (char)db_len;
  int2store(pos, error_code);
  pos += 2;

  return (my_b_write(file, (byte*) buf, (uint)(pos - buf)) ||
	  my_b_write(file, (db) ? (byte*) db : (byte*)"", db_len + 1) ||
	  my_b_write(file, (byte*) query, q_len)) ? -1 : 0;
}

Intvar_log_event:: Intvar_log_event(IO_CACHE* file, time_t when_arg,
				    uint32 server_id)
  :Log_event(when_arg,0,0,server_id), type(INVALID_INT_EVENT)
{
  char buf[9+4];
  if (!my_b_read(file, (byte*) buf, sizeof(buf)))
  {
    type = buf[4];
    val = uint8korr(buf+1+4);
  }
}

Intvar_log_event::Intvar_log_event(const char* buf):Log_event(buf)
{
  buf += LOG_EVENT_HEADER_LEN;
  type = buf[0];
  val = uint8korr(buf+1);
}

int Intvar_log_event::write_data(IO_CACHE* file)
{
  char buf[9];
  buf[0] = type;
  int8store(buf + 1, val);
  return my_b_write(file, (byte*) buf, sizeof(buf));
}

void Intvar_log_event::print(FILE* file, bool short_form, char* last_db)
{
  char llbuff[22];
  if(!short_form)
  {
    print_header(file);
    fprintf(file, "\tIntvar\n");
  }

  fprintf(file, "SET ");
  switch(type)
  {
  case LAST_INSERT_ID_EVENT:
    fprintf(file, "LAST_INSERT_ID = ");
    break;
  case INSERT_ID_EVENT:
    fprintf(file, "INSERT_ID = ");
    break;
  }
  fprintf(file, "%s;\n", llstr(val,llbuff));
  fflush(file);
  
}

int Load_log_event::write_data(IO_CACHE* file)
{
  char buf[LOAD_HEADER_LEN];
  int4store(buf, thread_id);
  int4store(buf + 4, exec_time);
  int4store(buf + 8, skip_lines);
  buf[12] = (char)table_name_len;
  buf[13] = (char)db_len;
  int4store(buf + 14, num_fields);
  
  if(my_b_write(file, (byte*)buf, sizeof(buf)) ||
     my_b_write(file, (byte*)&sql_ex, sizeof(sql_ex)))
    return 1;

  if (num_fields && fields && field_lens)
  {
    if(my_b_write(file, (byte*)field_lens, num_fields) ||
       my_b_write(file, (byte*)fields, field_block_len))
      return 1;
  }
  if(my_b_write(file, (byte*)table_name, table_name_len + 1) ||
     my_b_write(file, (byte*)db, db_len + 1) ||
     my_b_write(file, (byte*)fname, fname_len))
    return 1;
  return 0;
}

Load_log_event::Load_log_event(IO_CACHE* file, time_t when, uint32 server_id):
  Log_event(when,0,0,server_id),data_buf(0),num_fields(0),
  fields(0),field_lens(0),field_block_len(0),
  table_name(0),db(0),fname(0)
{
  char buf[LOAD_HEADER_LEN + 4];
  ulong data_len;
  if (my_b_read(file, (byte*)buf, sizeof(buf)) ||
      my_b_read(file, (byte*)&sql_ex, sizeof(sql_ex)))
    return;

  data_len = uint4korr(buf) - LOAD_EVENT_OVERHEAD;
  if (!(data_buf = (char*)my_malloc(data_len + 1, MYF(MY_WME))))
    return;
  if (my_b_read(file, (byte*)data_buf, data_len))
    return;
  copy_log_event(buf,data_len);
}

Load_log_event::Load_log_event(const char* buf, int event_len):
  Log_event(buf),data_buf(0),num_fields(0),fields(0),
  field_lens(0),field_block_len(0),
  table_name(0),db(0),fname(0)
{
  ulong data_len;

  if((uint)event_len < (LOAD_EVENT_OVERHEAD + LOG_EVENT_HEADER_LEN))
    return;
  buf += EVENT_LEN_OFFSET;
  memcpy(&sql_ex, buf + LOAD_HEADER_LEN + 4, sizeof(sql_ex));
  data_len = event_len;
  
  if(!(data_buf = (char*)my_malloc(data_len + 1, MYF(MY_WME))))
    return;
  memcpy(data_buf, buf + 22 + sizeof(sql_ex), data_len);
  copy_log_event(buf, data_len);
}

void Load_log_event::copy_log_event(const char *buf, ulong data_len)
{
  thread_id = uint4korr(buf+4);
  exec_time = uint4korr(buf+8);
  skip_lines = uint4korr(buf + 12);
  table_name_len = (uint)buf[16];
  db_len = (uint)buf[17];
  num_fields = uint4korr(buf + 18);
	  
  if (num_fields > data_len) // simple sanity check against corruption
    return;

  field_lens = (uchar*) data_buf;
  uint i;
  for (i = 0; i < num_fields; i++)
  {
    field_block_len += (uint)field_lens[i] + 1;
  }
  fields = (char*)field_lens + num_fields;
  
  *((char*)data_buf+data_len) = 0;
  table_name  = fields + field_block_len;
  db = table_name + table_name_len + 1;
  fname = db + db_len + 1;
  fname_len = data_len - 2 - db_len - table_name_len - num_fields -
    field_block_len;
}


void Load_log_event::print(FILE* file, bool short_form, char* last_db)
{
  if (!short_form)
  {
    print_header(file);
    fprintf(file, "\tQuery\tthread_id=%ld\texec_time=%ld\n",
	    thread_id, exec_time);
  }

  bool same_db = 0;

  if(db && last_db)
    {
      if(!(same_db = !memcmp(last_db, db, db_len)))
        memcpy(last_db, db, db_len + 1);
    }
  
  if(db && db[0] && !same_db)
    fprintf(file, "use %s;\n", db);

  fprintf(file, "LOAD DATA INFILE '%s' ", fname);

  if(sql_ex.opt_flags && REPLACE_FLAG )
    fprintf(file," REPLACE ");
  else if(sql_ex.opt_flags && IGNORE_FLAG )
    fprintf(file," IGNORE ");
  
  fprintf(file, "INTO TABLE %s ", table_name);
  if(!(sql_ex.empty_flags & FIELD_TERM_EMPTY))
  {
    fprintf(file, " FIELDS TERMINATED BY ");
    pretty_print_char(file, sql_ex.field_term);
  }

  if(!(sql_ex.empty_flags & ENCLOSED_EMPTY))
  {
    if(sql_ex.opt_flags && OPT_ENCLOSED_FLAG )
      fprintf(file," OPTIONALLY ");
    fprintf(file, " ENCLOSED BY ");
    pretty_print_char(file, sql_ex.enclosed);
  }
     
  if(!(sql_ex.empty_flags & ESCAPED_EMPTY))
  {
    fprintf(file, " ESCAPED BY ");
    pretty_print_char(file, sql_ex.escaped);
  }
     
  if(!(sql_ex.empty_flags & LINE_TERM_EMPTY))
  {
    fprintf(file," LINES TERMINATED BY ");
    pretty_print_char(file, sql_ex.line_term);
  }

  if(!(sql_ex.empty_flags & LINE_START_EMPTY))
  {
    fprintf(file," LINES STARTING BY ");
    pretty_print_char(file, sql_ex.line_start);
  }
     
  if((int)skip_lines > 0)
    fprintf(file, " IGNORE %d LINES ", skip_lines);

  if (num_fields)
  {
    uint i;
    const char* field = fields;
    fprintf( file, " (");
    for(i = 0; i < num_fields; i++)
    {
      if(i)
	fputc(',', file);
      fprintf(file, field);
	  
      field += field_lens[i]  + 1;
    }
    fputc(')', file);
  }

  fprintf(file, ";\n");
}

#ifndef MYSQL_CLIENT

void Load_log_event::set_fields(List<Item> &fields)
{
  uint i;
  const char* field = this->fields;
  for(i = 0; i < num_fields; i++)
    {
      fields.push_back(new Item_field(db, table_name, field));	  
      field += field_lens[i]  + 1;
    }
  
}

#endif
