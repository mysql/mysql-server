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

#ifdef MYSQL_CLIENT
static void pretty_print_str(FILE* file, char* str, int len)
{
  char* end = str + len;
  fputc('\'', file);
  while (str < end)
  {
    switch ((c=*str++)) {
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
  }
  fputc('\'', file);
}
#endif

#ifndef MYSQL_CLIENT


static void pretty_print_str(String* packet, char* str, int len)
{
  char* end = str + len;
  packet->append('\'');
  while (str < end)
  {
    char c;
    switch((c=*str++)) {
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
  }
  packet->append('\'');
}

static inline char* slave_load_file_stem(char*buf, uint file_id,
					 int event_server_id)
{
  fn_format(buf,"SQL_LOAD-",slave_load_tmpdir,"",4+32);
  buf = strend(buf);
  buf = int10_to_str(::server_id, buf, 10);
  *buf++ = '-';
  buf = int10_to_str(event_server_id, buf, 10);
  *buf++ = '-';
  return int10_to_str(file_id, buf, 10);
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
  case NEW_LOAD_EVENT:   return "New_load";
  case SLAVE_EVENT:  return "Slave";
  case CREATE_FILE_EVENT: return "Create_file";
  case APPEND_BLOCK_EVENT: return "Append_block";
  case DELETE_FILE_EVENT: return "Delete_file";
  case EXEC_LOAD_EVENT: return "Exec_load";
  default: /* impossible */ return "Unknown";
  }
}

#ifndef MYSQL_CLIENT
Log_event::Log_event(THD* thd_arg, uint16 flags_arg):
    exec_time(0),
    flags(flags_arg),cached_event_len(0),
    temp_buf(0),thd(thd_arg)
{
  if (thd)
  {
    server_id = thd->server_id;
    log_seq = thd->log_seq;
    when = thd->start_time;
  }
  else
  {
    server_id = ::server_id;
    log_seq = 0;
    when = time(NULL);
  }
}
#endif

Log_event::Log_event(const char* buf):cached_event_len(0),temp_buf(0)
{
  when = uint4korr(buf);
  server_id = uint4korr(buf + SERVER_ID_OFFSET);
  log_seq = uint4korr(buf + LOG_SEQ_OFFSET);
  flags = uint2korr(buf + FLAGS_OFFSET);
#ifndef MYSQL_CLIENT
  thd = 0;
#endif  
}


#ifndef MYSQL_CLIENT

int Log_event::exec_event(struct st_master_info* mi)
{
  if (mi)
  {
    thd->log_seq = 0;
    mi->inc_pos(get_event_len(), log_seq);
    flush_master_info(mi);
  }
  return 0;
}

void Log_event::pack_info(String* packet)
{
  net_store_data(packet, "", 0);
}

void Query_log_event::pack_info(String* packet)
{
  char buf[256];
  String tmp(buf, sizeof(buf));
  tmp.length(0);
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
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];

  tmp.append("Server ver: ");
  tmp.append(server_version);
  tmp.append(", Binlog ver: ");
  tmp.append(llstr(binlog_version, buf));
  net_store_data(packet, tmp.ptr(), tmp.length());
}

void Load_log_event::pack_info(String* packet)
{
  char buf[256];
  String tmp(buf, sizeof(buf));
  tmp.length(0);
  if(db && db_len)
  {
   tmp.append("use ");
   tmp.append(db, db_len);
   tmp.append("; ", 2);
  }

  tmp.append("LOAD DATA INFILE '");
  tmp.append(fname, fname_len);
  tmp.append("' ", 2);
  if(sql_ex.opt_flags && REPLACE_FLAG )
    tmp.append(" REPLACE ");
  else if(sql_ex.opt_flags && IGNORE_FLAG )
    tmp.append(" IGNORE ");
  
  tmp.append("INTO TABLE ");
  tmp.append(table_name);
  if (sql_ex.field_term_len)
  {
    tmp.append(" FIELDS TERMINATED BY ");
    pretty_print_str(&tmp, sql_ex.field_term, sql_ex.field_term_len);
  }

  if (sql_ex.enclosed_len)
  {
    if (sql_ex.opt_flags && OPT_ENCLOSED_FLAG )
      tmp.append(" OPTIONALLY ");
    tmp.append( " ENCLOSED BY ");
    pretty_print_str(&tmp, sql_ex.enclosed, sql_ex.enclosed_len);
  }
     
  if (sql_ex.escaped_len)
  {
    tmp.append( " ESCAPED BY ");
    pretty_print_str(&tmp, sql_ex.escaped, sql_ex.escaped_len);
  }
     
  if (sql_ex.line_term_len)
  {
    tmp.append(" LINES TERMINATED BY ");
    pretty_print_str(&tmp, sql_ex.line_term, sql_ex.line_term_len);
  }

  if (sql_ex.line_start_len)
  {
    tmp.append(" LINES STARTING BY ");
    pretty_print_str(&tmp, sql_ex.line_start, sql_ex.line_start_len);
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
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];
  tmp.append(new_log_ident, ident_len);
  tmp.append(";pos=");
  tmp.append(llstr(pos,buf));
  if(flags & LOG_EVENT_FORCED_ROTATE_F)
    tmp.append("; forced by master");
  net_store_data(packet, tmp.ptr(), tmp.length());
}

