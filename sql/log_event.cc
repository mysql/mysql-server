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
#include "slave.h"
#endif /* MYSQL_CLIENT */


static void pretty_print_char(FILE* file, int c)
{
  fputc('\'', file);
  switch(c) {
  case '\n': fprintf(file, "\\n"); break;
  case '\r': fprintf(file, "\\r"); break;
  case '\\': fprintf(file, "\\\\"); break;
  case '\b': fprintf(file, "\\b"); break;
  case '\t': fprintf(file, "\\t"); break;
  case '\'': fprintf(file, "\\'"); break;
  case 0   : fprintf(file, "\\0"); break;
  default:
    fputc(c, file);
    break;
  }
  fputc('\'', file);
}

#ifndef MYSQL_CLIENT

static void pretty_print_char(String* packet, int c)
{
  packet->append('\'');
  switch(c) {
  case '\n': packet->append( "\\n"); break;
  case '\r': packet->append( "\\r"); break;
  case '\\': packet->append( "\\\\"); break;
  case '\b': packet->append( "\\b"); break;
  case '\t': packet->append( "\\t"); break;
  case '\'': packet->append( "\\'"); break;
  case 0   : packet->append( "\\0"); break;
  default:
    packet->append((char)c);
    break;
  }
  packet->append('\'');
}

#endif

const char* Log_event::get_type_str()
{
  switch(get_type_code())
  {
  case START_EVENT:  return "Start";
  case STOP_EVENT:   return "Stop";
  case QUERY_EVENT:  return "Query";
  case ROTATE_EVENT: return "Rotate";
  case INTVAR_EVENT: return "Intvar";
  case LOAD_EVENT:   return "Load";
  case SLAVE_EVENT:  return "Slave";
  default: /* impossible */ return "Unknown";
  }
}

#ifndef MYSQL_CLIENT

void Log_event::pack_info(String* packet)
{
  net_store_data(packet, "", 0);
}

void Query_log_event::pack_info(String* packet)
{
  String tmp;
  if(db && db_len)
  {
   tmp.append("use ");
   tmp.append(db, db_len);
   tmp.append("; ", 2);
  }

  if(query && q_len)
    tmp.append(query, q_len);
  net_store_data(packet, (char*)tmp.ptr(), tmp.length());
}

void Start_log_event::pack_info(String* packet)
{
  String tmp;
  char buf[22];

  tmp.append("Server ver: ");
  tmp.append(server_version);
  tmp.append(", Binlog ver: ");
  tmp.append(llstr(binlog_version, buf));
  net_store_data(packet, tmp.ptr(), tmp.length());
}

void Load_log_event::pack_info(String* packet)
{
  String tmp;
  if(db && db_len)
  {
   tmp.append("use ");
   tmp.append(db, db_len);
   tmp.append("; ", 2);
  }

  tmp.append("LOAD DATA INFILE '");
  tmp.append(fname);
  tmp.append("' ", 2);
  if(sql_ex.opt_flags && REPLACE_FLAG )
    tmp.append(" REPLACE ");
  else if(sql_ex.opt_flags && IGNORE_FLAG )
    tmp.append(" IGNORE ");
  
  tmp.append("INTO TABLE ");
  tmp.append(table_name);
  if (!(sql_ex.empty_flags & FIELD_TERM_EMPTY))
  {
    tmp.append(" FIELDS TERMINATED BY ");
    pretty_print_char(&tmp, sql_ex.field_term);
  }

  if (!(sql_ex.empty_flags & ENCLOSED_EMPTY))
  {
    if (sql_ex.opt_flags && OPT_ENCLOSED_FLAG )
      tmp.append(" OPTIONALLY ");
    tmp.append( " ENCLOSED BY ");
    pretty_print_char(&tmp, sql_ex.enclosed);
  }
     
  if (!(sql_ex.empty_flags & ESCAPED_EMPTY))
  {
    tmp.append( " ESCAPED BY ");
    pretty_print_char(&tmp, sql_ex.escaped);
  }
     
  if (!(sql_ex.empty_flags & LINE_TERM_EMPTY))
  {
    tmp.append(" LINES TERMINATED BY ");
    pretty_print_char(&tmp, sql_ex.line_term);
  }

  if (!(sql_ex.empty_flags & LINE_START_EMPTY))
  {
    tmp.append(" LINES STARTING BY ");
    pretty_print_char(&tmp, sql_ex.line_start);
  }
     
  if ((int)skip_lines > 0)
    tmp.append( " IGNORE %ld LINES ", (long) skip_lines);

  if (num_fields)
  {
    uint i;
    const char* field = fields;
    tmp.append(" (");
    for(i = 0; i < num_fields; i++)
    {
      if(i)
	tmp.append(" ,");
      tmp.append( field);
	  
      field += field_lens[i]  + 1;
    }
    tmp.append(')');
  }

  net_store_data(packet, tmp.ptr(), tmp.length());
}