void Intvar_log_event::pack_info(String* packet)
{
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];
  tmp.append(get_var_type_name());
  tmp.append('=');
  tmp.append(llstr(val, buf));
  net_store_data(packet, tmp.ptr(), tmp.length());
}

void Slave_log_event::pack_info(String* packet)
{
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];
  tmp.append("host=");
  tmp.append(master_host);
  tmp.append(",port=");
  tmp.append(llstr(master_port,buf));
  tmp.append(",log=");
  tmp.append(master_log);
  tmp.append(",pos=");
  tmp.append(llstr(master_pos,buf));
  net_store_data(packet, tmp.ptr(), tmp.length());
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

#ifndef MYSQL_CLIENT
#define UNLOCK_MUTEX if(log_lock) pthread_mutex_unlock(log_lock);
#else
#define UNLOCK_MUTEX
#endif

// allocates memory - the caller is responsible for clean-up
#ifndef MYSQL_CLIENT
Log_event* Log_event::read_log_event(IO_CACHE* file, pthread_mutex_t* log_lock)
#else
Log_event* Log_event::read_log_event(IO_CACHE* file)
#endif  
{
  char head[LOG_EVENT_HEADER_LEN];
#ifndef MYSQL_CLIENT 
 if(log_lock) pthread_mutex_lock(log_lock);
#endif
  if (my_b_read(file, (byte *) head, sizeof(head)))
  {
    UNLOCK_MUTEX;
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

  // some events use the extra byte to null-terminate strings
  if (!(buf = my_malloc(data_len+1, MYF(MY_WME))))
  {
    error = "Out of memory";
    goto err;
  }
  buf[data_len] = 0;
  memcpy(buf, head, LOG_EVENT_HEADER_LEN);
  if(my_b_read(file, (byte*) buf + LOG_EVENT_HEADER_LEN,
	       data_len - LOG_EVENT_HEADER_LEN))
  {
    error = "read error";
    goto err;
  }
  if((res = read_log_event(buf, data_len)))
    res->register_temp_buf(buf);
err:
  UNLOCK_MUTEX;
  if(error)
  {
    sql_print_error(error);
    my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
  }
  return res;
}

Log_event* Log_event::read_log_event(const char* buf, int event_len)
{
  if(event_len < EVENT_LEN_OFFSET ||
     (uint)event_len != uint4korr(buf+EVENT_LEN_OFFSET))
    return NULL; // general sanity check - will fail on a partial read
  
  Log_event* ev = NULL;
  
  switch(buf[EVENT_TYPE_OFFSET])
  {
  case QUERY_EVENT:
    ev  = new Query_log_event(buf, event_len);
    break;
  case LOAD_EVENT:
  case NEW_LOAD_EVENT:
    ev = new Load_log_event(buf, event_len);
    break;
  case ROTATE_EVENT:
    ev = new Rotate_log_event(buf, event_len);
    break;
  case SLAVE_EVENT:
    ev = new Slave_log_event(buf, event_len);
    break;
  case CREATE_FILE_EVENT:
    ev = new Create_file_log_event(buf, event_len);
    break;
  case APPEND_BLOCK_EVENT:
    ev = new Append_block_log_event(buf, event_len);
    break;
  case DELETE_FILE_EVENT:
    ev = new Delete_file_log_event(buf, event_len);
    break;
  case EXEC_LOAD_EVENT:
    ev = new Execute_load_log_event(buf, event_len);
    break;
  case START_EVENT:
    ev = new Start_log_event(buf);
    break;
  case STOP_EVENT:
    ev = new Stop_log_event(buf);
    break;
  case INTVAR_EVENT:
    ev = new Intvar_log_event(buf);
    break;
  default:
    break;
  }
  if (!ev) return 0;
  if (!ev->is_valid())
  {
    delete ev;
    return 0;
  }
  ev->cached_event_len = event_len;
  return ev;  
}

#ifdef MYSQL_CLIENT
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
  char buf[22];
  if (short_form)
    return;

  print_header(file);
  fprintf(file, "\tRotate to ");
  if (new_log_ident)
    my_fwrite(file, (byte*) new_log_ident, (uint)ident_len, 
	      MYF(MY_NABP | MY_WME));
  fprintf(file, "pos=%s\n", llstr(pos, buf));
  fflush(file);
}

#endif /* #ifdef MYSQL_CLIENT */

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

  pos = uint8korr(buf + R_POS_OFFSET + LOG_EVENT_HEADER_LEN);
  ident_len = (uchar)(event_len - ROTATE_EVENT_OVERHEAD);
  if (!(new_log_ident = (char*) my_memdup((byte*) buf + R_IDENT_OFFSET
					  + LOG_EVENT_HEADER_LEN,
					  (uint) ident_len, MYF(MY_WME))))
    return;

  alloced = 1;
}

int Rotate_log_event::write_data(IO_CACHE* file)
{
  char buf[ROTATE_HEADER_LEN];
  int8store(buf, pos + R_POS_OFFSET);
  return my_b_write(file, (byte*)buf, ROTATE_HEADER_LEN) ||
    my_b_write(file, (byte*)new_log_ident, (uint) ident_len);
}

#ifndef MYSQL_CLIENT
Query_log_event::Query_log_event(THD* thd_arg, const char* query_arg,
				 bool using_trans):
    Log_event(thd_arg), data_buf(0), query(query_arg),  db(thd_arg->db),
    q_len(thd_arg->query_length),
    error_code(thd_arg->killed ? ER_SERVER_SHUTDOWN: thd_arg->net.last_errno),
    thread_id(thd_arg->thread_id), 
    cache_stmt(using_trans &&
	       (thd_arg->options & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN)))
  {
    time_t end_time;
    time(&end_time);
    exec_time = (ulong) (end_time  - thd->start_time);
    db_len = (db) ? (uint32) strlen(db) : 0;
  }
#endif

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

#ifdef MYSQL_CLIENT

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

#endif

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

#ifdef MYSQL_CLIENT
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
#endif

int Load_log_event::write_data_header(IO_CACHE* file)
{
  char buf[LOAD_HEADER_LEN];
  int4store(buf + L_THREAD_ID_OFFSET, thread_id);
  int4store(buf + L_EXEC_TIME_OFFSET, exec_time);
  int4store(buf + L_SKIP_LINES_OFFSET, skip_lines);
  buf[L_TBL_LEN_OFFSET] = (char)table_name_len;
  buf[L_DB_LEN_OFFSET] = (char)db_len;
  int4store(buf + L_NUM_FIELDS_OFFSET, num_fields);
  return my_b_write(file, (byte*)buf, LOAD_HEADER_LEN);
}

int Load_log_event::write_data_body(IO_CACHE* file)
{
  if (sql_ex.write_data(file)) return 1;
  if (num_fields && fields && field_lens)
  {
    if(my_b_write(file, (byte*)field_lens, num_fields) ||
       my_b_write(file, (byte*)fields, field_block_len))
      return 1;
  }
  return my_b_write(file, (byte*)table_name, table_name_len + 1) ||
     my_b_write(file, (byte*)db, db_len + 1) ||
    my_b_write(file, (byte*)fname, fname_len);
}