void Rotate_log_event::pack_info(String* packet)
{
  net_store_data(packet, new_log_ident, ident_len);
}

void Intvar_log_event::pack_info(String* packet)
{
  String tmp;
  char buf[22];
  tmp.append(get_var_type_name());
  tmp.append('=');
  tmp.append(llstr(val, buf));
  net_store_data(packet, tmp.ptr(), tmp.length());
}

void Slave_log_event::pack_info(String* packet)
{
  net_store_data(packet, "", 0);
}


void Log_event::init_show_field_list(List<Item>* field_list)
{
  field_list->push_back(new Item_empty_string("Log_name", 20));
  field_list->push_back(new Item_empty_string("Pos", 20));
  field_list->push_back(new Item_empty_string("Event_type", 20));
  field_list->push_back(new Item_empty_string("Server_id", 20));
  field_list->push_back(new Item_empty_string("Log_seq", 20));
  field_list->push_back(new Item_empty_string("Info", 20));
}

int Log_event::net_send(THD* thd, const char* log_name, ulong pos)
{
  String* packet = &thd->packet;
  const char* p = strrchr(log_name, FN_LIBCHAR);
  const char* event_type;
  if (p)
    log_name = p + 1;
  
  packet->length(0);
  net_store_data(packet, log_name, strlen(log_name));
  net_store_data(packet, (longlong)pos);
  event_type = get_type_str();
  net_store_data(packet, event_type, strlen(event_type));
  net_store_data(packet, server_id);
  net_store_data(packet, log_seq);
  pack_info(packet);
  return my_net_write(&thd->net, (char*)packet->ptr(), packet->length());
}

#endif

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
  int4store(pos, log_seq);
  pos += 4;
  int2store(pos, flags);
  pos += 2;
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
  char head[LOG_EVENT_HEADER_LEN];
  if(log_lock) pthread_mutex_lock(log_lock);
  if (my_b_read(file, (byte *) head, sizeof(head)))
  {
    if (log_lock) pthread_mutex_unlock(log_lock);
    return 0;
  }

  uint data_len = uint4korr(head + EVENT_LEN_OFFSET);
  char* buf = 0;
  const char* error = 0;
  Log_event* res =  0;

  if (data_len > max_allowed_packet)
  {
    error = "Event too big";
    goto err;
  }

  if (data_len < LOG_EVENT_HEADER_LEN)
  {
    error = "Event too small";
    goto err;
  }
  
  if (!(buf = my_malloc(data_len, MYF(MY_WME))))
  {
    error = "Out of memory";
    goto err;
  }

  memcpy(buf, head, LOG_EVENT_HEADER_LEN);
  if(my_b_read(file, (byte*) buf + LOG_EVENT_HEADER_LEN,
	       data_len - LOG_EVENT_HEADER_LEN))
  {
    error = "read error";
    goto err;
  }
  res = read_log_event(buf, data_len);