#define WRITE_STR(name) my_b_write(file,(byte*)&name ## _len, 1) || \
   my_b_write(file,(byte*)name,name ## _len)
#define OLD_EX_INIT(name) old_ex.##name = *name

int sql_ex_info::write_data(IO_CACHE* file)
{
  if (new_format())
  {
    return WRITE_STR(field_term) || WRITE_STR(enclosed) ||
      WRITE_STR(line_term) || WRITE_STR(line_start) ||
      WRITE_STR(escaped) || my_b_write(file,(byte*)&opt_flags,1);
  }
  else
  {
    old_sql_ex old_ex;
    OLD_EX_INIT(field_term);
    OLD_EX_INIT(enclosed);
    OLD_EX_INIT(line_term);
    OLD_EX_INIT(line_start);
    OLD_EX_INIT(escaped);
    old_ex.opt_flags = opt_flags;
    old_ex.empty_flags = empty_flags;
    return my_b_write(file,(byte*)&old_ex,sizeof(old_ex));
  }
}

#define READ_STR(name) name ## _len = *buf++;\
 if (buf >= buf_end) return 0;\
 name = buf; \
 buf += name ## _len; \
 if (buf >= buf_end) return 0;

#define READ_OLD_STR(name) name ## _len = 1; \
  name = buf++; \
  if (buf >= buf_end) return 0; 

#define FIX_OLD_LEN(name,NAME) if (empty_flags & NAME ## _EMPTY) \
  name ## _len = 0

char* sql_ex_info::init(char* buf,char* buf_end,bool use_new_format)
{
  cached_new_format = use_new_format;
  if (use_new_format)
  {
    READ_STR(field_term);
    READ_STR(enclosed);
    READ_STR(line_term);
    READ_STR(line_start);
    READ_STR(escaped);
    opt_flags = *buf++;
  }
  else
  {
    READ_OLD_STR(field_term);
    READ_OLD_STR(enclosed);
    READ_OLD_STR(line_term);
    READ_OLD_STR(line_start);
    READ_OLD_STR(escaped);
    opt_flags = *buf++;
    empty_flags = *buf++;
    FIX_OLD_LEN(field_term,FIELD_TERM);
    FIX_OLD_LEN(enclosed,ENCLOSED);
    FIX_OLD_LEN(line_term,LINE_TERM);
    FIX_OLD_LEN(line_start,LINE_START);
    FIX_OLD_LEN(escaped,ESCAPED);
  }
  return buf;
}


#ifndef MYSQL_CLIENT
Load_log_event::Load_log_event(THD* thd, sql_exchange* ex,
			       const char* db_arg, const char* table_name_arg,
		 List<Item>& fields_arg, enum enum_duplicates handle_dup):
    Log_event(thd),thread_id(thd->thread_id),
    num_fields(0),fields(0),field_lens(0),field_block_len(0),
    table_name(table_name_arg),
    db(db_arg),
    fname(ex->file_name)
  {
    time_t end_time;
    time(&end_time);
    exec_time = (ulong) (end_time  - thd->start_time);
    db_len = (db) ? (uint32) strlen(db) : 0;
    table_name_len = (table_name) ? (uint32) strlen(table_name) : 0;
    fname_len = (fname) ? (uint) strlen(fname) : 0;
    sql_ex.field_term = (char*)ex->field_term->ptr();
    sql_ex.field_term_len = ex->field_term->length();
    sql_ex.enclosed = (char*)ex->enclosed->ptr();
    sql_ex.enclosed_len = ex->enclosed->length();
    sql_ex.line_term = (char*)ex->line_term->ptr();
    sql_ex.line_term_len = ex->line_term->length();
    sql_ex.line_start = (char*)ex->line_start->ptr();
    sql_ex.line_start_len = ex->line_start->length();
    sql_ex.escaped = (char*)ex->escaped->ptr();
    sql_ex.escaped_len = ex->escaped->length();
    sql_ex.opt_flags = 0;
    sql_ex.cached_new_format = -1;
    
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

#endif

// the caller must do buf[event_len] = 0 before he starts using the
// constructed event
Load_log_event::Load_log_event(const char* buf, int event_len):
  Log_event(buf),num_fields(0),fields(0),
  field_lens(0),field_block_len(0),
  table_name(0),db(0),fname(0)
{
  if (!event_len) // derived class, will call copy_log_event() itself
    return;
  copy_log_event(buf, event_len);
}

int Load_log_event::copy_log_event(const char *buf, ulong event_len)
{
  uint data_len;
  char* buf_end = (char*)buf + event_len;
  thread_id = uint4korr(buf + L_THREAD_ID_OFFSET + LOG_EVENT_HEADER_LEN);
  exec_time = uint4korr(buf + L_EXEC_TIME_OFFSET + LOG_EVENT_HEADER_LEN);
  skip_lines = uint4korr(buf + L_SKIP_LINES_OFFSET + LOG_EVENT_HEADER_LEN);
  table_name_len = (uint)buf[L_TBL_LEN_OFFSET + LOG_EVENT_HEADER_LEN];
  db_len = (uint)buf[L_DB_LEN_OFFSET + LOG_EVENT_HEADER_LEN];
  num_fields = uint4korr(buf + L_NUM_FIELDS_OFFSET + LOG_EVENT_HEADER_LEN);
	  
  int body_offset = get_data_body_offset();
  if ((int)event_len < body_offset)
    return 1;
  //sql_ex.init() on success returns the pointer to the first byte after
  //the sql_ex structure, which is the start of field lengths array
  if (!(field_lens=(uchar*)sql_ex.init((char*)buf + body_offset,
		  buf_end,
		  buf[EVENT_TYPE_OFFSET] != LOAD_EVENT)))
    return 1;
  
  data_len = event_len - body_offset;
  if (num_fields > data_len) // simple sanity check against corruption
    return 1;
  uint i;
  for (i = 0; i < num_fields; i++)
  {
    field_block_len += (uint)field_lens[i] + 1;
  }
  fields = (char*)field_lens + num_fields;
  table_name  = fields + field_block_len;
  db = table_name + table_name_len + 1;
  fname = db + db_len + 1;
  int type_code = get_type_code();
  fname_len = strlen(fname);
  // null termination is accomplished by the caller doing buf[event_len]=0
  return 0;
}

#ifdef MYSQL_CLIENT

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

  fprintf(file, "LOAD DATA INFILE '%-*s' ", fname_len, fname);

  if(sql_ex.opt_flags && REPLACE_FLAG )
    fprintf(file," REPLACE ");
  else if(sql_ex.opt_flags && IGNORE_FLAG )
    fprintf(file," IGNORE ");
  
  fprintf(file, "INTO TABLE %s ", table_name);
  if(sql_ex.field_term)
  {
    fprintf(file, " FIELDS TERMINATED BY ");
    pretty_print_str(file, sql_ex.field_term, sql_ex.field_term_len);
  }

  if(sql_ex.enclosed)
  {
    if(sql_ex.opt_flags && OPT_ENCLOSED_FLAG )
      fprintf(file," OPTIONALLY ");
    fprintf(file, " ENCLOSED BY ");
    pretty_print_str(file, sql_ex.enclosed, sql_ex.enclosed_len);
  }
     
  if (sql_ex.escaped)
  {
    fprintf(file, " ESCAPED BY ");
    pretty_print_str(file, sql_ex.escaped, sql_ex.escaped_len);
  }
     
  if (sql_ex.line_term)
  {
    fprintf(file," LINES TERMINATED BY ");
    pretty_print_str(file, sql_ex.line_term, sql_ex.line_term_len);
  }

  if (sql_ex.line_start)
  {
    fprintf(file," LINES STARTING BY ");
    pretty_print_str(file, sql_ex.line_start, sql_ex.line_start_len);
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

#endif /* #ifdef MYSQL_CLIENT */

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

Slave_log_event::Slave_log_event(THD* thd_arg,struct st_master_info* mi):
  Log_event(thd_arg),mem_pool(0),master_host(0)
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

#ifdef MYSQL_CLIENT

void Slave_log_event::print(FILE* file, bool short_form, char* last_db)
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

#endif

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
  master_log = master_host + master_host_len + 1;
  if(master_log > mem_pool + data_size)
  {
    master_host = 0;
    return;
  }
  master_log_len = strlen(master_log);
}

Slave_log_event::Slave_log_event(const char* buf, int event_len):
  Log_event(buf),mem_pool(0),master_host(0)
{
  event_len -= LOG_EVENT_HEADER_LEN;
  if(event_len < 0)
    return;
  if(!(mem_pool = (char*)my_malloc(event_len + 1, MYF(MY_WME))))
    return;
  memcpy(mem_pool, buf + LOG_EVENT_HEADER_LEN, event_len);
  mem_pool[event_len] = 0;
  init_from_mem_pool(event_len);
}

#ifndef MYSQL_CLIENT
Create_file_log_event::Create_file_log_event(THD* thd_arg, sql_exchange* ex,
		 const char* db_arg, const char* table_name_arg,
		 List<Item>& fields_arg, enum enum_duplicates handle_dup,
			char* block_arg, uint block_len_arg):
  Load_log_event(thd_arg,ex,db_arg,table_name_arg,fields_arg,handle_dup),
 fake_base(0),block(block_arg),block_len(block_len_arg),
  file_id(thd_arg->file_id = mysql_bin_log.next_file_id())
{
  sql_ex.force_new_format();
}
#endif

int Create_file_log_event::write_data_body(IO_CACHE* file)
{
  int res;
  if ((res = Load_log_event::write_data_body(file)) || fake_base)
    return res;
  return my_b_write(file, "", 1) || my_b_write(file, block, block_len);
}

int Create_file_log_event::write_data_header(IO_CACHE* file)
{
  int res;
  if ((res = Load_log_event::write_data_header(file)) || fake_base)
    return res;
  char buf[CREATE_FILE_HEADER_LEN];
  int4store(buf + CF_FILE_ID_OFFSET, file_id);
  return my_b_write(file, buf, CREATE_FILE_HEADER_LEN);
}

int Create_file_log_event::write_base(IO_CACHE* file)
{
  int res;
  fake_base = 1; // pretend we are Load event
  res = write(file);
  fake_base = 0;
  return res;
}

Create_file_log_event::Create_file_log_event(const char* buf, int len):
  Load_log_event(buf,0),fake_base(0),block(0)
{
  int block_offset;
  if (copy_log_event(buf,len))
    return;
  file_id = uint4korr(buf + LOG_EVENT_HEADER_LEN +
		      + LOAD_HEADER_LEN + CF_FILE_ID_OFFSET);
  block_offset = LOG_EVENT_HEADER_LEN + Load_log_event::get_data_size() +
    CREATE_FILE_HEADER_LEN + 1; // 1 for \0 terminating fname  
  if (len < block_offset)
    return;
  block = (char*)buf + block_offset;
  block_len = len - block_offset;
}
#ifdef MYSQL_CLIENT
void Create_file_log_event::print(FILE* file, bool short_form,
				  char* last_db)
{
  if (short_form)
    return;
  Load_log_event::print(file, 1, last_db);
  fprintf(file, " file_id=%d, block_len=%d\n", file_id, block_len);
}
#endif

#ifndef MYSQL_CLIENT
void Create_file_log_event::pack_info(String* packet)
{
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];
  tmp.append("db=");
  tmp.append(db, db_len);
  tmp.append(";table=");
  tmp.append(table_name, table_name_len);
  tmp.append(";file_id=");
  tmp.append(llstr(file_id,buf));
  tmp.append(";block_len=");
  tmp.append(llstr(block_len,buf));
  net_store_data(packet, (char*)tmp.ptr(), tmp.length());
}
#endif  

#ifndef MYSQL_CLIENT  
Append_block_log_event::Append_block_log_event(THD* thd_arg, char* block_arg,
					       uint block_len_arg):
  Log_event(thd_arg), block(block_arg),block_len(block_len_arg),
  file_id(thd_arg->file_id)
{
}
#endif  
  
Append_block_log_event::Append_block_log_event(const char* buf, int len):
  Log_event(buf),block(0)
{
  if((uint)len < APPEND_BLOCK_EVENT_OVERHEAD)
    return;
  file_id = uint4korr(buf + LOG_EVENT_HEADER_LEN + AB_FILE_ID_OFFSET);
  block = (char*)buf + APPEND_BLOCK_EVENT_OVERHEAD;
  block_len = len - APPEND_BLOCK_EVENT_OVERHEAD;
}

int Append_block_log_event::write_data(IO_CACHE* file)
{
  char buf[APPEND_BLOCK_HEADER_LEN];
  int4store(buf + AB_FILE_ID_OFFSET, file_id);
  return my_b_write(file, buf, APPEND_BLOCK_HEADER_LEN) ||
    my_b_write(file, block, block_len);
}

#ifdef MYSQL_CLIENT  
void Append_block_log_event::print(FILE* file, bool short_form,
				   char* last_db)
{
  if (short_form)
    return;
  print_header(file);
  fputc('\n', file);
  fprintf(file, "#Append_block: file_id=%d, block_len=%d\n",
	  file_id, block_len);
}
#endif  
#ifndef MYSQL_CLIENT
void Append_block_log_event::pack_info(String* packet)
{
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];
  tmp.append(";file_id=");
  tmp.append(llstr(file_id,buf));
  tmp.append(";block_len=");
  tmp.append(llstr(block_len,buf));
  net_store_data(packet, (char*)tmp.ptr(), tmp.length());
}
#endif  

#ifndef MYSQL_CLIENT  
Delete_file_log_event::Delete_file_log_event(THD* thd_arg):
  Log_event(thd_arg),file_id(thd_arg->file_id)
{
}
#endif  

Delete_file_log_event::Delete_file_log_event(const char* buf, int len):
  Log_event(buf),file_id(0)
{
  if((uint)len < DELETE_FILE_EVENT_OVERHEAD)
    return;
  file_id = uint4korr(buf + LOG_EVENT_HEADER_LEN + AB_FILE_ID_OFFSET);
}

int Delete_file_log_event::write_data(IO_CACHE* file)
{
 char buf[DELETE_FILE_HEADER_LEN];
 int4store(buf + DF_FILE_ID_OFFSET, file_id);
 return my_b_write(file, buf, DELETE_FILE_HEADER_LEN);
}

#ifdef MYSQL_CLIENT  
void Delete_file_log_event::print(FILE* file, bool short_form,
				  char* last_db)
{
  if (short_form)
    return;
  print_header(file);
  fputc('\n', file);
  fprintf(file, "#Delete_file: file_id=%d\n",
	  file_id);
}
#endif  
#ifndef MYSQL_CLIENT
void Delete_file_log_event::pack_info(String* packet)
{
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];
  tmp.append(";file_id=");
  tmp.append(llstr(file_id,buf));
  net_store_data(packet, (char*)tmp.ptr(), tmp.length());
}
#endif  