err:
  if (log_lock) pthread_mutex_unlock(log_lock);
  if(error)
    sql_print_error(error);
  my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
  return res;
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
  case SLAVE_EVENT:
  {
    Slave_log_event* s = new Slave_log_event(buf, event_len);
    if (!s->master_host)
    {
      delete s;
      return NULL;
    }

    return s;
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

Start_log_event::Start_log_event(const char* buf) :Log_event(buf)
{
  binlog_version = uint2korr(buf + LOG_EVENT_HEADER_LEN +
			     ST_BINLOG_VER_OFFSET);
  memcpy(server_version, buf + ST_SERVER_VER_OFFSET + LOG_EVENT_HEADER_LEN,
	 ST_SERVER_VER_LEN);
  created = uint4korr(buf + ST_CREATED_OFFSET + LOG_EVENT_HEADER_LEN);
}

int Start_log_event::write_data(IO_CACHE* file)
{
  char buff[START_HEADER_LEN];
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy(buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  int4store(buff + ST_CREATED_OFFSET,created);
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

Query_log_event::Query_log_event(const char* buf, int event_len):
  Log_event(buf),data_buf(0), query(NULL), db(NULL)
{
  if ((uint)event_len < QUERY_EVENT_OVERHEAD)
    return;				
  ulong data_len;
  data_len = event_len - QUERY_EVENT_OVERHEAD;
 

  exec_time = uint4korr(buf + LOG_EVENT_HEADER_LEN + Q_EXEC_TIME_OFFSET);
  error_code = uint2korr(buf + LOG_EVENT_HEADER_LEN + Q_ERR_CODE_OFFSET);

  if (!(data_buf = (char*) my_malloc(data_len + 1, MYF(MY_WME))))
    return;

  memcpy(data_buf, buf + LOG_EVENT_HEADER_LEN + Q_DATA_OFFSET, data_len);
  thread_id = uint4korr(buf + LOG_EVENT_HEADER_LEN + Q_THREAD_ID_OFFSET);
  db = data_buf;
  db_len = (uint)buf[LOG_EVENT_HEADER_LEN + Q_DB_LEN_OFFSET];
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
      if(!(same_db = !memcmp(last_db, db, db_len + 1)))
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
  int4store(buf + Q_THREAD_ID_OFFSET, thread_id);
  int4store(buf + Q_EXEC_TIME_OFFSET, exec_time);
  buf[Q_DB_LEN_OFFSET] = (char)db_len;
  int2store(buf + Q_ERR_CODE_OFFSET, error_code);

  return (my_b_write(file, (byte*) buf, QUERY_HEADER_LEN) ||
	  my_b_write(file, (db) ? (byte*) db : (byte*)"", db_len + 1) ||
	  my_b_write(file, (byte*) query, q_len)) ? -1 : 0;
}

Intvar_log_event::Intvar_log_event(const char* buf):Log_event(buf)
{
  buf += LOG_EVENT_HEADER_LEN;
  type = buf[I_TYPE_OFFSET];
  val = uint8korr(buf+I_VAL_OFFSET);
}

const char* Intvar_log_event::get_var_type_name()
{
  switch(type)
  {
  case LAST_INSERT_ID_EVENT: return "LAST_INSERT_ID";
  case INSERT_ID_EVENT: return "INSERT_ID";
  default: /* impossible */ return "UNKNOWN";
  }
}

int Intvar_log_event::write_data(IO_CACHE* file)
{
  char buf[9];
  buf[I_TYPE_OFFSET] = type;
  int8store(buf + I_VAL_OFFSET, val);
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
  int4store(buf + L_THREAD_ID_OFFSET, thread_id);
  int4store(buf + L_EXEC_TIME_OFFSET, exec_time);
  int4store(buf + L_SKIP_LINES_OFFSET, skip_lines);
  buf[L_TBL_LEN_OFFSET] = (char)table_name_len;
  buf[L_DB_LEN_OFFSET] = (char)db_len;
  int4store(buf + L_NUM_FIELDS_OFFSET, num_fields);
  
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

Load_log_event::Load_log_event(const char* buf, int event_len):
  Log_event(buf),data_buf(0),num_fields(0),fields(0),
  field_lens(0),field_block_len(0),
  table_name(0),db(0),fname(0)
{
  uint data_len;
  if((uint)event_len < (LOAD_EVENT_OVERHEAD + LOG_EVENT_HEADER_LEN))
    return;
  memcpy(&sql_ex, buf + LOAD_HEADER_LEN + LOG_EVENT_HEADER_LEN,
	 sizeof(sql_ex));
  data_len = event_len - LOAD_HEADER_LEN - LOG_EVENT_HEADER_LEN -
    sizeof(sql_ex);
  if(!(data_buf = (char*)my_malloc(data_len + 1, MYF(MY_WME))))
    return;
  memcpy(data_buf, buf +LOG_EVENT_HEADER_LEN + LOAD_HEADER_LEN
	 + sizeof(sql_ex), data_len);
  copy_log_event(buf, data_len);
}

void Load_log_event::copy_log_event(const char *buf, ulong data_len)
{
  thread_id = uint4korr(buf + L_THREAD_ID_OFFSET + LOG_EVENT_HEADER_LEN);
  exec_time = uint4korr(buf + L_EXEC_TIME_OFFSET + LOG_EVENT_HEADER_LEN);
  skip_lines = uint4korr(buf + L_SKIP_LINES_OFFSET + LOG_EVENT_HEADER_LEN);
  table_name_len = (uint)buf[L_TBL_LEN_OFFSET + LOG_EVENT_HEADER_LEN];
  db_len = (uint)buf[L_DB_LEN_OFFSET + LOG_EVENT_HEADER_LEN];
  num_fields = uint4korr(buf + L_NUM_FIELDS_OFFSET + LOG_EVENT_HEADER_LEN);
	  
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
      if(!(same_db = !memcmp(last_db, db, db_len + 1)))
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
    fprintf(file, " IGNORE %ld LINES ", (long) skip_lines);

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

void Log_event::set_log_seq(THD* thd, MYSQL_LOG* log)
 {
   log_seq = (thd && thd->log_seq) ? thd->log_seq++ : log->log_seq++;
 }


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

Slave_log_event::Slave_log_event(THD* thd_arg,MASTER_INFO* mi):
  Log_event(thd_arg->start_time, 0, 1, thd_arg->server_id),
  mem_pool(0),master_host(0)
{
  if(!mi->inited)
    return;
  pthread_mutex_lock(&mi->lock);
  master_host_len = strlen(mi->host);
  master_log_len = strlen(mi->log_file_name);
  // on OOM, just do not initialize the structure and print the error
  if((mem_pool = (char*)my_malloc(get_data_size() + 1,
				  MYF(MY_WME))))
  {
    master_host = mem_pool + SL_MASTER_HOST_OFFSET ;
    memcpy(master_host, mi->host, master_host_len + 1);
    master_log = master_host + master_host_len + 1;
    memcpy(master_log, mi->log_file_name, master_log_len + 1);
    master_port = mi->port;
    master_pos = mi->pos;
  }
  else
    sql_print_error("Out of memory while recording slave event");
  pthread_mutex_unlock(&mi->lock);
}


#endif


Slave_log_event::~Slave_log_event()
{
  my_free(mem_pool, MYF(MY_ALLOW_ZERO_PTR));
}

void Slave_log_event::print(FILE* file, bool short_form = 0,
			    char* last_db = 0)
{
  char llbuff[22];
  if(short_form)
    return;
  print_header(file);
  fputc('\n', file);
  fprintf(file, "Slave: master_host='%s' master_port=%d \
 master_log=%s master_pos=%s\n", master_host, master_port, master_log,
	  llstr(master_pos, llbuff));
}

int Slave_log_event::get_data_size()
{
  return master_host_len + master_log_len + 1 + SL_MASTER_HOST_OFFSET;
}

int Slave_log_event::write_data(IO_CACHE* file)
{
  int8store(mem_pool + SL_MASTER_POS_OFFSET, master_pos);
  int2store(mem_pool + SL_MASTER_PORT_OFFSET, master_port);
  // log and host are already there
  return my_b_write(file, (byte*)mem_pool, get_data_size());
}

void Slave_log_event::init_from_mem_pool(int data_size)
{
  master_pos = uint8korr(mem_pool + SL_MASTER_POS_OFFSET);
  master_port = uint2korr(mem_pool + SL_MASTER_PORT_OFFSET);
  master_host = mem_pool + SL_MASTER_HOST_OFFSET;
  master_host_len = strlen(master_host);
  // safety
  master_log = master_host + master_host_len;
  if(master_log >= mem_pool + data_size)
  {
    master_host = 0;
    return;
  }

  master_log_len = strlen(master_log);
}

Slave_log_event::Slave_log_event(const char* buf, int event_len):
  Log_event(buf),mem_pool(0),master_host(0)
{
  if(!(mem_pool = (char*)my_malloc(event_len + 1, MYF(MY_WME))))
    return;
  memcpy(mem_pool, buf, event_len);
  mem_pool[event_len] = 0;
  init_from_mem_pool(event_len);
}