#ifndef MYSQL_CLIENT  
Execute_load_log_event::Execute_load_log_event(THD* thd_arg):
  Log_event(thd_arg),file_id(thd_arg->file_id)
{
}
#endif  
  
Execute_load_log_event::Execute_load_log_event(const char* buf,int len):
  Log_event(buf),file_id(0)
{
  if((uint)len < EXEC_LOAD_EVENT_OVERHEAD)
    return;
  file_id = uint4korr(buf + LOG_EVENT_HEADER_LEN + EL_FILE_ID_OFFSET);
}

int Execute_load_log_event::write_data(IO_CACHE* file)
{
 char buf[EXEC_LOAD_HEADER_LEN];
 int4store(buf + EL_FILE_ID_OFFSET, file_id);
 return my_b_write(file, buf, EXEC_LOAD_HEADER_LEN);
}

#ifdef MYSQL_CLIENT  
void Execute_load_log_event::print(FILE* file, bool short_form,
				   char* last_db)
{
  if (short_form)
    return;
  print_header(file);
  fputc('\n', file);
  fprintf(file, "#Exec_load: file_id=%d\n",
	  file_id);
}
#endif  
#ifndef MYSQL_CLIENT
void Execute_load_log_event::pack_info(String* packet)
{
  char buf1[256];
  String tmp(buf1, sizeof(buf1));
  tmp.length(0);
  char buf[22];
  tmp.append(";file_id=");
  tmp.append(llstr(file_id,buf));
  net_store_data(packet, (char*)tmp.ptr(), tmp.length());
}
#endif

#ifndef MYSQL_CLIENT
int Query_log_event::exec_event(struct st_master_info* mi)
{
  int expected_error,actual_error = 0;
  init_sql_alloc(&thd->mem_root, 8192,0);
  thd->db = rewrite_db((char*)db);
  if (db_ok(thd->db, replicate_do_db, replicate_ignore_db))
  {
    thd->query = (char*)query;
    thd->set_time((time_t)when);
    thd->current_tablenr = 0;
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    thd->query_id = query_id++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    thd->query_error = 0;			// clear error
    thd->net.last_errno = 0;
    thd->net.last_error[0] = 0;
    thd->slave_proxy_id = thread_id;	// for temp tables
	
    // sanity check to make sure the master did not get a really bad
    // error on the query
    if (!check_expected_error(thd, (expected_error = error_code)))
    {
      mysql_parse(thd, thd->query, q_len);
      if (expected_error !=
	  (actual_error = thd->net.last_errno) && expected_error)
      {
	const char* errmsg = "Slave: did not get the expected error\
 running query from master - expected: '%s'(%d), got '%s'(%d)"; 
	sql_print_error(errmsg, ER_SAFE(expected_error),
			expected_error,
			actual_error ? thd->net.last_error:"no error",
			actual_error);
	thd->query_error = 1;
      }
      else if (expected_error == actual_error)
      {
	thd->query_error = 0;
	*last_slave_error = 0;
	last_slave_errno = 0;
      }
    }
    else
    {
      // master could be inconsistent, abort and tell DBA to check/fix it
      thd->db = thd->query = 0;
      thd->convert_set = 0;
      close_thread_tables(thd);
      free_root(&thd->mem_root,0);
      return 1;
    }
  }
  thd->db = 0;				// prevent db from being freed
  thd->query = 0;				// just to be sure
  // assume no convert for next query unless set explictly
  thd->convert_set = 0;
  close_thread_tables(thd);
      
  if (thd->query_error || thd->fatal_error)
  {
    slave_print_error(actual_error, "error '%s' on query '%s'",
		actual_error ? thd->net.last_error :
		"unexpected success or fatal error", query);
    free_root(&thd->mem_root,0);
    return 1;
  }
  free_root(&thd->mem_root,0);
  return Log_event::exec_event(mi); 
}

int Load_log_event::exec_event(NET* net, struct st_master_info* mi)
{
  init_sql_alloc(&thd->mem_root, 8192,0);
  thd->db = rewrite_db((char*)db);
  thd->query = 0;
  thd->query_error = 0;
	    
  if(db_ok(thd->db, replicate_do_db, replicate_ignore_db))
  {
    thd->set_time((time_t)when);
    thd->current_tablenr = 0;
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    thd->query_id = query_id++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));

    TABLE_LIST tables;
    bzero((char*) &tables,sizeof(tables));
    tables.db = thd->db;
    tables.name = tables.real_name = (char*)table_name;
    tables.lock_type = TL_WRITE;
    // the table will be opened in mysql_load    
    if(table_rules_on && !tables_ok(thd, &tables))
    {
      if (net)
        skip_load_data_infile(net);
    }
    else
    {
      char llbuff[22];
      enum enum_duplicates handle_dup = DUP_IGNORE;
      if(sql_ex.opt_flags && REPLACE_FLAG)
	handle_dup = DUP_REPLACE;
      sql_exchange ex((char*)fname, sql_ex.opt_flags &&
		      DUMPFILE_FLAG );
      
#define SET_EX(name) String name(sql_ex.name,sql_ex.name ## _len);\
 ex.name = &name;

      SET_EX(field_term);
      SET_EX(enclosed);
      SET_EX(line_term);
      SET_EX(line_start);
      SET_EX(escaped);
	    
      ex.opt_enclosed = (sql_ex.opt_flags & OPT_ENCLOSED_FLAG);
      if(sql_ex.empty_flags & FIELD_TERM_EMPTY)
	ex.field_term->length(0);
	    
      ex.skip_lines = skip_lines;
      List<Item> fields;
      set_fields(fields);
      thd->slave_proxy_id = thd->thread_id;
      if (net)
      {
	// mysql_load will use thd->net to read the file
	thd->net.vio = net->vio;
	// make sure the client does not get confused
	// about the packet sequence
	thd->net.pkt_nr = net->pkt_nr;
      }
      if(mysql_load(thd, &ex, &tables, fields, handle_dup, net != 0,
		    TL_WRITE))
	thd->query_error = 1;
      if(thd->cuted_fields)
	sql_print_error("Slave: load data infile at position %s in log \
'%s' produced %d warning(s)", llstr(mi->pos,llbuff), RPL_LOG_NAME,
			thd->cuted_fields );
      if(net)
        net->pkt_nr = thd->net.pkt_nr;
    }
  }
  else
  {
    // we will just ask the master to send us /dev/null if we do not
    // want to load the data
    if (net)
      skip_load_data_infile(net);
  }
	    
  thd->net.vio = 0; 
  thd->db = 0;// prevent db from being freed
  close_thread_tables(thd);
  if(thd->query_error)
  {
    int sql_error = thd->net.last_errno;
    if(!sql_error)
      sql_error = ER_UNKNOWN_ERROR;
		
    slave_print_error(sql_error, "Slave: Error '%s' running load data infile ",
		    ER_SAFE(sql_error));
    free_root(&thd->mem_root,0);
    return 1;
  }
  free_root(&thd->mem_root,0);
	    
  if(thd->fatal_error)
  {
    sql_print_error("Slave: Fatal error running LOAD DATA INFILE ");
    return 1;
  }

  return Log_event::exec_event(mi); 
}

int Start_log_event::exec_event(struct st_master_info* mi)
{
  close_temporary_tables(thd);
  return Log_event::exec_event(mi);
}

int Stop_log_event::exec_event(struct st_master_info* mi)
{
  if(mi->pos > 4) // stop event should be ignored after rotate event
  {
    close_temporary_tables(thd);
    mi->inc_pos(get_event_len(), log_seq);
    flush_master_info(mi);
  }
  thd->log_seq = 0;
  return 0;
}

int Rotate_log_event::exec_event(struct st_master_info* mi)
{
  bool rotate_binlog = 0, write_slave_event = 0;
  char* log_name = mi->log_file_name;
  pthread_mutex_lock(&mi->lock);
      
  // rotate local binlog only if the name of remote has changed
  if (!*log_name || !(log_name[ident_len] == 0 &&
		      !memcmp(log_name, new_log_ident, ident_len)))
  {
    write_slave_event = (!(flags & LOG_EVENT_FORCED_ROTATE_F)
			 && mysql_bin_log.is_open());
    rotate_binlog = (*log_name && write_slave_event);
    memcpy(log_name, new_log_ident,ident_len );
    log_name[ident_len] = 0;
  }
  mi->pos = pos; 
  mi->last_log_seq = log_seq;
#ifndef DBUG_OFF
  if (abort_slave_event_count)
    ++events_till_abort;
#endif
  if (rotate_binlog)
  {
    mysql_bin_log.new_file();
    mi->last_log_seq = 0;
  }
  pthread_cond_broadcast(&mi->cond);
  pthread_mutex_unlock(&mi->lock);
  flush_master_info(mi);
      
  if (write_slave_event)
  {
    Slave_log_event s(thd, mi);
    if (s.master_host)
    {
      s.set_log_seq(0, &mysql_bin_log);
      s.server_id = ::server_id;
      mysql_bin_log.write(&s);
    }
  }
  thd->log_seq = 0;
  return 0;
}

int Intvar_log_event::exec_event(struct st_master_info* mi)
{
  switch(type)
  {
  case LAST_INSERT_ID_EVENT:
    thd->last_insert_id_used = 1;
    thd->last_insert_id = val;
    break;
  case INSERT_ID_EVENT:
    thd->next_insert_id = val;
    break;
  }
  mi->inc_pending(get_event_len());
  return 0;
}

int Slave_log_event::exec_event(struct st_master_info* mi)
{
  if(mysql_bin_log.is_open())
    mysql_bin_log.write(this);
  return Log_event::exec_event(mi);
}

int Create_file_log_event::exec_event(struct st_master_info* mi)
{
  char fname_buf[FN_REFLEN+10];
  char* p,*p1;
  int fd = -1;
  IO_CACHE file;
  int error = 1;
  p = slave_load_file_stem(fname_buf, file_id, server_id);
  memcpy(p, ".info", 6);
  bzero((char*)&file, sizeof(file));
  if ((fd = my_open(fname_buf, O_WRONLY|O_CREAT|O_BINARY|O_TRUNC,
		    MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, WRITE_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    slave_print_error(my_errno, "Could not open file '%s'", fname_buf);
    goto err;
  }
  
  // a trick to avoid allocating another buffer
  memcpy(p, ".data", 6);
  fname = fname_buf;
  fname_len = (uint)(p-fname) + 5;
  if (write_base(&file))
  {
    memcpy(p, ".info", 6); // to have it right in the error message
    slave_print_error(my_errno, "Could not write to file '%s'", fname_buf);
    goto err;
  }
  end_io_cache(&file);
  my_close(fd, MYF(0));
  
  // fname_buf now already has .data, not .info, because we did our trick
  if ((fd = my_open(fname_buf, O_WRONLY|O_CREAT|O_BINARY|O_TRUNC,
		    MYF(MY_WME))) < 0)
  {
    slave_print_error(my_errno, "Could not open file '%s'", fname_buf);
    goto err;
  }
  if (my_write(fd, block, block_len, MYF(MY_WME+MY_NABP)))
  {
    slave_print_error(my_errno, "Write to '%s' failed", fname_buf);
    goto err;
  }
  if (mysql_bin_log.is_open())
    mysql_bin_log.write(this);
  error=0;
err:
  if (error)
    end_io_cache(&file);
  if (fd >= 0)
    my_close(fd, MYF(0));
  return error ? 1 : Log_event::exec_event(mi);
}

int Delete_file_log_event::exec_event(struct st_master_info* mi)
{
  char fname[FN_REFLEN+10];
  char* p;
  p = slave_load_file_stem(fname, file_id, server_id);
  memcpy(p, ".data", 6);
  (void)my_delete(fname, MYF(MY_WME));
  memcpy(p, ".info", 6);
  (void)my_delete(fname, MYF(MY_WME));
  if (mysql_bin_log.is_open())
    mysql_bin_log.write(this);
  return Log_event::exec_event(mi);
}

int Append_block_log_event::exec_event(struct st_master_info* mi)
{
  char fname[FN_REFLEN+10];
  char* p;
  int fd = -1;
  int error = 1;
  p = slave_load_file_stem(fname, file_id, server_id);
  memcpy(p, ".data", 6);
  if ((fd = my_open(fname, O_WRONLY|O_APPEND|O_BINARY, MYF(MY_WME))) < 0)
  {
    slave_print_error(my_errno, "Could not open file '%s'", fname);
    goto err;
  }
  if (my_write(fd, block, block_len, MYF(MY_WME+MY_NABP)))
  {
    slave_print_error(my_errno, "Write to '%s' failed", fname);
    goto err;
  }
  if (mysql_bin_log.is_open())
    mysql_bin_log.write(this);
  error=0;
err:
  if (fd >= 0)
    my_close(fd, MYF(0));
  return error ? error : Log_event::exec_event(mi);
}

int Execute_load_log_event::exec_event(struct st_master_info* mi)
{
  char fname[FN_REFLEN+10];
  char* p;
  int fd = -1;
  int error = 1;
  int save_options;
  IO_CACHE file;
  Load_log_event* lev = 0;
  p = slave_load_file_stem(fname, file_id, server_id);
  memcpy(p, ".info", 6);
  bzero((char*)&file, sizeof(file));
  if ((fd = my_open(fname, O_RDONLY|O_BINARY, MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, READ_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    slave_print_error(my_errno, "Could not open file '%s'", fname);
    goto err;
  }
  if (!(lev = (Load_log_event*)Log_event::read_log_event(&file,0))
      || lev->get_type_code() != NEW_LOAD_EVENT)
  {
    slave_print_error(0, "File '%s' appears corrupted", fname);
    goto err;
  }
  // we want to disable binary logging in slave thread
  // because we need the file events to appear in the same order
  // as they do on the master relative to other events, so that we
  // can preserve ascending order of log sequence numbers - needed
  // to handle failover 
  save_options = thd->options;
  thd->options &= ~ (ulong) OPTION_BIN_LOG;
  lev->thd = thd;
  if (lev->exec_event(0,0))
  {
    slave_print_error(my_errno, "Failed executing load from '%s'", fname);
    thd->options = save_options;
    goto err;
  }
  thd->options = save_options;
  (void)my_delete(fname, MYF(MY_WME));
  memcpy(p, ".data", 6);
  (void)my_delete(fname, MYF(MY_WME));
  if (mysql_bin_log.is_open())
    mysql_bin_log.write(this);
  error = 0;
err:
  delete lev;
  end_io_cache(&file);
  if (fd >= 0)
    my_close(fd, MYF(0));
  return error ? error : Log_event::exec_event(mi);
}


#endif







