/* Copyright (C) 2000-2004 MySQL AB

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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "slave.h"
#include "rpl_filter.h"
#include "rpl_utility.h"
#include <my_dir.h>
#endif /* MYSQL_CLIENT */
#include <base64.h>
#include <my_bitmap.h>

#define log_cs	&my_charset_latin1

/*
  pretty_print_str()
*/

#ifdef MYSQL_CLIENT
static void pretty_print_str(FILE* file, char* str, int len)
{
  char* end = str + len;
  fputc('\'', file);
  while (str < end)
  {
    char c;
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
#endif /* MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

static void clear_all_errors(THD *thd, struct st_relay_log_info *rli)
{
  thd->query_error = 0;
  thd->clear_error();
  *rli->last_slave_error = 0;
  rli->last_slave_errno = 0;
}


/*
  Ignore error code specified on command line
*/

inline int ignored_error_code(int err_code)
{
#ifdef HAVE_NDB_BINLOG
  /*
    The following error codes are hard-coded and will always be ignored.
  */
  switch (err_code)
  {
  case ER_DB_CREATE_EXISTS:
  case ER_DB_DROP_EXISTS:
    return 1;
  default:
    /* Nothing to do */
    break;
  }
#endif
  return ((err_code == ER_SLAVE_IGNORED_TABLE) ||
          (use_slave_mask && bitmap_is_set(&slave_error_mask, err_code)));
}
#endif


/*
  pretty_print_str()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
static char *pretty_print_str(char *packet, char *str, int len)
{
  char *end= str + len;
  char *pos= packet;
  *pos++= '\'';
  while (str < end)
  {
    char c;
    switch ((c=*str++)) {
    case '\n': *pos++= '\\'; *pos++= 'n'; break;
    case '\r': *pos++= '\\'; *pos++= 'r'; break;
    case '\\': *pos++= '\\'; *pos++= '\\'; break;
    case '\b': *pos++= '\\'; *pos++= 'b'; break;
    case '\t': *pos++= '\\'; *pos++= 't'; break;
    case '\'': *pos++= '\\'; *pos++= '\''; break;
    case 0   : *pos++= '\\'; *pos++= '0'; break;
    default:
      *pos++= c;
      break;
    }
  }
  *pos++= '\'';
  return pos;
}
#endif /* !MYSQL_CLIENT */


/*
  Creates a temporary name for load data infile:

  SYNOPSIS
    slave_load_file_stem()
    buf		      Store new filename here
    file_id	      File_id (part of file name)
    event_server_id   Event_id (part of file name)
    ext		      Extension for file name

  RETURN
    Pointer to start of extension
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
static char *slave_load_file_stem(char *buf, uint file_id,
                                  int event_server_id, const char *ext)
{
  char *res;
  fn_format(buf,"SQL_LOAD-",slave_load_tmpdir, "", MY_UNPACK_FILENAME);
  to_unix_path(buf);

  buf = strend(buf);
  buf = int10_to_str(::server_id, buf, 10);
  *buf++ = '-';
  buf = int10_to_str(event_server_id, buf, 10);
  *buf++ = '-';
  res= int10_to_str(file_id, buf, 10);
  strmov(res, ext);                             // Add extension last
  return res;                                   // Pointer to extension
}
#endif


/*
  Delete all temporary files used for SQL_LOAD.

  SYNOPSIS
    cleanup_load_tmpdir()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
static void cleanup_load_tmpdir()
{
  MY_DIR *dirp;
  FILEINFO *file;
  uint i;
  char fname[FN_REFLEN], prefbuf[31], *p;

  if (!(dirp=my_dir(slave_load_tmpdir,MYF(MY_WME))))
    return;

  /* 
     When we are deleting temporary files, we should only remove
     the files associated with the server id of our server.
     We don't use event_server_id here because since we've disabled
     direct binlogging of Create_file/Append_file/Exec_load events
     we cannot meet Start_log event in the middle of events from one 
     LOAD DATA.
  */
  p= strmake(prefbuf, STRING_WITH_LEN("SQL_LOAD-"));
  p= int10_to_str(::server_id, p, 10);
  *(p++)= '-';
  *p= 0;

  for (i=0 ; i < (uint)dirp->number_off_files; i++)
  {
    file=dirp->dir_entry+i;
    if (is_prefix(file->name, prefbuf))
    {
      fn_format(fname,file->name,slave_load_tmpdir,"",MY_UNPACK_FILENAME);
      my_delete(fname, MYF(0));
    }
  }

  my_dirend(dirp);
}
#endif


/*
  write_str()
*/

static bool write_str(IO_CACHE *file, char *str, uint length)
{
  byte tmp[1];
  tmp[0]= (byte) length;
  return (my_b_safe_write(file, tmp, sizeof(tmp)) ||
	  my_b_safe_write(file, (byte*) str, length));
}


/*
  read_str()
*/

static inline int read_str(char **buf, char *buf_end, char **str,
			   uint8 *len)
{
  if (*buf + ((uint) (uchar) **buf) >= buf_end)
    return 1;
  *len= (uint8) **buf;
  *str= (*buf)+1;
  (*buf)+= (uint) *len+1;
  return 0;
}


/*
  Transforms a string into "" or its expression in 0x... form.
*/

char *str_to_hex(char *to, const char *from, uint len)
{
  if (len)
  {
    *to++= '0';
    *to++= 'x';
    to= octet2hex(to, from, len);
  }
  else
    to= strmov(to, "\"\"");
  return to;                               // pointer to end 0 of 'to'
}

/*
  Append a version of the 'from' string suitable for use in a query to
  the 'to' string.  To generate a correct escaping, the character set
  information in 'csinfo' is used.
 */
#ifndef MYSQL_CLIENT
int
append_query_string(CHARSET_INFO *csinfo,
                    String const *from, String *to)
{
  char *beg, *ptr;
  uint32 const orig_len= to->length();
  if (to->reserve(orig_len + from->length()*2+3))
    return 1;

  beg= to->c_ptr_quick() + to->length();
  ptr= beg;
  if (csinfo->escape_with_backslash_is_dangerous)
    ptr= str_to_hex(ptr, from->ptr(), from->length());
  else
  {
    *ptr++= '\'';
    ptr+= escape_string_for_mysql(from->charset(), ptr, 0,
                                  from->ptr(), from->length());
    *ptr++='\'';
  }
  to->length(orig_len + ptr - beg);
  return 0;
}
#endif

/*
  Prints a "session_var=value" string. Used by mysqlbinlog to print some SET
  commands just before it prints a query.
*/

#ifdef MYSQL_CLIENT
static void print_set_option(FILE* file, uint32 bits_changed, uint32 option,
                             uint32 flags, const char* name, bool* need_comma) 
{
  if (bits_changed & option)
  {
    if (*need_comma)
      fprintf(file,", ");
    fprintf(file,"%s=%d", name, test(flags & option));
    *need_comma= 1;
  }
}
#endif

/**************************************************************************
	Log_event methods (= the parent class of all events)
**************************************************************************/

/*
  Log_event::get_type_str()
*/

const char* Log_event::get_type_str()
{
  switch(get_type_code()) {
  case START_EVENT_V3:  return "Start_v3";
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
  case RAND_EVENT: return "RAND";
  case XID_EVENT: return "Xid";
  case USER_VAR_EVENT: return "User var";
  case FORMAT_DESCRIPTION_EVENT: return "Format_desc";
  case TABLE_MAP_EVENT: return "Table_map";
  case WRITE_ROWS_EVENT: return "Write_rows";
  case UPDATE_ROWS_EVENT: return "Update_rows";
  case DELETE_ROWS_EVENT: return "Delete_rows";
  case BEGIN_LOAD_QUERY_EVENT: return "Begin_load_query";
  case EXECUTE_LOAD_QUERY_EVENT: return "Execute_load_query";
  default: return "Unknown";				/* impossible */
  }
}


/*
  Log_event::Log_event()
*/

#ifndef MYSQL_CLIENT
Log_event::Log_event(THD* thd_arg, uint16 flags_arg, bool using_trans)
  :log_pos(0), temp_buf(0), exec_time(0), flags(flags_arg), thd(thd_arg)
{
  server_id=	thd->server_id;
  when=		thd->start_time;
  cache_stmt=	using_trans;
}


/*
  This minimal constructor is for when you are not even sure that there
  is a valid THD. For example in the server when we are shutting down or
  flushing logs after receiving a SIGHUP (then we must write a Rotate to
  the binlog but we have no THD, so we need this minimal constructor).
*/

Log_event::Log_event()
  :temp_buf(0), exec_time(0), flags(0), cache_stmt(0),
   thd(0)
{
  server_id=	::server_id;
  when=		time(NULL);
  log_pos=	0;
}
#endif /* !MYSQL_CLIENT */


/*
  Log_event::Log_event()
*/

Log_event::Log_event(const char* buf,
                     const Format_description_log_event* description_event)
  :temp_buf(0), cache_stmt(0)
{
#ifndef MYSQL_CLIENT
  thd = 0;
#endif
  when = uint4korr(buf);
  server_id = uint4korr(buf + SERVER_ID_OFFSET);
  if (description_event->binlog_version==1)
  {
    log_pos= 0;
    flags= 0;
    return;
  }
  /* 4.0 or newer */
  log_pos= uint4korr(buf + LOG_POS_OFFSET);
  /*
    If the log is 4.0 (so here it can only be a 4.0 relay log read by
    the SQL thread or a 4.0 master binlog read by the I/O thread),
    log_pos is the beginning of the event: we transform it into the end
    of the event, which is more useful.
    But how do you know that the log is 4.0: you know it if
    description_event is version 3 *and* you are not reading a
    Format_desc (remember that mysqlbinlog starts by assuming that 5.0
    logs are in 4.0 format, until it finds a Format_desc).
  */
  if (description_event->binlog_version==3 &&
      buf[EVENT_TYPE_OFFSET]<FORMAT_DESCRIPTION_EVENT && log_pos)
  {
      /*
        If log_pos=0, don't change it. log_pos==0 is a marker to mean
        "don't change rli->group_master_log_pos" (see
        inc_group_relay_log_pos()). As it is unreal log_pos, adding the
        event len's is nonsense. For example, a fake Rotate event should
        not have its log_pos (which is 0) changed or it will modify
        Exec_master_log_pos in SHOW SLAVE STATUS, displaying a nonsense
        value of (a non-zero offset which does not exist in the master's
        binlog, so which will cause problems if the user uses this value
        in CHANGE MASTER).
      */
    log_pos+= uint4korr(buf + EVENT_LEN_OFFSET);
  }
  DBUG_PRINT("info", ("log_pos: %lu", (ulong) log_pos));

  flags= uint2korr(buf + FLAGS_OFFSET);
  if ((buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT) ||
      (buf[EVENT_TYPE_OFFSET] == ROTATE_EVENT))
  {
    /*
      These events always have a header which stops here (i.e. their
      header is FROZEN).
    */
    /*
      Initialization to zero of all other Log_event members as they're
      not specified. Currently there are no such members; in the future
      there will be an event UID (but Format_description and Rotate
      don't need this UID, as they are not propagated through
      --log-slave-updates (remember the UID is used to not play a query
      twice when you have two masters which are slaves of a 3rd master).
      Then we are done.
    */
    return;
  }
  /* otherwise, go on with reading the header from buf (nothing now) */
}

#ifndef MYSQL_CLIENT
#ifdef HAVE_REPLICATION

/*
  Log_event::exec_event()
*/

int Log_event::exec_event(struct st_relay_log_info* rli)
{
  DBUG_ENTER("Log_event::exec_event");

  /*
    rli is null when (as far as I (Guilhem) know)
    the caller is
    Load_log_event::exec_event *and* that one is called from
    Execute_load_log_event::exec_event. 
    In this case, we don't do anything here ;
    Execute_load_log_event::exec_event will call Log_event::exec_event
    again later with the proper rli.
    Strictly speaking, if we were sure that rli is null
    only in the case discussed above, 'if (rli)' is useless here.
    But as we are not 100% sure, keep it for now.
  */
  if (rli)
  {
    /*
      If in a transaction, and if the slave supports transactions, just
      inc_event_relay_log_pos(). We only have to check for OPTION_BEGIN
      (not OPTION_NOT_AUTOCOMMIT) as transactions are logged with
      BEGIN/COMMIT, not with SET AUTOCOMMIT= .

      CAUTION: opt_using_transactions means
      innodb || bdb ; suppose the master supports InnoDB and BDB,
      but the slave supports only BDB, problems
      will arise:
      - suppose an InnoDB table is created on the master,
      - then it will be MyISAM on the slave
      - but as opt_using_transactions is true, the slave will believe he
      is transactional with the MyISAM table. And problems will come
      when one does START SLAVE; STOP SLAVE; START SLAVE; (the slave
      will resume at BEGIN whereas there has not been any rollback).
      This is the problem of using opt_using_transactions instead of a
      finer "does the slave support
      _the_transactional_handler_used_on_the_master_".

      More generally, we'll have problems when a query mixes a
      transactional handler and MyISAM and STOP SLAVE is issued in the
      middle of the "transaction". START SLAVE will resume at BEGIN
      while the MyISAM table has already been updated.
    */
    if ((thd->options & OPTION_BEGIN) && opt_using_transactions)
      rli->inc_event_relay_log_pos();
    else
    {
      rli->inc_group_relay_log_pos(log_pos);
      flush_relay_log_info(rli);
      /* 
         Note that Rotate_log_event::exec_event() does not call this
         function, so there is no chance that a fake rotate event resets
         last_master_timestamp.
         Note that we update without mutex (probably ok - except in some very
         rare cases, only consequence is that value may take some time to
         display in Seconds_Behind_Master - not critical).
      */
      rli->last_master_timestamp= when;
    }
  }
  DBUG_RETURN(0);
}


/*
  Log_event::pack_info()
*/

void Log_event::pack_info(Protocol *protocol)
{
  protocol->store("", &my_charset_bin);
}


/*
  Log_event::net_send()

  Only called by SHOW BINLOG EVENTS
*/

int Log_event::net_send(Protocol *protocol, const char* log_name, my_off_t pos)
{
  const char *p= strrchr(log_name, FN_LIBCHAR);
  const char *event_type;
  if (p)
    log_name = p + 1;
  
  protocol->prepare_for_resend();
  protocol->store(log_name, &my_charset_bin);
  protocol->store((ulonglong) pos);
  event_type = get_type_str();
  protocol->store(event_type, strlen(event_type), &my_charset_bin);
  protocol->store((uint32) server_id);
  protocol->store((ulonglong) log_pos);
  pack_info(protocol);
  return protocol->write();
}
#endif /* HAVE_REPLICATION */


/*
  Log_event::init_show_field_list()
*/

void Log_event::init_show_field_list(List<Item>* field_list)
{
  field_list->push_back(new Item_empty_string("Log_name", 20));
  field_list->push_back(new Item_return_int("Pos", 11,
					    MYSQL_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Event_type", 20));
  field_list->push_back(new Item_return_int("Server_id", 10,
					    MYSQL_TYPE_LONG));
  field_list->push_back(new Item_return_int("End_log_pos", 11,
					    MYSQL_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Info", 20));
}


/*
  Log_event::write()
*/

bool Log_event::write_header(IO_CACHE* file, ulong event_data_length)
{
  byte header[LOG_EVENT_HEADER_LEN];
  DBUG_ENTER("Log_event::write_header");

  /* Store number of bytes that will be written by this event */
  data_written= event_data_length + sizeof(header);

  /*
    log_pos != 0 if this is relay-log event. In this case we should not
    change the position
  */

  if (is_artificial_event())
  {
    /*
      We should not do any cleanup on slave when reading this. We
      mark this by setting log_pos to 0.  Start_log_event_v3() will
      detect this on reading and set artificial_event=1 for the event.
    */
    log_pos= 0;
  }
  else  if (!log_pos)
  {
    /*
      Calculate position of end of event

      Note that with a SEQ_READ_APPEND cache, my_b_tell() does not
      work well.  So this will give slightly wrong positions for the
      Format_desc/Rotate/Stop events which the slave writes to its
      relay log. For example, the initial Format_desc will have
      end_log_pos=91 instead of 95. Because after writing the first 4
      bytes of the relay log, my_b_tell() still reports 0. Because
      my_b_append() does not update the counter which my_b_tell()
      later uses (one should probably use my_b_append_tell() to work
      around this).  To get right positions even when writing to the
      relay log, we use the (new) my_b_safe_tell().

      Note that this raises a question on the correctness of all these
      DBUG_ASSERT(my_b_tell()=rli->event_relay_log_pos).

      If in a transaction, the log_pos which we calculate below is not
      very good (because then my_b_safe_tell() returns start position
      of the BEGIN, so it's like the statement was at the BEGIN's
      place), but it's not a very serious problem (as the slave, when
      it is in a transaction, does not take those end_log_pos into
      account (as it calls inc_event_relay_log_pos()). To be fixed
      later, so that it looks less strange. But not bug.
    */

    log_pos= my_b_safe_tell(file)+data_written;
  }

  /*
    Header will be of size LOG_EVENT_HEADER_LEN for all events, except for
    FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT, where it will be
    LOG_EVENT_MINIMAL_HEADER_LEN (remember these 2 have a frozen header,
    because we read them before knowing the format).
  */

  int4store(header, (ulong) when);              // timestamp
  header[EVENT_TYPE_OFFSET]= get_type_code();
  int4store(header+ SERVER_ID_OFFSET, server_id);
  int4store(header+ EVENT_LEN_OFFSET, data_written);
  int4store(header+ LOG_POS_OFFSET, log_pos);
  int2store(header+ FLAGS_OFFSET, flags);

  DBUG_RETURN(my_b_safe_write(file, header, sizeof(header)) != 0);
}


/*
  Log_event::read_log_event()

  This needn't be format-tolerant, because we only read
  LOG_EVENT_MINIMAL_HEADER_LEN (we just want to read the event's length).

*/

int Log_event::read_log_event(IO_CACHE* file, String* packet,
			      pthread_mutex_t* log_lock)
{
  ulong data_len;
  int result=0;
  char buf[LOG_EVENT_MINIMAL_HEADER_LEN];
  DBUG_ENTER("read_log_event");

  if (log_lock)
    pthread_mutex_lock(log_lock);
  if (my_b_read(file, (byte*) buf, sizeof(buf)))
  {
    /*
      If the read hits eof, we must report it as eof so the caller
      will know it can go into cond_wait to be woken up on the next
      update to the log.
    */
    DBUG_PRINT("error",("file->error: %d", file->error));
    if (!file->error)
      result= LOG_READ_EOF;
    else
      result= (file->error > 0 ? LOG_READ_TRUNC : LOG_READ_IO);
    goto end;
  }
  data_len= uint4korr(buf + EVENT_LEN_OFFSET);
  if (data_len < LOG_EVENT_MINIMAL_HEADER_LEN ||
      data_len > current_thd->variables.max_allowed_packet)
  {
    DBUG_PRINT("error",("data_len: %ld", data_len));
    result= ((data_len < LOG_EVENT_MINIMAL_HEADER_LEN) ? LOG_READ_BOGUS :
	     LOG_READ_TOO_LARGE);
    goto end;
  }
  packet->append(buf, sizeof(buf));
  data_len-= LOG_EVENT_MINIMAL_HEADER_LEN;
  if (data_len)
  {
    if (packet->append(file, data_len))
    {
      /*
	Here if we hit EOF it's really an error: as data_len is >=0
        there's supposed to be more bytes available. 
	EOF means we are reading the event partially, which should
	never happen: either we read badly or the binlog is truncated.
      */
      result= file->error >= 0 ? LOG_READ_TRUNC: LOG_READ_IO;
      /* Implicit goto end; */
    }
  }

end:
  if (log_lock)
    pthread_mutex_unlock(log_lock);
  DBUG_RETURN(result);
}
#endif /* !MYSQL_CLIENT */

#ifndef MYSQL_CLIENT
#define UNLOCK_MUTEX if (log_lock) pthread_mutex_unlock(log_lock);
#define LOCK_MUTEX if (log_lock) pthread_mutex_lock(log_lock);
#else
#define UNLOCK_MUTEX
#define LOCK_MUTEX
#endif

/*
  Log_event::read_log_event()

  NOTE:
    Allocates memory;  The caller is responsible for clean-up.
*/

#ifndef MYSQL_CLIENT
Log_event* Log_event::read_log_event(IO_CACHE* file,
				     pthread_mutex_t* log_lock,
                                     const Format_description_log_event *description_event)
#else
Log_event* Log_event::read_log_event(IO_CACHE* file,
                                     const Format_description_log_event *description_event)
#endif
{
  DBUG_ENTER("Log_event::read_log_event(IO_CACHE *, Format_description_log_event *");
  DBUG_ASSERT(description_event != 0);
  char head[LOG_EVENT_MINIMAL_HEADER_LEN];
  /*
    First we only want to read at most LOG_EVENT_MINIMAL_HEADER_LEN, just to
    check the event for sanity and to know its length; no need to really parse
    it. We say "at most" because this could be a 3.23 master, which has header
    of 13 bytes, whereas LOG_EVENT_MINIMAL_HEADER_LEN is 19 bytes (it's
    "minimal" over the set {MySQL >=4.0}).
  */
  uint header_size= min(description_event->common_header_len,
                        LOG_EVENT_MINIMAL_HEADER_LEN);

  LOCK_MUTEX;
  DBUG_PRINT("info", ("my_b_tell=%lu", my_b_tell(file)));
  if (my_b_read(file, (byte *) head, header_size))
  {
    DBUG_PRINT("info", ("Log_event::read_log_event(IO_CACHE*,Format_desc*) \
failed my_b_read"));
    UNLOCK_MUTEX;
    /*
      No error here; it could be that we are at the file's end. However
      if the next my_b_read() fails (below), it will be an error as we
      were able to read the first bytes.
    */
    DBUG_RETURN(0);
  }
  uint data_len = uint4korr(head + EVENT_LEN_OFFSET);
  char *buf= 0;
  const char *error= 0;
  Log_event *res=  0;
#ifndef max_allowed_packet
  THD *thd=current_thd;
  uint max_allowed_packet= thd ? thd->variables.max_allowed_packet : ~(ulong)0;
#endif

  if (data_len > max_allowed_packet)
  {
    error = "Event too big";
    goto err;
  }

  if (data_len < header_size)
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
  memcpy(buf, head, header_size);
  if (my_b_read(file, (byte*) buf + header_size, data_len - header_size))
  {
    error = "read error";
    goto err;
  }
  if ((res= read_log_event(buf, data_len, &error, description_event)))
    res->register_temp_buf(buf);

err:
  UNLOCK_MUTEX;
  if (!res)
  {
    DBUG_ASSERT(error != 0);
    sql_print_error("Error in Log_event::read_log_event(): "
                    "'%s', data_len: %d, event_type: %d",
		    error,data_len,head[EVENT_TYPE_OFFSET]);
    my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
    /*
      The SQL slave thread will check if file->error<0 to know
      if there was an I/O error. Even if there is no "low-level" I/O errors
      with 'file', any of the high-level above errors is worrying
      enough to stop the SQL thread now ; as we are skipping the current event,
      going on with reading and successfully executing other events can
      only corrupt the slave's databases. So stop.
    */
    file->error= -1;
  }
  DBUG_RETURN(res);
}


/*
  Log_event::read_log_event()
  Binlog format tolerance is in (buf, event_len, description_event)
  constructors.
*/

Log_event* Log_event::read_log_event(const char* buf, uint event_len,
				     const char **error,
                                     const Format_description_log_event *description_event)
{
  Log_event* ev;
  DBUG_ENTER("Log_event::read_log_event(char*,...)");
  DBUG_ASSERT(description_event != 0);
  DBUG_PRINT("info", ("binlog_version: %d", description_event->binlog_version));
  if (event_len < EVENT_LEN_OFFSET ||
      (uint) event_len != uint4korr(buf+EVENT_LEN_OFFSET))
  {
    *error="Sanity check failed";		// Needed to free buffer
    DBUG_RETURN(NULL); // general sanity check - will fail on a partial read
  }

  /* To check the integrity of the Log_event_type enumeration */
  DBUG_ASSERT(buf[EVENT_TYPE_OFFSET] < ENUM_END_EVENT);

  switch(buf[EVENT_TYPE_OFFSET]) {
  case QUERY_EVENT:
    ev  = new Query_log_event(buf, event_len, description_event, QUERY_EVENT);
    break;
  case LOAD_EVENT:
    ev = new Load_log_event(buf, event_len, description_event);
    break;
  case NEW_LOAD_EVENT:
    ev = new Load_log_event(buf, event_len, description_event);
    break;
  case ROTATE_EVENT:
    ev = new Rotate_log_event(buf, event_len, description_event);
    break;
#ifdef HAVE_REPLICATION
  case SLAVE_EVENT: /* can never happen (unused event) */
    ev = new Slave_log_event(buf, event_len);
    break;
#endif /* HAVE_REPLICATION */
  case CREATE_FILE_EVENT:
    ev = new Create_file_log_event(buf, event_len, description_event);
    break;
  case APPEND_BLOCK_EVENT:
    ev = new Append_block_log_event(buf, event_len, description_event);
    break;
  case DELETE_FILE_EVENT:
    ev = new Delete_file_log_event(buf, event_len, description_event);
    break;
  case EXEC_LOAD_EVENT:
    ev = new Execute_load_log_event(buf, event_len, description_event);
    break;
  case START_EVENT_V3: /* this is sent only by MySQL <=4.x */
    ev = new Start_log_event_v3(buf, description_event);
    break;
  case STOP_EVENT:
    ev = new Stop_log_event(buf, description_event);
    break;
  case INTVAR_EVENT:
    ev = new Intvar_log_event(buf, description_event);
    break;
  case XID_EVENT:
    ev = new Xid_log_event(buf, description_event);
    break;
  case RAND_EVENT:
    ev = new Rand_log_event(buf, description_event);
    break;
  case USER_VAR_EVENT:
    ev = new User_var_log_event(buf, description_event);
    break;
  case FORMAT_DESCRIPTION_EVENT:
    ev = new Format_description_log_event(buf, event_len, description_event); 
    break;
#if defined(HAVE_REPLICATION) && defined(HAVE_ROW_BASED_REPLICATION)
  case WRITE_ROWS_EVENT:
    ev = new Write_rows_log_event(buf, event_len, description_event);
    break;
  case UPDATE_ROWS_EVENT:
    ev = new Update_rows_log_event(buf, event_len, description_event);
    break;
  case DELETE_ROWS_EVENT:
    ev = new Delete_rows_log_event(buf, event_len, description_event);
    break;
  case TABLE_MAP_EVENT:
    ev = new Table_map_log_event(buf, event_len, description_event);
    break;
#endif
  case BEGIN_LOAD_QUERY_EVENT:
    ev = new Begin_load_query_log_event(buf, event_len, description_event);
    break;
  case EXECUTE_LOAD_QUERY_EVENT:
    ev = new Execute_load_query_log_event(buf, event_len, description_event);
    break;
  default:
    DBUG_PRINT("error",("Unknown evernt code: %d",(int) buf[EVENT_TYPE_OFFSET]));
    ev= NULL;
    break;
  }

  /*
    is_valid() are small event-specific sanity tests which are
    important; for example there are some my_malloc() in constructors
    (e.g. Query_log_event::Query_log_event(char*...)); when these
    my_malloc() fail we can't return an error out of the constructor
    (because constructor is "void") ; so instead we leave the pointer we
    wanted to allocate (e.g. 'query') to 0 and we test it in is_valid().
    Same for Format_description_log_event, member 'post_header_len'.
  */
  if (!ev || !ev->is_valid())
  {
    DBUG_PRINT("error",("Found invalid event in binary log"));

    delete ev;
#ifdef MYSQL_CLIENT
    if (!force_opt) /* then mysqlbinlog dies */
    {
      *error= "Found invalid event in binary log";
      DBUG_RETURN(0);
    }
    ev= new Unknown_log_event(buf, description_event);
#else
    *error= "Found invalid event in binary log";
    DBUG_RETURN(0);
#endif
  }
  DBUG_RETURN(ev);  
}

#ifdef MYSQL_CLIENT

/*
  Log_event::print_header()
*/

void Log_event::print_header(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char llbuff[22];
  my_off_t hexdump_from= print_event_info->hexdump_from;

  fputc('#', file);
  print_timestamp(file);
  fprintf(file, " server id %d  end_log_pos %s ", server_id,
	  llstr(log_pos,llbuff));

  /* mysqlbinlog --hexdump */
  if (print_event_info->hexdump_from)
  {
    fprintf(file, "\n");
    uchar *ptr= (uchar*)temp_buf;
    my_off_t size=
      uint4korr(ptr + EVENT_LEN_OFFSET) - LOG_EVENT_MINIMAL_HEADER_LEN;
    my_off_t i;

    /* Header len * 4 >= header len * (2 chars + space + extra space) */
    char *h, hex_string[LOG_EVENT_MINIMAL_HEADER_LEN*4]= {0};
    char *c, char_string[16+1]= {0};

    /* Pretty-print event common header if header is exactly 19 bytes */
    if (print_event_info->common_header_len == LOG_EVENT_MINIMAL_HEADER_LEN)
    {
      fprintf(file, "# Position  Timestamp   Type   Master ID        "
	      "Size      Master Pos    Flags \n");
      fprintf(file, "# %8.8lx %02x %02x %02x %02x   %02x   "
	      "%02x %02x %02x %02x   %02x %02x %02x %02x   "
	      "%02x %02x %02x %02x   %02x %02x\n",
	      (unsigned long) hexdump_from,
              ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6],
              ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13],
              ptr[14], ptr[15], ptr[16], ptr[17], ptr[18]);
      ptr += LOG_EVENT_MINIMAL_HEADER_LEN;
      hexdump_from += LOG_EVENT_MINIMAL_HEADER_LEN;
    }

    /* Rest of event (without common header) */
    for (i= 0, c= char_string, h=hex_string;
	 i < size;
	 i++, ptr++)
    {
      my_snprintf(h, 4, "%02x ", *ptr);
      h += 3;

      *c++= my_isalnum(&my_charset_bin, *ptr) ? *ptr : '.';

      if (i % 16 == 15)
      {
	fprintf(file, "# %8.8lx %-48.48s |%16s|\n",
		(unsigned long) (hexdump_from + (i & 0xfffffff0)),
                hex_string, char_string);
	hex_string[0]= 0;
	char_string[0]= 0;
	c= char_string;
	h= hex_string;
      }
      else if (i % 8 == 7) *h++ = ' ';
    }
    *c= '\0';

    /* Non-full last line */
    if (hex_string[0])
      fprintf(file, "# %8.8lx %-48.48s |%s|\n# ",
	     (unsigned long) (hexdump_from + (i & 0xfffffff0)),
             hex_string, char_string);
  }
}


void Log_event::print_base64(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  uchar *ptr= (uchar*)temp_buf;
  my_off_t size= uint4korr(ptr + EVENT_LEN_OFFSET);

  char *tmp_str=
    (char *) my_malloc(base64_needed_encoded_length(size), MYF(MY_WME));
  if (!tmp_str) {
    fprintf(stderr, "\nError: Out of memory. "
            "Could not print correct binlog event.\n");
    return;
  }
  int res= base64_encode(ptr, size, tmp_str);
  fprintf(file, "\nBINLOG '\n%s\n';\n", tmp_str);
  my_free(tmp_str, MYF(0));
}


/*
  Log_event::print_timestamp()
*/

void Log_event::print_timestamp(FILE* file, time_t* ts)
{
  struct tm *res;
  if (!ts)
    ts = &when;
#ifdef MYSQL_SERVER				// This is always false
  struct tm tm_tmp;
  localtime_r(ts,(res= &tm_tmp));
#else
  res=localtime(ts);
#endif

  fprintf(file,"%02d%02d%02d %2d:%02d:%02d",
	  res->tm_year % 100,
	  res->tm_mon+1,
	  res->tm_mday,
	  res->tm_hour,
	  res->tm_min,
	  res->tm_sec);
}

#endif /* MYSQL_CLIENT */


/**************************************************************************
	Query_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/*
  Query_log_event::pack_info()
  This (which is used only for SHOW BINLOG EVENTS) could be updated to
  print SET @@session_var=. But this is not urgent, as SHOW BINLOG EVENTS is
  only an information, it does not produce suitable queries to replay (for
  example it does not print LOAD DATA INFILE).
*/

void Query_log_event::pack_info(Protocol *protocol)
{
  // TODO: show the catalog ??
  char *buf, *pos;
  if (!(buf= my_malloc(9 + db_len + q_len, MYF(MY_WME))))
    return;
  pos= buf;
  if (!(flags & LOG_EVENT_SUPPRESS_USE_F)
      && db && db_len)
  {
    pos= strmov(buf, "use `");
    memcpy(pos, db, db_len);
    pos= strmov(pos+db_len, "`; ");
  }
  if (query && q_len)
  {
    memcpy(pos, query, q_len);
    pos+= q_len;
  }
  protocol->store(buf, pos-buf, &my_charset_bin);
  my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
}
#endif

#ifndef MYSQL_CLIENT

/* Utility function for the next method */
static void write_str_with_code_and_len(char **dst, const char *src,
                                        int len, uint code)
{
  DBUG_ASSERT(src);
  *((*dst)++)= code;
  *((*dst)++)= (uchar) len;
  bmove(*dst, src, len);
  (*dst)+= len;
}


/*
  Query_log_event::write()

  NOTES:
    In this event we have to modify the header to have the correct
    EVENT_LEN_OFFSET as we don't yet know how many status variables we
    will print!
*/

bool Query_log_event::write(IO_CACHE* file)
{
  uchar buf[QUERY_HEADER_LEN+
            1+4+           // code of flags2 and flags2
            1+8+           // code of sql_mode and sql_mode
            1+1+FN_REFLEN+ // code of catalog and catalog length and catalog
            1+4+           // code of autoinc and the 2 autoinc variables
            1+6+           // code of charset and charset
            1+1+MAX_TIME_ZONE_NAME_LENGTH // code of tz and tz length and tz name
            ], *start, *start_of_status;
  ulong event_length;

  if (!query)
    return 1;                                   // Something wrong with event

  /*
    We want to store the thread id:
    (- as an information for the user when he reads the binlog)
    - if the query uses temporary table: for the slave SQL thread to know to
    which master connection the temp table belongs.
    Now imagine we (write()) are called by the slave SQL thread (we are
    logging a query executed by this thread; the slave runs with
    --log-slave-updates). Then this query will be logged with
    thread_id=the_thread_id_of_the_SQL_thread. Imagine that 2 temp tables of
    the same name were created simultaneously on the master (in the master
    binlog you have
    CREATE TEMPORARY TABLE t; (thread 1)
    CREATE TEMPORARY TABLE t; (thread 2)
    ...)
    then in the slave's binlog there will be
    CREATE TEMPORARY TABLE t; (thread_id_of_the_slave_SQL_thread)
    CREATE TEMPORARY TABLE t; (thread_id_of_the_slave_SQL_thread)
    which is bad (same thread id!).

    To avoid this, we log the thread's thread id EXCEPT for the SQL
    slave thread for which we log the original (master's) thread id.
    Now this moves the bug: what happens if the thread id on the
    master was 10 and when the slave replicates the query, a
    connection number 10 is opened by a normal client on the slave,
    and updates a temp table of the same name? We get a problem
    again. To avoid this, in the handling of temp tables (sql_base.cc)
    we use thread_id AND server_id.  TODO when this is merged into
    4.1: in 4.1, slave_proxy_id has been renamed to pseudo_thread_id
    and is a session variable: that's to make mysqlbinlog work with
    temp tables. We probably need to introduce

    SET PSEUDO_SERVER_ID
    for mysqlbinlog in 4.1. mysqlbinlog would print:
    SET PSEUDO_SERVER_ID=
    SET PSEUDO_THREAD_ID=
    for each query using temp tables.
  */
  int4store(buf + Q_THREAD_ID_OFFSET, slave_proxy_id);
  int4store(buf + Q_EXEC_TIME_OFFSET, exec_time);
  buf[Q_DB_LEN_OFFSET] = (char) db_len;
  int2store(buf + Q_ERR_CODE_OFFSET, error_code);

  /*
    You MUST always write status vars in increasing order of code. This
    guarantees that a slightly older slave will be able to parse those he
    knows.
  */
  start_of_status= start= buf+QUERY_HEADER_LEN;
  if (flags2_inited)
  {
    *start++= Q_FLAGS2_CODE;
    int4store(start, flags2);
    start+= 4;
  }
  if (sql_mode_inited)
  {
    *start++= Q_SQL_MODE_CODE;
    int8store(start, (ulonglong)sql_mode);
    start+= 8;
  }
  if (catalog_len) // i.e. this var is inited (false for 4.0 events)
  {
    write_str_with_code_and_len((char **)(&start),
                                catalog, catalog_len, Q_CATALOG_NZ_CODE);
    /*
      In 5.0.x where x<4 masters we used to store the end zero here. This was
      a waste of one byte so we don't do it in x>=4 masters. We change code to
      Q_CATALOG_NZ_CODE, because re-using the old code would make x<4 slaves
      of this x>=4 master segfault (expecting a zero when there is
      none). Remaining compatibility problems are: the older slave will not
      find the catalog; but it is will not crash, and it's not an issue
      that it does not find the catalog as catalogs were not used in these
      older MySQL versions (we store it in binlog and read it from relay log
      but do nothing useful with it). What is an issue is that the older slave
      will stop processing the Q_* blocks (and jumps to the db/query) as soon
      as it sees unknown Q_CATALOG_NZ_CODE; so it will not be able to read
      Q_AUTO_INCREMENT*, Q_CHARSET and so replication will fail silently in
      various ways. Documented that you should not mix alpha/beta versions if
      they are not exactly the same version, with example of 5.0.3->5.0.2 and
      5.0.4->5.0.3. If replication is from older to new, the new will
      recognize Q_CATALOG_CODE and have no problem.
    */
  }
  if (auto_increment_increment != 1)
  {
    *start++= Q_AUTO_INCREMENT;
    int2store(start, auto_increment_increment);
    int2store(start+2, auto_increment_offset);
    start+= 4;
  }
  if (charset_inited)
  {
    *start++= Q_CHARSET_CODE;
    memcpy(start, charset, 6);
    start+= 6;
  }
  if (time_zone_len)
  {
    /* In the TZ sys table, column Name is of length 64 so this should be ok */
    DBUG_ASSERT(time_zone_len <= MAX_TIME_ZONE_NAME_LENGTH);
    *start++= Q_TIME_ZONE_CODE;
    *start++= time_zone_len;
    memcpy(start, time_zone_str, time_zone_len);
    start+= time_zone_len;
  }
  /*
    Here there could be code like
    if (command-line-option-which-says-"log_this_variable" && inited)
    {
    *start++= Q_THIS_VARIABLE_CODE;
    int4store(start, this_variable);
    start+= 4;
    }
  */
  
  /* Store length of status variables */
  status_vars_len= (uint) (start-start_of_status);
  int2store(buf + Q_STATUS_VARS_LEN_OFFSET, status_vars_len);

  /*
    Calculate length of whole event
    The "1" below is the \0 in the db's length
  */
  event_length= (uint) (start-buf) + get_post_header_size_for_derived() + db_len + 1 + q_len;

  return (write_header(file, event_length) ||
          my_b_safe_write(file, (byte*) buf, QUERY_HEADER_LEN) ||
          write_post_header_for_derived(file) ||
          my_b_safe_write(file, (byte*) start_of_status,
                          (uint) (start-start_of_status)) ||
          my_b_safe_write(file, (db) ? (byte*) db : (byte*)"", db_len + 1) ||
          my_b_safe_write(file, (byte*) query, q_len)) ? 1 : 0;
}

/*
  Query_log_event::Query_log_event()
 
  The simplest constructor that could possibly work.  This is used for
  creating static objects that have a special meaning and are invisible
  to the log.  
*/
Query_log_event::Query_log_event()
  :Log_event(), data_buf(0)
{
}


/*
  Query_log_event::Query_log_event()
*/
Query_log_event::Query_log_event(THD* thd_arg, const char* query_arg,
				 ulong query_length, bool using_trans,
				 bool suppress_use)
  :Log_event(thd_arg,
	     ((thd_arg->tmp_table_used ? LOG_EVENT_THREAD_SPECIFIC_F : 0)
	      | (suppress_use          ? LOG_EVENT_SUPPRESS_USE_F    : 0)),
	     using_trans),
   data_buf(0), query(query_arg), catalog(thd_arg->catalog),
   db(thd_arg->db), q_len((uint32) query_length),
   error_code((thd_arg->killed != THD::NOT_KILLED) ?
              ((thd_arg->system_thread & SYSTEM_THREAD_DELAYED_INSERT) ?
               0 : thd->killed_errno()) : thd_arg->net.last_errno),
   thread_id(thd_arg->thread_id),
   /* save the original thread id; we already know the server id */
   slave_proxy_id(thd_arg->variables.pseudo_thread_id),
   flags2_inited(1), sql_mode_inited(1), charset_inited(1),
   sql_mode(thd_arg->variables.sql_mode),
   auto_increment_increment(thd_arg->variables.auto_increment_increment),
   auto_increment_offset(thd_arg->variables.auto_increment_offset)
{
  time_t end_time;
  time(&end_time);
  exec_time = (ulong) (end_time  - thd->start_time);
  catalog_len = (catalog) ? (uint32) strlen(catalog) : 0;
  /* status_vars_len is set just before writing the event */
  db_len = (db) ? (uint32) strlen(db) : 0;
  /*
    If we don't use flags2 for anything else than options contained in
    thd->options, it would be more efficient to flags2=thd_arg->options
    (OPTIONS_WRITTEN_TO_BINLOG would be used only at reading time).
    But it's likely that we don't want to use 32 bits for 3 bits; in the future
    we will probably want to reclaim the 29 bits. So we need the &.
  */
  flags2= (uint32) (thd_arg->options & OPTIONS_WRITTEN_TO_BIN_LOG);
  DBUG_ASSERT(thd->variables.character_set_client->number < 256*256);
  DBUG_ASSERT(thd->variables.collation_connection->number < 256*256);
  DBUG_ASSERT(thd->variables.collation_server->number < 256*256);
  int2store(charset, thd_arg->variables.character_set_client->number);
  int2store(charset+2, thd_arg->variables.collation_connection->number);
  int2store(charset+4, thd_arg->variables.collation_server->number);
  if (thd_arg->time_zone_used)
  {
    /*
      Note that our event becomes dependent on the Time_zone object
      representing the time zone. Fortunately such objects are never deleted
      or changed during mysqld's lifetime.
    */
    time_zone_len= thd_arg->variables.time_zone->get_name()->length();
    time_zone_str= thd_arg->variables.time_zone->get_name()->ptr();
  }
  else
    time_zone_len= 0;
  DBUG_PRINT("info",("Query_log_event has flags2=%lu sql_mode=%lu",flags2,sql_mode));
}
#endif /* MYSQL_CLIENT */


/* 2 utility functions for the next method */

static void get_str_len_and_pointer(const char **dst, const char **src, uint *len)
{
  if ((*len= **src))
    *dst= *src + 1;                          // Will be copied later
  (*src)+= *len+1;
}


static void copy_str_and_move(char **dst, const char **src, uint len)
{
  memcpy(*dst, *src, len);
  *src= *dst;
  (*dst)+= len;
  *(*dst)++= 0;
}


/*
  Query_log_event::Query_log_event()
  This is used by the SQL slave thread to prepare the event before execution.
*/

Query_log_event::Query_log_event(const char* buf, uint event_len,
                                 const Format_description_log_event *description_event,
                                 Log_event_type event_type)
  :Log_event(buf, description_event), data_buf(0), query(NullS),
   db(NullS), catalog_len(0), status_vars_len(0),
   flags2_inited(0), sql_mode_inited(0), charset_inited(0),
   auto_increment_increment(1), auto_increment_offset(1),
   time_zone_len(0)
{
  ulong data_len;
  uint32 tmp;
  uint8 common_header_len, post_header_len;
  char *start;
  const char *end;
  bool catalog_nz= 1;
  DBUG_ENTER("Query_log_event::Query_log_event(char*,...)");

  common_header_len= description_event->common_header_len;
  post_header_len= description_event->post_header_len[event_type-1];
  DBUG_PRINT("info",("event_len=%ld, common_header_len=%d, post_header_len=%d",
                     event_len, common_header_len, post_header_len));
  
  /*
    We test if the event's length is sensible, and if so we compute data_len.
    We cannot rely on QUERY_HEADER_LEN here as it would not be format-tolerant.
    We use QUERY_HEADER_MINIMAL_LEN which is the same for 3.23, 4.0 & 5.0.
  */
  if (event_len < (uint)(common_header_len + post_header_len))
    DBUG_VOID_RETURN;				
  data_len = event_len - (common_header_len + post_header_len);
  buf+= common_header_len;
  
  slave_proxy_id= thread_id = uint4korr(buf + Q_THREAD_ID_OFFSET);
  exec_time = uint4korr(buf + Q_EXEC_TIME_OFFSET);
  db_len = (uint)buf[Q_DB_LEN_OFFSET]; // TODO: add a check of all *_len vars
  error_code = uint2korr(buf + Q_ERR_CODE_OFFSET);

  /*
    5.0 format starts here.
    Depending on the format, we may or not have affected/warnings etc
    The remnent post-header to be parsed has length:
  */
  tmp= post_header_len - QUERY_HEADER_MINIMAL_LEN; 
  if (tmp)
  {
    status_vars_len= uint2korr(buf + Q_STATUS_VARS_LEN_OFFSET);
    data_len-= status_vars_len;
    DBUG_PRINT("info", ("Query_log_event has status_vars_len: %u",
                        (uint) status_vars_len));
    tmp-= 2;
  }
  /*
    We have parsed everything we know in the post header for QUERY_EVENT,
    the rest of post header is either comes from older version MySQL or
    dedicated to derived events (e.g. Execute_load_query...)
  */

  /* variable-part: the status vars; only in MySQL 5.0  */
  
  start= (char*) (buf+post_header_len);
  end= (const char*) (start+status_vars_len);
  for (const uchar* pos= (const uchar*) start; pos < (const uchar*) end;)
  {
    switch (*pos++) {
    case Q_FLAGS2_CODE:
      flags2_inited= 1;
      flags2= uint4korr(pos);
      DBUG_PRINT("info",("In Query_log_event, read flags2: %lu", flags2));
      pos+= 4;
      break;
    case Q_SQL_MODE_CODE:
    {
#ifndef DBUG_OFF
      char buff[22];
#endif
      sql_mode_inited= 1;
      sql_mode= (ulong) uint8korr(pos); // QQ: Fix when sql_mode is ulonglong
      DBUG_PRINT("info",("In Query_log_event, read sql_mode: %s",
			 llstr(sql_mode, buff)));
      pos+= 8;
      break;
    }
    case Q_CATALOG_NZ_CODE:
      get_str_len_and_pointer(&catalog, (const char **)(&pos), &catalog_len);
      break;
    case Q_AUTO_INCREMENT:
      auto_increment_increment= uint2korr(pos);
      auto_increment_offset=    uint2korr(pos+2);
      pos+= 4;
      break;
    case Q_CHARSET_CODE:
    {
      charset_inited= 1;
      memcpy(charset, pos, 6);
      pos+= 6;
      break;
    }
    case Q_TIME_ZONE_CODE:
    {
      get_str_len_and_pointer(&time_zone_str, (const char **)(&pos), &time_zone_len);
      break;
    }
    case Q_CATALOG_CODE: /* for 5.0.x where 0<=x<=3 masters */
      if ((catalog_len= *pos))
        catalog= (char*) pos+1;                           // Will be copied later
      pos+= catalog_len+2; // leap over end 0
      catalog_nz= 0; // catalog has end 0 in event
      break;
    default:
      /* That's why you must write status vars in growing order of code */
      DBUG_PRINT("info",("Query_log_event has unknown status vars (first has\
 code: %u), skipping the rest of them", (uint) *(pos-1)));
      pos= (const uchar*) end;                         // Break loop
    }
  }
  
#if !defined(MYSQL_CLIENT) && defined(HAVE_QUERY_CACHE)
  if (!(start= data_buf = (char*) my_malloc(catalog_len + 1 +
                                            time_zone_len + 1 +
                                            data_len + 1 +
					    QUERY_CACHE_FLAGS_SIZE +
					    db_len + 1,
					    MYF(MY_WME))))
#else
  if (!(start= data_buf = (char*) my_malloc(catalog_len + 1 +
                                            time_zone_len + 1 +
                                            data_len + 1,
					    MYF(MY_WME))))
#endif
      DBUG_VOID_RETURN;
  if (catalog_len)                                  // If catalog is given
  {
    if (likely(catalog_nz)) // true except if event comes from 5.0.0|1|2|3.
      copy_str_and_move(&start, &catalog, catalog_len);
    else
    {
      memcpy(start, catalog, catalog_len+1); // copy end 0
      catalog= start;
      start+= catalog_len+1;
    }
  }
  if (time_zone_len)
    copy_str_and_move(&start, &time_zone_str, time_zone_len);

  /* A 2nd variable part; this is common to all versions */ 
  memcpy((char*) start, end, data_len);          // Copy db and query
  start[data_len]= '\0';              // End query with \0 (For safetly)
  db= start;
  query= start + db_len + 1;
  q_len= data_len - db_len -1;
  DBUG_VOID_RETURN;
}


/*
  Query_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Query_log_event::print_query_header(FILE* file,
					 PRINT_EVENT_INFO* print_event_info)
{
  // TODO: print the catalog ??
  char buff[40],*end;				// Enough for SET TIMESTAMP
  bool different_db= 1;
  uint32 tmp;

  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\t%s\tthread_id=%lu\texec_time=%lu\terror_code=%d\n",
	    get_type_str(), (ulong) thread_id, (ulong) exec_time, error_code);
  }

  if (!(flags & LOG_EVENT_SUPPRESS_USE_F) && db)
  {
    if (different_db= memcmp(print_event_info->db, db, db_len + 1))
      memcpy(print_event_info->db, db, db_len + 1);
    if (db[0] && different_db) 
      fprintf(file, "use %s;\n", db);
  }

  end=int10_to_str((long) when, strmov(buff,"SET TIMESTAMP="),10);
  *end++=';';
  *end++='\n';
  my_fwrite(file, (byte*) buff, (uint) (end-buff),MYF(MY_NABP | MY_WME));
  if (flags & LOG_EVENT_THREAD_SPECIFIC_F)
    fprintf(file,"SET @@session.pseudo_thread_id=%lu;\n",(ulong)thread_id);

  /*
    If flags2_inited==0, this is an event from 3.23 or 4.0; nothing to
    print (remember we don't produce mixed relay logs so there cannot be
    5.0 events before that one so there is nothing to reset).
  */
  if (likely(flags2_inited)) /* likely as this will mainly read 5.0 logs */
  {
    /* tmp is a bitmask of bits which have changed. */
    if (likely(print_event_info->flags2_inited)) 
      /* All bits which have changed */
      tmp= (print_event_info->flags2) ^ flags2;
    else /* that's the first Query event we read */
    {
      print_event_info->flags2_inited= 1;
      tmp= ~((uint32)0); /* all bits have changed */
    }

    if (unlikely(tmp)) /* some bits have changed */
    {
      bool need_comma= 0;
      fprintf(file, "SET ");
      print_set_option(file, tmp, OPTION_NO_FOREIGN_KEY_CHECKS, ~flags2,
                   "@@session.foreign_key_checks", &need_comma);
      print_set_option(file, tmp, OPTION_AUTO_IS_NULL, flags2,
                   "@@session.sql_auto_is_null", &need_comma);
      print_set_option(file, tmp, OPTION_RELAXED_UNIQUE_CHECKS, ~flags2,
                   "@@session.unique_checks", &need_comma);
      fprintf(file,";\n");
      print_event_info->flags2= flags2;
    }
  }

  /*
    Now the session variables;
    it's more efficient to pass SQL_MODE as a number instead of a
    comma-separated list.
    FOREIGN_KEY_CHECKS, SQL_AUTO_IS_NULL, UNIQUE_CHECKS are session-only
    variables (they have no global version; they're not listed in
    sql_class.h), The tests below work for pure binlogs or pure relay
    logs. Won't work for mixed relay logs but we don't create mixed
    relay logs (that is, there is no relay log with a format change
    except within the 3 first events, which mysqlbinlog handles
    gracefully). So this code should always be good.
  */

  if (likely(sql_mode_inited))
  {
    if (unlikely(!print_event_info->sql_mode_inited)) /* first Query event */
    {
      print_event_info->sql_mode_inited= 1;
      /* force a difference to force write */
      print_event_info->sql_mode= ~sql_mode;
    }
    if (unlikely(print_event_info->sql_mode != sql_mode))
    {
      fprintf(file,"SET @@session.sql_mode=%lu;\n",(ulong)sql_mode);
      print_event_info->sql_mode= sql_mode;
    }
  }
  if (print_event_info->auto_increment_increment != auto_increment_increment ||
      print_event_info->auto_increment_offset != auto_increment_offset)
  {
    fprintf(file,"SET @@session.auto_increment_increment=%lu, @@session.auto_increment_offset=%lu;\n",
            auto_increment_increment,auto_increment_offset);
    print_event_info->auto_increment_increment= auto_increment_increment;
    print_event_info->auto_increment_offset=    auto_increment_offset;
  }

  /* TODO: print the catalog when we feature SET CATALOG */

  if (likely(charset_inited))
  {
    if (unlikely(!print_event_info->charset_inited)) /* first Query event */
    {
      print_event_info->charset_inited= 1;
      print_event_info->charset[0]= ~charset[0]; // force a difference to force write
    }
    if (unlikely(bcmp(print_event_info->charset, charset, 6)))
    {
      CHARSET_INFO *cs_info= get_charset(uint2korr(charset), MYF(MY_WME));
      if (cs_info)
      {
        fprintf(file, "/*!\\C %s */;\n", cs_info->csname); /* for mysql client */
      }
      fprintf(file,"SET "
              "@@session.character_set_client=%d,"
              "@@session.collation_connection=%d,"
              "@@session.collation_server=%d"
              ";\n",
              uint2korr(charset),
              uint2korr(charset+2),
              uint2korr(charset+4));
      memcpy(print_event_info->charset, charset, 6);
    }
  }
  if (time_zone_len)
  {
    if (bcmp(print_event_info->time_zone_str, time_zone_str, time_zone_len+1))
    {
      fprintf(file,"SET @@session.time_zone='%s';\n", time_zone_str);
      memcpy(print_event_info->time_zone_str, time_zone_str, time_zone_len+1);
    }
  }
}


void Query_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  print_query_header(file, print_event_info);
  my_fwrite(file, (byte*) query, q_len, MYF(MY_NABP | MY_WME));
  fputs(";\n", file);
}
#endif /* MYSQL_CLIENT */


/*
  Query_log_event::exec_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

int Query_log_event::exec_event(struct st_relay_log_info* rli)
{
  return exec_event(rli, query, q_len);
}


int Query_log_event::exec_event(struct st_relay_log_info* rli,
                                const char *query_arg, uint32 q_len_arg)
{
  LEX_STRING new_db;
  int expected_error,actual_error= 0;
  /*
    Colleagues: please never free(thd->catalog) in MySQL. This would lead to
    bugs as here thd->catalog is a part of an alloced block, not an entire
    alloced block (see Query_log_event::exec_event()). Same for thd->db.
    Thank you.
  */
  thd->catalog= catalog_len ? (char *) catalog : (char *)"";
  new_db.length= db_len;
  new_db.str= (char *) rpl_filter->get_rewrite_db(db, &new_db.length);
  thd->set_db(new_db.str, new_db.length);       /* allocates a copy of 'db' */
  thd->variables.auto_increment_increment= auto_increment_increment;
  thd->variables.auto_increment_offset=    auto_increment_offset;

  /*
    InnoDB internally stores the master log position it has executed so far,
    i.e. the position just after the COMMIT event.
    When InnoDB will want to store, the positions in rli won't have
    been updated yet, so group_master_log_* will point to old BEGIN
    and event_master_log* will point to the beginning of current COMMIT.
    But log_pos of the COMMIT Query event is what we want, i.e. the pos of the
    END of the current log event (COMMIT). We save it in rli so that InnoDB can
    access it.
  */
  rli->future_group_master_log_pos= log_pos;
  DBUG_PRINT("info", ("log_pos: %lu", (ulong) log_pos));

  clear_all_errors(thd, rli);
  rli->clear_tables_to_lock();

  /*
    Note:   We do not need to execute reset_one_shot_variables() if this
            db_ok() test fails.
    Reason: The db stored in binlog events is the same for SET and for
            its companion query.  If the SET is ignored because of
            db_ok(), the companion query will also be ignored, and if
            the companion query is ignored in the db_ok() test of
            ::exec_event(), then the companion SET also have so we
            don't need to reset_one_shot_variables().
  */
  if (rpl_filter->db_ok(thd->db))
  {
    thd->set_time((time_t)when);
    thd->query_length= q_len_arg;
    thd->query= (char*)query_arg;
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    thd->query_id = next_query_id();
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    thd->variables.pseudo_thread_id= thread_id;		// for temp tables
    DBUG_PRINT("query",("%s",thd->query));

    if (ignored_error_code((expected_error= error_code)) ||
	!check_expected_error(thd,rli,expected_error))
    {
      if (flags2_inited)
        /*
          all bits of thd->options which are 1 in OPTIONS_WRITTEN_TO_BIN_LOG
          must take their value from flags2.
        */
        thd->options= flags2|(thd->options & ~OPTIONS_WRITTEN_TO_BIN_LOG);
      /*
        else, we are in a 3.23/4.0 binlog; we previously received a
        Rotate_log_event which reset thd->options and sql_mode etc, so
        nothing to do.
      */
      /*
        We do not replicate IGNORE_DIR_IN_CREATE. That is, if the master is a
        slave which runs with SQL_MODE=IGNORE_DIR_IN_CREATE, this should not
        force us to ignore the dir too. Imagine you are a ring of machines, and
        one has a disk problem so that you temporarily need
        IGNORE_DIR_IN_CREATE on this machine; you don't want it to propagate
        elsewhere (you don't want all slaves to start ignoring the dirs).
      */
      if (sql_mode_inited)
        thd->variables.sql_mode=
          (ulong) ((thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE) |
                   (sql_mode & ~(ulong) MODE_NO_DIR_IN_CREATE));
      if (charset_inited)
      {
        if (rli->cached_charset_compare(charset))
        {
          /* Verify that we support the charsets found in the event. */
          if (!(thd->variables.character_set_client=
                get_charset(uint2korr(charset), MYF(MY_WME))) ||
              !(thd->variables.collation_connection=
                get_charset(uint2korr(charset+2), MYF(MY_WME))) ||
              !(thd->variables.collation_server=
                get_charset(uint2korr(charset+4), MYF(MY_WME))))
          {
            /*
              We updated the thd->variables with nonsensical values (0). Let's
              set them to something safe (i.e. which avoids crash), and we'll
              stop with EE_UNKNOWN_CHARSET in compare_errors (unless set to
              ignore this error).
            */
            set_slave_thread_default_charset(thd, rli);
            goto compare_errors;
          }
          thd->update_charset(); // for the charset change to take effect
        }
      }
      if (time_zone_len)
      {
        String tmp(time_zone_str, time_zone_len, &my_charset_bin);
        if (!(thd->variables.time_zone=
              my_tz_find_with_opening_tz_tables(thd, &tmp)))
        {
          my_error(ER_UNKNOWN_TIME_ZONE, MYF(0), tmp.c_ptr());
          thd->variables.time_zone= global_system_variables.time_zone;
          goto compare_errors;
        }
      }

      /* Execute the query (note that we bypass dispatch_command()) */
      mysql_parse(thd, thd->query, thd->query_length);

    }
    else
    {
      /*
        The query got a really bad error on the master (thread killed etc),
        which could be inconsistent. Parse it to test the table names: if the
        replicate-*-do|ignore-table rules say "this query must be ignored" then
        we exit gracefully; otherwise we warn about the bad error and tell DBA
        to check/fix it.
      */
      if (mysql_test_parse_for_slave(thd, thd->query, thd->query_length))
        clear_all_errors(thd, rli);        /* Can ignore query */
      else
      {
        slave_print_msg(ERROR_LEVEL, rli, expected_error, 
                          "\
Query partially completed on the master (error on master: %d) \
and was aborted. There is a chance that your master is inconsistent at this \
point. If you are sure that your master is ok, run this query manually on the \
slave and then restart the slave with SET GLOBAL SQL_SLAVE_SKIP_COUNTER=1; \
START SLAVE; . Query: '%s'", expected_error, thd->query);
        thd->query_error= 1;
      }
      goto end;
    }

    /* If the query was not ignored, it is printed to the general log */
    if (thd->net.last_errno != ER_SLAVE_IGNORED_TABLE)
      general_log_print(thd, COM_QUERY, "%s", thd->query);

compare_errors:

     /*
      If we expected a non-zero error code, and we don't get the same error
      code, and none of them should be ignored.
    */
    DBUG_PRINT("info",("expected_error: %d  last_errno: %d",
 		       expected_error, thd->net.last_errno));
    if ((expected_error != (actual_error= thd->net.last_errno)) &&
 	expected_error &&
 	!ignored_error_code(actual_error) &&
 	!ignored_error_code(expected_error))
    {
      slave_print_msg(ERROR_LEVEL, rli, 0,
                      "\
Query caused different errors on master and slave.     \
Error on master: '%s' (%d), Error on slave: '%s' (%d). \
Default database: '%s'. Query: '%s'",
                      ER_SAFE(expected_error),
                      expected_error,
                      actual_error ? thd->net.last_error: "no error",
                      actual_error,
                      print_slave_db_safe(db), query_arg);
      thd->query_error= 1;
    }
    /*
      If we get the same error code as expected, or they should be ignored. 
    */
    else if (expected_error == actual_error ||
 	     ignored_error_code(actual_error))
    {
      DBUG_PRINT("info",("error ignored"));
      clear_all_errors(thd, rli);
    }
    /*
      Other cases: mostly we expected no error and get one.
    */
    else if (thd->query_error || thd->is_fatal_error)
    {
      slave_print_msg(ERROR_LEVEL, rli, actual_error,
                      "Error '%s' on query. Default database: '%s'. Query: '%s'",
                      (actual_error ? thd->net.last_error :
                       "unexpected success or fatal error"),
                      print_slave_db_safe(thd->db), query_arg);
      thd->query_error= 1;
    }

    /*
      TODO: compare the values of "affected rows" around here. Something
      like:
      if ((uint32) affected_in_event != (uint32) affected_on_slave)
      {
      sql_print_error("Slave: did not get the expected number of affected \
      rows running query from master - expected %d, got %d (this numbers \
      should have matched modulo 4294967296).", 0, ...);
      thd->query_error = 1;
      }
      We may also want an option to tell the slave to ignore "affected"
      mismatch. This mismatch could be implemented with a new ER_ code, and
      to ignore it you would use --slave-skip-errors...

      To do the comparison we need to know the value of "affected" which the
      above mysql_parse() computed. And we need to know the value of
      "affected" in the master's binlog. Both will be implemented later. The
      important thing is that we now have the format ready to log the values
      of "affected" in the binlog. So we can release 5.0.0 before effectively
      logging "affected" and effectively comparing it.
    */
  } /* End of if (db_ok(... */

end:
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  /*
    Probably we have set thd->query, thd->db, thd->catalog to point to places
    in the data_buf of this event. Now the event is going to be deleted
    probably, so data_buf will be freed, so the thd->... listed above will be
    pointers to freed memory. 
    So we must set them to 0, so that those bad pointers values are not later
    used. Note that "cleanup" queries like automatic DROP TEMPORARY TABLE
    don't suffer from these assignments to 0 as DROP TEMPORARY
    TABLE uses the db.table syntax.
  */
  thd->catalog= 0;
  thd->set_db(NULL, 0);                 /* will free the current database */
  thd->query= 0;			// just to be sure
  thd->query_length= 0;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  close_thread_tables(thd);      
  /*
    As a disk space optimization, future masters will not log an event for
    LAST_INSERT_ID() if that function returned 0 (and thus they will be able
    to replace the THD::stmt_depends_on_first_successful_insert_id_in_prev_stmt
    variable by (THD->first_successful_insert_id_in_prev_stmt > 0) ; with the
    resetting below we are ready to support that.
  */
  thd->first_successful_insert_id_in_prev_stmt_for_binlog= 0;
  thd->first_successful_insert_id_in_prev_stmt= 0;
  thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
  /*
    If there was an error we stop. Otherwise we increment positions. Note that
    we will not increment group* positions if we are just after a SET
    ONE_SHOT, because SET ONE_SHOT should not be separated from its following
    updating query.
  */
  return (thd->query_error ? thd->query_error : 
          (thd->one_shot_set ? (rli->inc_event_relay_log_pos(),0) :
           Log_event::exec_event(rli))); 
}
#endif


/**************************************************************************
	Muted_query_log_event methods
**************************************************************************/

#ifndef MYSQL_CLIENT
/*
  Muted_query_log_event::Muted_query_log_event()
*/
Muted_query_log_event::Muted_query_log_event()
  :Query_log_event()
{
}
#endif


/**************************************************************************
	Start_log_event_v3 methods
**************************************************************************/

#ifndef MYSQL_CLIENT
Start_log_event_v3::Start_log_event_v3() :Log_event(), binlog_version(BINLOG_VERSION), artificial_event(0)
{
  created= when;
  memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
}
#endif

/*
  Start_log_event_v3::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Start_log_event_v3::pack_info(Protocol *protocol)
{
  char buf[12 + ST_SERVER_VER_LEN + 14 + 22], *pos;
  pos= strmov(buf, "Server ver: ");
  pos= strmov(pos, server_version);
  pos= strmov(pos, ", Binlog ver: ");
  pos= int10_to_str(binlog_version, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}
#endif


/*
  Start_log_event_v3::print()
*/

#ifdef MYSQL_CLIENT
void Start_log_event_v3::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tStart: binlog v %d, server v %s created ", binlog_version,
            server_version);
    print_timestamp(file);
    if (created)
      fprintf(file," at startup");
    fputc('\n', file);
    if (flags & LOG_EVENT_BINLOG_IN_USE_F)
      fprintf(file, "# Warning: this binlog was not closed properly. "
              "Most probably mysqld crashed writing it.\n");
  }
  if (!artificial_event && created)
  {
#ifdef WHEN_WE_HAVE_THE_RESET_CONNECTION_SQL_COMMAND
    /*
      This is for mysqlbinlog: like in replication, we want to delete the stale
      tmp files left by an unclean shutdown of mysqld (temporary tables)
      and rollback unfinished transaction.
      Probably this can be done with RESET CONNECTION (syntax to be defined).
    */
    fprintf(file,"RESET CONNECTION;\n");
#else
    fprintf(file,"ROLLBACK;\n");
#endif
  }
  fflush(file);
}
#endif /* MYSQL_CLIENT */

/*
  Start_log_event_v3::Start_log_event_v3()
*/

Start_log_event_v3::Start_log_event_v3(const char* buf,
                                       const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  buf+= description_event->common_header_len;
  binlog_version= uint2korr(buf+ST_BINLOG_VER_OFFSET);
  memcpy(server_version, buf+ST_SERVER_VER_OFFSET,
	 ST_SERVER_VER_LEN);
  created= uint4korr(buf+ST_CREATED_OFFSET);
  /* We use log_pos to mark if this was an artificial event or not */
  artificial_event= (log_pos == 0);
}


/*
  Start_log_event_v3::write()
*/

#ifndef MYSQL_CLIENT
bool Start_log_event_v3::write(IO_CACHE* file)
{
  char buff[START_V3_HEADER_LEN];
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy(buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  int4store(buff + ST_CREATED_OFFSET,created);
  return (write_header(file, sizeof(buff)) ||
          my_b_safe_write(file, (byte*) buff, sizeof(buff)));
}
#endif


/*
  Start_log_event_v3::exec_event()

  The master started

  IMPLEMENTATION
    - To handle the case where the master died without having time to write
      DROP TEMPORARY TABLE, DO RELEASE_LOCK (prepared statements' deletion is
      TODO), we clean up all temporary tables that we got, if we are sure we
      can (see below).

  TODO
    - Remove all active user locks.
      Guilhem 2003-06: this is true but not urgent: the worst it can cause is
      the use of a bit of memory for a user lock which will not be used
      anymore. If the user lock is later used, the old one will be released. In
      other words, no deadlock problem.
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Start_log_event_v3::exec_event(struct st_relay_log_info* rli)
{
  DBUG_ENTER("Start_log_event_v3::exec_event");
  switch (binlog_version)
  {
  case 3:
  case 4:
    /*
      This can either be 4.x (then a Start_log_event_v3 is only at master
      startup so we are sure the master has restarted and cleared his temp
      tables; the event always has 'created'>0) or 5.0 (then we have to test
      'created').
    */
    if (created)
    {
      close_temporary_tables(thd);
      cleanup_load_tmpdir();
    }
    break;

    /*
       Now the older formats; in that case load_tmpdir is cleaned up by the I/O
       thread.
    */
  case 1:
    if (strncmp(rli->relay_log.description_event_for_exec->server_version,
                "3.23.57",7) >= 0 && created)
    {
      /*
        Can distinguish, based on the value of 'created': this event was
        generated at master startup.
      */
      close_temporary_tables(thd);
    }
    /*
      Otherwise, can't distinguish a Start_log_event generated at
      master startup and one generated by master FLUSH LOGS, so cannot
      be sure temp tables have to be dropped. So do nothing.
    */
    break;
  default:
    /* this case is impossible */
    DBUG_RETURN(1);
  }
  DBUG_RETURN(Log_event::exec_event(rli));
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */

/***************************************************************************
       Format_description_log_event methods
****************************************************************************/

/*
  Format_description_log_event 1st ctor.

  SYNOPSIS
    Format_description_log_event::Format_description_log_event
      binlog_version              the binlog version for which we want to build
                                  an event. Can be 1 (=MySQL 3.23), 3 (=4.0.x
                                  x>=2 and 4.1) or 4 (MySQL 5.0). Note that the
                                  old 4.0 (binlog version 2) is not supported;
                                  it should not be used for replication with
                                  5.0.

  DESCRIPTION
    Ctor. Can be used to create the event to write to the binary log (when the
    server starts or when FLUSH LOGS), or to create artificial events to parse
    binlogs from MySQL 3.23 or 4.x.
    When in a client, only the 2nd use is possible.
*/

Format_description_log_event::
Format_description_log_event(uint8 binlog_ver, const char* server_ver)
  :Start_log_event_v3()
{
  created= when;
  binlog_version= binlog_ver;
  switch (binlog_ver) {
  case 4: /* MySQL 5.0 */
    memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
    common_header_len= LOG_EVENT_HEADER_LEN;
    number_of_event_types= LOG_EVENT_TYPES;
    /* we'll catch my_malloc() error in is_valid() */
    post_header_len=(uint8*) my_malloc(number_of_event_types*sizeof(uint8),
                                       MYF(MY_ZEROFILL));
    /*
      This long list of assignments is not beautiful, but I see no way to
      make it nicer, as the right members are #defines, not array members, so
      it's impossible to write a loop.
    */
    if (post_header_len)
    {
      post_header_len[START_EVENT_V3-1]= START_V3_HEADER_LEN;
      post_header_len[QUERY_EVENT-1]= QUERY_HEADER_LEN;
      post_header_len[ROTATE_EVENT-1]= ROTATE_HEADER_LEN;
      post_header_len[LOAD_EVENT-1]= LOAD_HEADER_LEN;
      post_header_len[CREATE_FILE_EVENT-1]= CREATE_FILE_HEADER_LEN;
      post_header_len[APPEND_BLOCK_EVENT-1]= APPEND_BLOCK_HEADER_LEN;
      post_header_len[EXEC_LOAD_EVENT-1]= EXEC_LOAD_HEADER_LEN;
      post_header_len[DELETE_FILE_EVENT-1]= DELETE_FILE_HEADER_LEN;
      post_header_len[NEW_LOAD_EVENT-1]= post_header_len[LOAD_EVENT-1];
      post_header_len[FORMAT_DESCRIPTION_EVENT-1]= FORMAT_DESCRIPTION_HEADER_LEN;
      post_header_len[TABLE_MAP_EVENT-1]=    TABLE_MAP_HEADER_LEN;
      post_header_len[WRITE_ROWS_EVENT-1]=   ROWS_HEADER_LEN;
      post_header_len[UPDATE_ROWS_EVENT-1]=  ROWS_HEADER_LEN;
      post_header_len[DELETE_ROWS_EVENT-1]=  ROWS_HEADER_LEN;
      /*
        We here have the possibility to simulate a master of before we changed
        the table map id to be stored in 6 bytes: when it was stored in 4
        bytes (=> post_header_len was 6). This is used to test backward
        compatibility.
        This code can be removed after a few months (today is Dec 21st 2005),
        when we know that the 4-byte masters are not deployed anymore (check
        with Tomas Ulin first!), and the accompanying test (rpl_row_4_bytes)
        too.
      */
      DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                      post_header_len[TABLE_MAP_EVENT-1]=
                      post_header_len[WRITE_ROWS_EVENT-1]=
                      post_header_len[UPDATE_ROWS_EVENT-1]=
                      post_header_len[DELETE_ROWS_EVENT-1]= 6;);
      post_header_len[BEGIN_LOAD_QUERY_EVENT-1]= post_header_len[APPEND_BLOCK_EVENT-1];
      post_header_len[EXECUTE_LOAD_QUERY_EVENT-1]= EXECUTE_LOAD_QUERY_HEADER_LEN;
    }
    break;

  case 1: /* 3.23 */
  case 3: /* 4.0.x x>=2 */
    /*
      We build an artificial (i.e. not sent by the master) event, which
      describes what those old master versions send.
    */
    if (binlog_ver==1)
      strmov(server_version, server_ver ? server_ver : "3.23");
    else
      strmov(server_version, server_ver ? server_ver : "4.0");
    common_header_len= binlog_ver==1 ? OLD_HEADER_LEN :
      LOG_EVENT_MINIMAL_HEADER_LEN;
    /*
      The first new event in binlog version 4 is Format_desc. So any event type
      after that does not exist in older versions. We use the events known by
      version 3, even if version 1 had only a subset of them (this is not a
      problem: it uses a few bytes for nothing but unifies code; it does not
      make the slave detect less corruptions).
    */
    number_of_event_types= FORMAT_DESCRIPTION_EVENT - 1;
    post_header_len=(uint8*) my_malloc(number_of_event_types*sizeof(uint8),
                                       MYF(0));
    if (post_header_len)
    {
      post_header_len[START_EVENT_V3-1]= START_V3_HEADER_LEN;
      post_header_len[QUERY_EVENT-1]= QUERY_HEADER_MINIMAL_LEN;
      post_header_len[STOP_EVENT-1]= 0;
      post_header_len[ROTATE_EVENT-1]= (binlog_ver==1) ? 0 : ROTATE_HEADER_LEN;
      post_header_len[INTVAR_EVENT-1]= 0;
      post_header_len[LOAD_EVENT-1]= LOAD_HEADER_LEN;
      post_header_len[SLAVE_EVENT-1]= 0;
      post_header_len[CREATE_FILE_EVENT-1]= CREATE_FILE_HEADER_LEN;
      post_header_len[APPEND_BLOCK_EVENT-1]= APPEND_BLOCK_HEADER_LEN;
      post_header_len[EXEC_LOAD_EVENT-1]= EXEC_LOAD_HEADER_LEN;
      post_header_len[DELETE_FILE_EVENT-1]= DELETE_FILE_HEADER_LEN;
      post_header_len[NEW_LOAD_EVENT-1]= post_header_len[LOAD_EVENT-1];
      post_header_len[RAND_EVENT-1]= 0;
      post_header_len[USER_VAR_EVENT-1]= 0;
    }
    break;
  default: /* Includes binlog version 2 i.e. 4.0.x x<=1 */
    post_header_len= 0; /* will make is_valid() fail */
    break;
  }
}


/*
  The problem with this constructor is that the fixed header may have a
  length different from this version, but we don't know this length as we
  have not read the Format_description_log_event which says it, yet. This
  length is in the post-header of the event, but we don't know where the
  post-header starts.
  So this type of event HAS to:
  - either have the header's length at the beginning (in the header, at a
  fixed position which will never be changed), not in the post-header. That
  would make the header be "shifted" compared to other events.
  - or have a header of size LOG_EVENT_MINIMAL_HEADER_LEN (19), in all future
  versions, so that we know for sure.
  I (Guilhem) chose the 2nd solution. Rotate has the same constraint (because
  it is sent before Format_description_log_event).
*/

Format_description_log_event::
Format_description_log_event(const char* buf,
                             uint event_len,
                             const
                             Format_description_log_event*
                             description_event)
  :Start_log_event_v3(buf, description_event)
{
  DBUG_ENTER("Format_description_log_event::Format_description_log_event(char*,...)");
  buf+= LOG_EVENT_MINIMAL_HEADER_LEN;
  if ((common_header_len=buf[ST_COMMON_HEADER_LEN_OFFSET]) < OLD_HEADER_LEN)
    DBUG_VOID_RETURN; /* sanity check */
  number_of_event_types=
    event_len-(LOG_EVENT_MINIMAL_HEADER_LEN+ST_COMMON_HEADER_LEN_OFFSET+1);
  DBUG_PRINT("info", ("common_header_len=%d number_of_event_types=%d",
                      common_header_len, number_of_event_types));
  /* If alloc fails, we'll detect it in is_valid() */
  post_header_len= (uint8*) my_memdup((byte*)buf+ST_COMMON_HEADER_LEN_OFFSET+1,
                                      number_of_event_types*
                                      sizeof(*post_header_len), MYF(0));
  DBUG_VOID_RETURN;
}

#ifndef MYSQL_CLIENT
bool Format_description_log_event::write(IO_CACHE* file)
{
  /*
    We don't call Start_log_event_v3::write() because this would make 2
    my_b_safe_write().
  */
  byte buff[FORMAT_DESCRIPTION_HEADER_LEN];
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy((char*) buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  int4store(buff + ST_CREATED_OFFSET,created);
  buff[ST_COMMON_HEADER_LEN_OFFSET]= LOG_EVENT_HEADER_LEN;
  memcpy((char*) buff+ST_COMMON_HEADER_LEN_OFFSET+1, (byte*) post_header_len,
         LOG_EVENT_TYPES);
  return (write_header(file, sizeof(buff)) ||
          my_b_safe_write(file, buff, sizeof(buff)));
}
#endif

/*
  SYNOPSIS
    Format_description_log_event::exec_event()

  IMPLEMENTATION
    Save the information which describes the binlog's format, to be able to
    read all coming events.
    Call Start_log_event_v3::exec_event().
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Format_description_log_event::exec_event(struct st_relay_log_info* rli)
{
  DBUG_ENTER("Format_description_log_event::exec_event");

  /* save the information describing this binlog */
  delete rli->relay_log.description_event_for_exec;
  rli->relay_log.description_event_for_exec= this;

#ifdef USING_TRANSACTIONS
  /*
    As a transaction NEVER spans on 2 or more binlogs:
    if we have an active transaction at this point, the master died
    while writing the transaction to the binary log, i.e. while
    flushing the binlog cache to the binlog. XA guarantees that master has
    rolled back. So we roll back.
    Note: this event could be sent by the master to inform us of the
    format of its binlog; in other words maybe it is not at its
    original place when it comes to us; we'll know this by checking
    log_pos ("artificial" events have log_pos == 0).
  */
  if (!artificial_event && created && thd->transaction.all.nht)
  {
    /* This is not an error (XA is safe), just an information */
    slave_print_msg(INFORMATION_LEVEL, rli, 0,
                    "Rolling back unfinished transaction (no COMMIT "
                    "or ROLLBACK in relay log). A probable cause is that "
                    "the master died while writing the transaction to "
                    "its binary log, thus rolled back too."); 
    rli->cleanup_context(thd, 1);
  }
#endif
  /*
    If this event comes from ourselves, there is no cleaning task to perform,
    we don't call Start_log_event_v3::exec_event() (this was just to update the
    log's description event).
  */
  if (server_id == (uint32) ::server_id)
  {
    /*
      Do not modify rli->group_master_log_pos, as this event did not exist on
      the master. That is, just update the *relay log* coordinates; this is
      done by passing log_pos=0 to inc_group_relay_log_pos, like we do in
      Stop_log_event::exec_event().
      If in a transaction, don't touch group_* coordinates.
    */
    if (thd->options & OPTION_BEGIN)
      rli->inc_event_relay_log_pos();
    else
    {
      rli->inc_group_relay_log_pos(0);
      flush_relay_log_info(rli);
    }
    DBUG_RETURN(0);
  }

  /*
    If the event was not requested by the slave i.e. the master sent it while
    the slave asked for a position >4, the event will make
    rli->group_master_log_pos advance. Say that the slave asked for position
    1000, and the Format_desc event's end is 96. Then in the beginning of
    replication rli->group_master_log_pos will be 0, then 96, then jump to
    first really asked event (which is >96). So this is ok.
  */
  DBUG_RETURN(Start_log_event_v3::exec_event(rli));
}
#endif

  /**************************************************************************
        Load_log_event methods
   General note about Load_log_event: the binlogging of LOAD DATA INFILE is
   going to be changed in 5.0 (or maybe in 5.1; not decided yet).
   However, the 5.0 slave could still have to read such events (from a 4.x
   master), convert them (which just means maybe expand the header, when 5.0
   servers have a UID in events) (remember that whatever is after the header
   will be like in 4.x, as this event's format is not modified in 5.0 as we
   will use new types of events to log the new LOAD DATA INFILE features).
   To be able to read/convert, we just need to not assume that the common
   header is of length LOG_EVENT_HEADER_LEN (we must use the description
   event).
   Note that I (Guilhem) manually tested replication of a big LOAD DATA INFILE
   between 3.23 and 5.0, and between 4.0 and 5.0, and it works fine (and the
   positions displayed in SHOW SLAVE STATUS then are fine too).
  **************************************************************************/

/*
  Load_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
uint Load_log_event::get_query_buffer_length()
{
  return
    5 + db_len + 3 +                        // "use DB; "
    18 + fname_len + 2 +                    // "LOAD DATA INFILE 'file''"
    7 +					    // LOCAL
    9 +                                     // " REPLACE or IGNORE "
    13 + table_name_len*2 +                 // "INTO TABLE `table`"
    21 + sql_ex.field_term_len*4 + 2 +      // " FIELDS TERMINATED BY 'str'"
    23 + sql_ex.enclosed_len*4 + 2 +        // " OPTIONALLY ENCLOSED BY 'str'"
    12 + sql_ex.escaped_len*4 + 2 +         // " ESCAPED BY 'str'"
    21 + sql_ex.line_term_len*4 + 2 +       // " FIELDS TERMINATED BY 'str'"
    19 + sql_ex.line_start_len*4 + 2 +      // " LINES STARTING BY 'str'"
    15 + 22 +                               // " IGNORE xxx  LINES"
    3 + (num_fields-1)*2 + field_block_len; // " (field1, field2, ...)"
}


void Load_log_event::print_query(bool need_db, char *buf,
                                 char **end, char **fn_start, char **fn_end)
{
  char *pos= buf;

  if (need_db && db && db_len)
  {
    pos= strmov(pos, "use `");
    memcpy(pos, db, db_len);
    pos= strmov(pos+db_len, "`; ");
  }

  pos= strmov(pos, "LOAD DATA ");

  if (fn_start)
    *fn_start= pos;

  if (check_fname_outside_temp_buf())
    pos= strmov(pos, "LOCAL ");
  pos= strmov(pos, "INFILE '");
  memcpy(pos, fname, fname_len);
  pos= strmov(pos+fname_len, "' ");

  if (sql_ex.opt_flags & REPLACE_FLAG)
    pos= strmov(pos, " REPLACE ");
  else if (sql_ex.opt_flags & IGNORE_FLAG)
    pos= strmov(pos, " IGNORE ");

  pos= strmov(pos ,"INTO");

  if (fn_end)
    *fn_end= pos;

  pos= strmov(pos ," TABLE `");
  memcpy(pos, table_name, table_name_len);
  pos+= table_name_len;

  /* We have to create all optinal fields as the default is not empty */
  pos= strmov(pos, "` FIELDS TERMINATED BY ");
  pos= pretty_print_str(pos, sql_ex.field_term, sql_ex.field_term_len);
  if (sql_ex.opt_flags & OPT_ENCLOSED_FLAG)
    pos= strmov(pos, " OPTIONALLY ");
  pos= strmov(pos, " ENCLOSED BY ");
  pos= pretty_print_str(pos, sql_ex.enclosed, sql_ex.enclosed_len);

  pos= strmov(pos, " ESCAPED BY ");
  pos= pretty_print_str(pos, sql_ex.escaped, sql_ex.escaped_len);

  pos= strmov(pos, " LINES TERMINATED BY ");
  pos= pretty_print_str(pos, sql_ex.line_term, sql_ex.line_term_len);
  if (sql_ex.line_start_len)
  {
    pos= strmov(pos, " STARTING BY ");
    pos= pretty_print_str(pos, sql_ex.line_start, sql_ex.line_start_len);
  }

  if ((long) skip_lines > 0)
  {
    pos= strmov(pos, " IGNORE ");
    pos= longlong10_to_str((longlong) skip_lines, pos, 10);
    pos= strmov(pos," LINES ");    
  }

  if (num_fields)
  {
    uint i;
    const char *field= fields;
    pos= strmov(pos, " (");
    for (i = 0; i < num_fields; i++)
    {
      if (i)
      {
        *pos++= ' ';
        *pos++= ',';
      }
      memcpy(pos, field, field_lens[i]);
      pos+=   field_lens[i];
      field+= field_lens[i]  + 1;
    }
    *pos++= ')';
  }

  *end= pos;
}


void Load_log_event::pack_info(Protocol *protocol)
{
  char *buf, *end;

  if (!(buf= my_malloc(get_query_buffer_length(), MYF(MY_WME))))
    return;
  print_query(TRUE, buf, &end, 0, 0);
  protocol->store(buf, end-buf, &my_charset_bin);
  my_free(buf, MYF(0));
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


#ifndef MYSQL_CLIENT

/*
  Load_log_event::write_data_header()
*/

bool Load_log_event::write_data_header(IO_CACHE* file)
{
  char buf[LOAD_HEADER_LEN];
  int4store(buf + L_THREAD_ID_OFFSET, slave_proxy_id);
  int4store(buf + L_EXEC_TIME_OFFSET, exec_time);
  int4store(buf + L_SKIP_LINES_OFFSET, skip_lines);
  buf[L_TBL_LEN_OFFSET] = (char)table_name_len;
  buf[L_DB_LEN_OFFSET] = (char)db_len;
  int4store(buf + L_NUM_FIELDS_OFFSET, num_fields);
  return my_b_safe_write(file, (byte*)buf, LOAD_HEADER_LEN) != 0;
}


/*
  Load_log_event::write_data_body()
*/

bool Load_log_event::write_data_body(IO_CACHE* file)
{
  if (sql_ex.write_data(file))
    return 1;
  if (num_fields && fields && field_lens)
  {
    if (my_b_safe_write(file, (byte*)field_lens, num_fields) ||
	my_b_safe_write(file, (byte*)fields, field_block_len))
      return 1;
  }
  return (my_b_safe_write(file, (byte*)table_name, table_name_len + 1) ||
	  my_b_safe_write(file, (byte*)db, db_len + 1) ||
	  my_b_safe_write(file, (byte*)fname, fname_len));
}


/*
  Load_log_event::Load_log_event()
*/

Load_log_event::Load_log_event(THD *thd_arg, sql_exchange *ex,
			       const char *db_arg, const char *table_name_arg,
			       List<Item> &fields_arg,
			       enum enum_duplicates handle_dup,
			       bool ignore, bool using_trans)
  :Log_event(thd_arg, !thd_arg->tmp_table_used ?
	     0 : LOG_EVENT_THREAD_SPECIFIC_F, using_trans),
   thread_id(thd_arg->thread_id),
   slave_proxy_id(thd_arg->variables.pseudo_thread_id),
   num_fields(0),fields(0),
   field_lens(0),field_block_len(0),
   table_name(table_name_arg ? table_name_arg : ""),
   db(db_arg), fname(ex->file_name), local_fname(FALSE)
{
  time_t end_time;
  time(&end_time);
  exec_time = (ulong) (end_time  - thd_arg->start_time);
  /* db can never be a zero pointer in 4.0 */
  db_len = (uint32) strlen(db);
  table_name_len = (uint32) strlen(table_name);
  fname_len = (fname) ? (uint) strlen(fname) : 0;
  sql_ex.field_term = (char*) ex->field_term->ptr();
  sql_ex.field_term_len = (uint8) ex->field_term->length();
  sql_ex.enclosed = (char*) ex->enclosed->ptr();
  sql_ex.enclosed_len = (uint8) ex->enclosed->length();
  sql_ex.line_term = (char*) ex->line_term->ptr();
  sql_ex.line_term_len = (uint8) ex->line_term->length();
  sql_ex.line_start = (char*) ex->line_start->ptr();
  sql_ex.line_start_len = (uint8) ex->line_start->length();
  sql_ex.escaped = (char*) ex->escaped->ptr();
  sql_ex.escaped_len = (uint8) ex->escaped->length();
  sql_ex.opt_flags = 0;
  sql_ex.cached_new_format = -1;
    
  if (ex->dumpfile)
    sql_ex.opt_flags|= DUMPFILE_FLAG;
  if (ex->opt_enclosed)
    sql_ex.opt_flags|= OPT_ENCLOSED_FLAG;

  sql_ex.empty_flags= 0;

  switch (handle_dup) {
  case DUP_REPLACE:
    sql_ex.opt_flags|= REPLACE_FLAG;
    break;
  case DUP_UPDATE:				// Impossible here
  case DUP_ERROR:
    break;	
  }
  if (ignore)
    sql_ex.opt_flags|= IGNORE_FLAG;

  if (!ex->field_term->length())
    sql_ex.empty_flags |= FIELD_TERM_EMPTY;
  if (!ex->enclosed->length())
    sql_ex.empty_flags |= ENCLOSED_EMPTY;
  if (!ex->line_term->length())
    sql_ex.empty_flags |= LINE_TERM_EMPTY;
  if (!ex->line_start->length())
    sql_ex.empty_flags |= LINE_START_EMPTY;
  if (!ex->escaped->length())
    sql_ex.empty_flags |= ESCAPED_EMPTY;
    
  skip_lines = ex->skip_lines;

  List_iterator<Item> li(fields_arg);
  field_lens_buf.length(0);
  fields_buf.length(0);
  Item* item;
  while ((item = li++))
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
#endif /* !MYSQL_CLIENT */


/*
  Load_log_event::Load_log_event()

  NOTE
    The caller must do buf[event_len] = 0 before he starts using the
    constructed event.
*/

Load_log_event::Load_log_event(const char *buf, uint event_len,
                               const Format_description_log_event *description_event)
  :Log_event(buf, description_event), num_fields(0), fields(0),
   field_lens(0),field_block_len(0),
   table_name(0), db(0), fname(0), local_fname(FALSE)
{
  DBUG_ENTER("Load_log_event");
  /*
    I (Guilhem) manually tested replication of LOAD DATA INFILE for 3.23->5.0,
    4.0->5.0 and 5.0->5.0 and it works.
  */
  if (event_len)
    copy_log_event(buf, event_len,
                   ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                    LOAD_HEADER_LEN + 
                    description_event->common_header_len :
                    LOAD_HEADER_LEN + LOG_EVENT_HEADER_LEN),
                   description_event);
  /* otherwise it's a derived class, will call copy_log_event() itself */
  DBUG_VOID_RETURN;
}


/*
  Load_log_event::copy_log_event()
*/

int Load_log_event::copy_log_event(const char *buf, ulong event_len,
                                   int body_offset,
                                   const Format_description_log_event *description_event)
{
  DBUG_ENTER("Load_log_event::copy_log_event");
  uint data_len;
  char* buf_end = (char*)buf + event_len;
  /* this is the beginning of the post-header */
  const char* data_head = buf + description_event->common_header_len;
  slave_proxy_id= thread_id= uint4korr(data_head + L_THREAD_ID_OFFSET);
  exec_time = uint4korr(data_head + L_EXEC_TIME_OFFSET);
  skip_lines = uint4korr(data_head + L_SKIP_LINES_OFFSET);
  table_name_len = (uint)data_head[L_TBL_LEN_OFFSET];
  db_len = (uint)data_head[L_DB_LEN_OFFSET];
  num_fields = uint4korr(data_head + L_NUM_FIELDS_OFFSET);
	  
  if ((int) event_len < body_offset)
    DBUG_RETURN(1);
  /*
    Sql_ex.init() on success returns the pointer to the first byte after
    the sql_ex structure, which is the start of field lengths array.
  */
  if (!(field_lens= (uchar*)sql_ex.init((char*)buf + body_offset,
                                        buf_end,
                                        buf[EVENT_TYPE_OFFSET] != LOAD_EVENT)))
    DBUG_RETURN(1);
  
  data_len = event_len - body_offset;
  if (num_fields > data_len) // simple sanity check against corruption
    DBUG_RETURN(1);
  for (uint i = 0; i < num_fields; i++)
    field_block_len += (uint)field_lens[i] + 1;

  fields = (char*)field_lens + num_fields;
  table_name  = fields + field_block_len;
  db = table_name + table_name_len + 1;
  fname = db + db_len + 1;
  fname_len = strlen(fname);
  // null termination is accomplished by the caller doing buf[event_len]=0

  DBUG_RETURN(0);
}


/*
  Load_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Load_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  print(file, print_event_info, 0);
}


void Load_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info,
			   bool commented)
{
  DBUG_ENTER("Load_log_event::print");
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tQuery\tthread_id=%ld\texec_time=%ld\n",
	    thread_id, exec_time);
  }

  bool different_db= 1;
  if (db)
  {
    /*
      If the database is different from the one of the previous statement, we
      need to print the "use" command, and we update the last_db.
      But if commented, the "use" is going to be commented so we should not
      update the last_db.
    */
    if ((different_db= memcmp(print_event_info->db, db, db_len + 1)) &&
        !commented)
      memcpy(print_event_info->db, db, db_len + 1);
  }
  
  if (db && db[0] && different_db)
    fprintf(file, "%suse %s;\n", 
            commented ? "# " : "",
            db);

  if (flags & LOG_EVENT_THREAD_SPECIFIC_F)
    fprintf(file,"%sSET @@session.pseudo_thread_id=%lu;\n",
            commented ? "# " : "", (ulong)thread_id);
  fprintf(file, "%sLOAD DATA ",
          commented ? "# " : "");
  if (check_fname_outside_temp_buf())
    fprintf(file, "LOCAL ");
  fprintf(file, "INFILE '%-*s' ", fname_len, fname);

  if (sql_ex.opt_flags & REPLACE_FLAG)
    fprintf(file," REPLACE ");
  else if (sql_ex.opt_flags & IGNORE_FLAG)
    fprintf(file," IGNORE ");
  
  fprintf(file, "INTO TABLE `%s`", table_name);
  fprintf(file, " FIELDS TERMINATED BY ");
  pretty_print_str(file, sql_ex.field_term, sql_ex.field_term_len);

  if (sql_ex.opt_flags & OPT_ENCLOSED_FLAG)
    fprintf(file," OPTIONALLY ");
  fprintf(file, " ENCLOSED BY ");
  pretty_print_str(file, sql_ex.enclosed, sql_ex.enclosed_len);
     
  fprintf(file, " ESCAPED BY ");
  pretty_print_str(file, sql_ex.escaped, sql_ex.escaped_len);
     
  fprintf(file," LINES TERMINATED BY ");
  pretty_print_str(file, sql_ex.line_term, sql_ex.line_term_len);


  if (sql_ex.line_start)
  {
    fprintf(file," STARTING BY ");
    pretty_print_str(file, sql_ex.line_start, sql_ex.line_start_len);
  }
  if ((long) skip_lines > 0)
    fprintf(file, " IGNORE %ld LINES", (long) skip_lines);

  if (num_fields)
  {
    uint i;
    const char* field = fields;
    fprintf(file, " (");
    for (i = 0; i < num_fields; i++)
    {
      if (i)
	fputc(',', file);
      fprintf(file, field);
	  
      field += field_lens[i]  + 1;
    }
    fputc(')', file);
  }

  fprintf(file, ";\n");
  DBUG_VOID_RETURN;
}
#endif /* MYSQL_CLIENT */


/*
  Load_log_event::set_fields()

  Note that this function can not use the member variable 
  for the database, since LOAD DATA INFILE on the slave
  can be for a different database than the current one.
  This is the reason for the affected_db argument to this method.
*/

#ifndef MYSQL_CLIENT
void Load_log_event::set_fields(const char* affected_db, 
				List<Item> &field_list,
                                Name_resolution_context *context)
{
  uint i;
  const char* field = fields;
  for (i= 0; i < num_fields; i++)
  {
    field_list.push_back(new Item_field(context,
                                        affected_db, table_name, field));
    field+= field_lens[i]  + 1;
  }
}
#endif /* !MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
/*
  Does the data loading job when executing a LOAD DATA on the slave

  SYNOPSIS
    Load_log_event::exec_event
      net  
      rli                             
      use_rli_only_for_errors	  - if set to 1, rli is provided to 
                                  Load_log_event::exec_event only for this 
				  function to have RPL_LOG_NAME and 
				  rli->last_slave_error, both being used by 
				  error reports. rli's position advancing
				  is skipped (done by the caller which is
				  Execute_load_log_event::exec_event).
				  - if set to 0, rli is provided for full use,
				  i.e. for error reports and position
				  advancing.

  DESCRIPTION
    Does the data loading job when executing a LOAD DATA on the slave
 
  RETURN VALUE
    0           Success                                                 
    1    	Failure
*/

int Load_log_event::exec_event(NET* net, struct st_relay_log_info* rli, 
			       bool use_rli_only_for_errors)
{
  LEX_STRING new_db;
  new_db.length= db_len;
  new_db.str= (char *) rpl_filter->get_rewrite_db(db, &new_db.length);
  thd->set_db(new_db.str, new_db.length);
  DBUG_ASSERT(thd->query == 0);
  thd->query_length= 0;                         // Should not be needed
  thd->query_error= 0;
  clear_all_errors(thd, rli);

  /* see Query_log_event::exec_event() and BUG#13360 */
  DBUG_ASSERT(!rli->m_table_map.count());
  /*
    Usually mysql_init_query() is called by mysql_parse(), but we need it here
    as the present method does not call mysql_parse().
  */
  mysql_init_query(thd, 0, 0);
  if (!use_rli_only_for_errors)
  {
    /* Saved for InnoDB, see comment in Query_log_event::exec_event() */
    rli->future_group_master_log_pos= log_pos;
    DBUG_PRINT("info", ("log_pos: %lu", (ulong) log_pos));
  }
 
   /*
    We test replicate_*_db rules. Note that we have already prepared the file
    to load, even if we are going to ignore and delete it now. So it is
    possible that we did a lot of disk writes for nothing. In other words, a
    big LOAD DATA INFILE on the master will still consume a lot of space on
    the slave (space in the relay log + space of temp files: twice the space
    of the file to load...) even if it will finally be ignored.
    TODO: fix this; this can be done by testing rules in
    Create_file_log_event::exec_event() and then discarding Append_block and
    al. Another way is do the filtering in the I/O thread (more efficient: no
    disk writes at all).


    Note:   We do not need to execute reset_one_shot_variables() if this
            db_ok() test fails.
    Reason: The db stored in binlog events is the same for SET and for
            its companion query.  If the SET is ignored because of
            db_ok(), the companion query will also be ignored, and if
            the companion query is ignored in the db_ok() test of
            ::exec_event(), then the companion SET also have so we
            don't need to reset_one_shot_variables().
  */
  if (rpl_filter->db_ok(thd->db))
  {
    thd->set_time((time_t)when);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    thd->query_id = next_query_id();
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    /*
      Initing thd->row_count is not necessary in theory as this variable has no
      influence in the case of the slave SQL thread (it is used to generate a
      "data truncated" warning but which is absorbed and never gets to the
      error log); still we init it to avoid a Valgrind message.
    */
    mysql_reset_errors(thd, 0);

    TABLE_LIST tables;
    bzero((char*) &tables,sizeof(tables));
    tables.db= thd->strmake(thd->db, thd->db_length);
    tables.alias = tables.table_name = (char*) table_name;
    tables.lock_type = TL_WRITE;
    tables.updating= 1;

    // the table will be opened in mysql_load    
    if (rpl_filter->is_on() && !rpl_filter->tables_ok(thd->db, &tables))
    {
      // TODO: this is a bug - this needs to be moved to the I/O thread
      if (net)
        skip_load_data_infile(net);
    }
    else
    {
      char llbuff[22];
      char *end;
      enum enum_duplicates handle_dup;
      bool ignore= 0;
      char *load_data_query;

      /*
        Forge LOAD DATA INFILE query which will be used in SHOW PROCESS LIST
        and written to slave's binlog if binlogging is on.
      */
      if (!(load_data_query= (char *)thd->alloc(get_query_buffer_length() + 1)))
      {
        /*
          This will set thd->fatal_error in case of OOM. So we surely will notice
          that something is wrong.
        */
        goto error;
      }

      print_query(FALSE, load_data_query, &end, (char **)&thd->lex->fname_start,
                  (char **)&thd->lex->fname_end);
      *end= 0;
      thd->query_length= end - load_data_query;
      thd->query= load_data_query;

      if (sql_ex.opt_flags & REPLACE_FLAG)
      {
	handle_dup= DUP_REPLACE;
      }
      else if (sql_ex.opt_flags & IGNORE_FLAG)
      {
        ignore= 1;
        handle_dup= DUP_ERROR;
      }
      else
      {
        /*
	  When replication is running fine, if it was DUP_ERROR on the
          master then we could choose IGNORE here, because if DUP_ERROR
          suceeded on master, and data is identical on the master and slave,
          then there should be no uniqueness errors on slave, so IGNORE is
          the same as DUP_ERROR. But in the unlikely case of uniqueness errors
          (because the data on the master and slave happen to be different
	  (user error or bug), we want LOAD DATA to print an error message on
	  the slave to discover the problem.

          If reading from net (a 3.23 master), mysql_load() will change this
          to IGNORE.
        */
        handle_dup= DUP_ERROR;
      }
      /*
        We need to set thd->lex->sql_command and thd->lex->duplicates
        since InnoDB tests these variables to decide if this is a LOAD
        DATA ... REPLACE INTO ... statement even though mysql_parse()
        is not called.  This is not needed in 5.0 since there the LOAD
        DATA ... statement is replicated using mysql_parse(), which
        sets the thd->lex fields correctly.
      */
      thd->lex->sql_command= SQLCOM_LOAD;
      thd->lex->duplicates= handle_dup;

      sql_exchange ex((char*)fname, sql_ex.opt_flags & DUMPFILE_FLAG);
      String field_term(sql_ex.field_term,sql_ex.field_term_len,log_cs);
      String enclosed(sql_ex.enclosed,sql_ex.enclosed_len,log_cs);
      String line_term(sql_ex.line_term,sql_ex.line_term_len,log_cs);
      String line_start(sql_ex.line_start,sql_ex.line_start_len,log_cs);
      String escaped(sql_ex.escaped,sql_ex.escaped_len, log_cs);
      ex.field_term= &field_term;
      ex.enclosed= &enclosed;
      ex.line_term= &line_term;
      ex.line_start= &line_start;
      ex.escaped= &escaped;

      ex.opt_enclosed = (sql_ex.opt_flags & OPT_ENCLOSED_FLAG);
      if (sql_ex.empty_flags & FIELD_TERM_EMPTY)
	ex.field_term->length(0);

      ex.skip_lines = skip_lines;
      List<Item> field_list;
      thd->main_lex.select_lex.context.resolve_in_table_list_only(&tables);
      set_fields(tables.db, field_list, &thd->main_lex.select_lex.context);
      thd->variables.pseudo_thread_id= thread_id;
      List<Item> set_fields;
      if (net)
      {
	// mysql_load will use thd->net to read the file
	thd->net.vio = net->vio;
	/*
	  Make sure the client does not get confused about the packet sequence
	*/
	thd->net.pkt_nr = net->pkt_nr;
      }
      /*
        It is safe to use set_fields twice because we are not going to
        update it inside mysql_load().
      */
      if (mysql_load(thd, &ex, &tables, field_list, set_fields, set_fields,
                     handle_dup, ignore, net != 0))
        thd->query_error= 1;
      if (thd->cuted_fields)
      {
	/* log_pos is the position of the LOAD event in the master log */
        sql_print_warning("Slave: load data infile on table '%s' at "
                          "log position %s in log '%s' produced %ld "
                          "warning(s). Default database: '%s'",
                          (char*) table_name,
                          llstr(log_pos,llbuff), RPL_LOG_NAME, 
                          (ulong) thd->cuted_fields,
                          print_slave_db_safe(thd->db));
      }
      if (net)
        net->pkt_nr= thd->net.pkt_nr;
    }
  }
  else
  {
    /*
      We will just ask the master to send us /dev/null if we do not
      want to load the data.
      TODO: this a bug - needs to be done in I/O thread
    */
    if (net)
      skip_load_data_infile(net);
  }

error:
  thd->net.vio = 0; 
  const char *remember_db= thd->db;
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->catalog= 0;
  thd->set_db(NULL, 0);                   /* will free the current database */
  thd->query= 0;
  thd->query_length= 0;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  close_thread_tables(thd);
  if (thd->query_error)
  {
    /* this err/sql_errno code is copy-paste from net_send_error() */
    const char *err;
    int sql_errno;
    if ((err=thd->net.last_error)[0])
      sql_errno=thd->net.last_errno;
    else
    {
      sql_errno=ER_UNKNOWN_ERROR;
      err=ER(sql_errno);       
    }
    slave_print_msg(ERROR_LEVEL, rli, sql_errno,"\
Error '%s' running LOAD DATA INFILE on table '%s'. Default database: '%s'",
                    err, (char*)table_name, print_slave_db_safe(remember_db));
    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    return 1;
  }
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
	    
  if (thd->is_fatal_error)
  {
    slave_print_msg(ERROR_LEVEL, rli, ER_UNKNOWN_ERROR, "\
Fatal error running LOAD DATA INFILE on table '%s'. Default database: '%s'",
                    (char*)table_name, print_slave_db_safe(remember_db));
    return 1;
  }

  return ( use_rli_only_for_errors ? 0 : Log_event::exec_event(rli) ); 
}
#endif


/**************************************************************************
  Rotate_log_event methods
**************************************************************************/

/*
  Rotate_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Rotate_log_event::pack_info(Protocol *protocol)
{
  char buf1[256], buf[22];
  String tmp(buf1, sizeof(buf1), log_cs);
  tmp.length(0);
  tmp.append(new_log_ident, ident_len);
  tmp.append(STRING_WITH_LEN(";pos="));
  tmp.append(llstr(pos,buf));
  protocol->store(tmp.ptr(), tmp.length(), &my_charset_bin);
}
#endif


/*
  Rotate_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Rotate_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char buf[22];

  if (print_event_info->short_form)
    return;
  print_header(file, print_event_info);
  fprintf(file, "\tRotate to ");
  if (new_log_ident)
    my_fwrite(file, (byte*) new_log_ident, (uint)ident_len, 
              MYF(MY_NABP | MY_WME));
  fprintf(file, "  pos: %s", llstr(pos, buf));
  fputc('\n', file);
  fflush(file);
}
#endif /* MYSQL_CLIENT */



/*
  Rotate_log_event::Rotate_log_event() (2 constructors)
*/


#ifndef MYSQL_CLIENT
Rotate_log_event::Rotate_log_event(const char* new_log_ident_arg,
                                   uint ident_len_arg, ulonglong pos_arg,
                                   uint flags_arg)
  :Log_event(), new_log_ident(new_log_ident_arg),
   pos(pos_arg),ident_len(ident_len_arg ? ident_len_arg :
                          (uint) strlen(new_log_ident_arg)), flags(flags_arg)
{
#ifndef DBUG_OFF
  char buff[22];
  DBUG_ENTER("Rotate_log_event::Rotate_log_event(...,flags)");
  DBUG_PRINT("enter",("new_log_ident %s pos %s flags %lu", new_log_ident_arg,
                      llstr(pos_arg, buff), flags));
#endif
  if (flags & DUP_NAME)
    new_log_ident= my_strndup(new_log_ident_arg, ident_len, MYF(MY_WME));
  DBUG_VOID_RETURN;
}
#endif


Rotate_log_event::Rotate_log_event(const char* buf, uint event_len,
                                   const Format_description_log_event* description_event)
  :Log_event(buf, description_event) ,new_log_ident(0), flags(DUP_NAME)
{
  DBUG_ENTER("Rotate_log_event::Rotate_log_event(char*,...)");
  // The caller will ensure that event_len is what we have at EVENT_LEN_OFFSET
  uint8 header_size= description_event->common_header_len;
  uint8 post_header_len= description_event->post_header_len[ROTATE_EVENT-1];
  uint ident_offset;
  if (event_len < header_size)
    DBUG_VOID_RETURN;
  buf += header_size;
  pos = post_header_len ? uint8korr(buf + R_POS_OFFSET) : 4;
  ident_len = (uint)(event_len -
                     (header_size+post_header_len)); 
  ident_offset = post_header_len; 
  set_if_smaller(ident_len,FN_REFLEN-1);
  new_log_ident= my_strndup(buf + ident_offset, (uint) ident_len, MYF(MY_WME));
  DBUG_VOID_RETURN;
}


/*
  Rotate_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Rotate_log_event::write(IO_CACHE* file)
{
  char buf[ROTATE_HEADER_LEN];
  int8store(buf + R_POS_OFFSET, pos);
  return (write_header(file, ROTATE_HEADER_LEN + ident_len) ||
          my_b_safe_write(file, (byte*)buf, ROTATE_HEADER_LEN) ||
          my_b_safe_write(file, (byte*)new_log_ident, (uint) ident_len));
}
#endif

/*
  Rotate_log_event::exec_event()

  Got a rotate log event from the master

  IMPLEMENTATION
    This is mainly used so that we can later figure out the logname and
    position for the master.

    We can't rotate the slave's BINlog as this will cause infinitive rotations
    in a A -> B -> A setup.
    The NOTES below is a wrong comment which will disappear when 4.1 is merged.

  RETURN VALUES
    0	ok
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Rotate_log_event::exec_event(struct st_relay_log_info* rli)
{
  DBUG_ENTER("Rotate_log_event::exec_event");

  pthread_mutex_lock(&rli->data_lock);
  rli->event_relay_log_pos= my_b_tell(rli->cur_log);
  /*
    If we are in a transaction: the only normal case is when the I/O thread was
    copying a big transaction, then it was stopped and restarted: we have this
    in the relay log:
    BEGIN
    ...
    ROTATE (a fake one)
    ...
    COMMIT or ROLLBACK
    In that case, we don't want to touch the coordinates which correspond to
    the beginning of the transaction.
    Starting from 5.0.0, there also are some rotates from the slave itself, in
    the relay log.
  */
  if (!(thd->options & OPTION_BEGIN))
  {
    memcpy(rli->group_master_log_name, new_log_ident, ident_len+1);
    rli->notify_group_master_log_name_update();
    rli->group_master_log_pos= pos;
    rli->group_relay_log_pos= rli->event_relay_log_pos;
    DBUG_PRINT("info", ("group_master_log_name: '%s'  "
                        "group_master_log_pos: %lu",
                        rli->group_master_log_name,
                        (ulong) rli->group_master_log_pos));
    /*
      Reset thd->options and sql_mode etc, because this could be the signal of
      a master's downgrade from 5.0 to 4.0.
      However, no need to reset description_event_for_exec: indeed, if the next
      master is 5.0 (even 5.0.1) we will soon get a Format_desc; if the next
      master is 4.0 then the events are in the slave's format (conversion).
    */
    set_slave_thread_options(thd);
    set_slave_thread_default_charset(thd, rli);
    thd->variables.sql_mode= global_system_variables.sql_mode;
    thd->variables.auto_increment_increment=
      thd->variables.auto_increment_offset= 1;
  }
  pthread_mutex_unlock(&rli->data_lock);
  pthread_cond_broadcast(&rli->data_cond);
  flush_relay_log_info(rli);
  DBUG_RETURN(0);
}
#endif


/**************************************************************************
	Intvar_log_event methods
**************************************************************************/

/*
  Intvar_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Intvar_log_event::pack_info(Protocol *protocol)
{
  char buf[256], *pos;
  pos= strmake(buf, get_var_type_name(), sizeof(buf)-23);
  *pos++= '=';
  pos= longlong10_to_str(val, pos, -10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}
#endif


/*
  Intvar_log_event::Intvar_log_event()
*/

Intvar_log_event::Intvar_log_event(const char* buf,
                                   const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  buf+= description_event->common_header_len;
  type= buf[I_TYPE_OFFSET];
  val= uint8korr(buf+I_VAL_OFFSET);
}


/*
  Intvar_log_event::get_var_type_name()
*/

const char* Intvar_log_event::get_var_type_name()
{
  switch(type) {
  case LAST_INSERT_ID_EVENT: return "LAST_INSERT_ID";
  case INSERT_ID_EVENT: return "INSERT_ID";
  default: /* impossible */ return "UNKNOWN";
  }
}


/*
  Intvar_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Intvar_log_event::write(IO_CACHE* file)
{
  byte buf[9];
  buf[I_TYPE_OFFSET]= (byte) type;
  int8store(buf + I_VAL_OFFSET, val);
  return (write_header(file, sizeof(buf)) ||
          my_b_safe_write(file, buf, sizeof(buf)));
}
#endif


/*
  Intvar_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Intvar_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char llbuff[22];
  const char *msg;
  LINT_INIT(msg);

  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tIntvar\n");
  }

  fprintf(file, "SET ");
  switch (type) {
  case LAST_INSERT_ID_EVENT:
    msg="LAST_INSERT_ID";
    break;
  case INSERT_ID_EVENT:
    msg="INSERT_ID";
    break;
  case INVALID_INT_EVENT:
  default: // cannot happen
    msg="INVALID_INT";
    break;
  }
  fprintf(file, "%s=%s;\n", msg, llstr(val,llbuff));
  fflush(file);
}
#endif


/*
  Intvar_log_event::exec_event()
*/

#if defined(HAVE_REPLICATION)&& !defined(MYSQL_CLIENT)
int Intvar_log_event::exec_event(struct st_relay_log_info* rli)
{
  switch (type) {
  case LAST_INSERT_ID_EVENT:
    thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 1;
    thd->first_successful_insert_id_in_prev_stmt= val;
    break;
  case INSERT_ID_EVENT:
    thd->force_one_auto_inc_interval(val);
    break;
  }
  rli->inc_event_relay_log_pos();
  return 0;
}
#endif


/**************************************************************************
  Rand_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Rand_log_event::pack_info(Protocol *protocol)
{
  char buf1[256], *pos;
  pos= strmov(buf1,"rand_seed1=");
  pos= int10_to_str((long) seed1, pos, 10);
  pos= strmov(pos, ",rand_seed2=");
  pos= int10_to_str((long) seed2, pos, 10);
  protocol->store(buf1, (uint) (pos-buf1), &my_charset_bin);
}
#endif


Rand_log_event::Rand_log_event(const char* buf,
                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  buf+= description_event->common_header_len;
  seed1= uint8korr(buf+RAND_SEED1_OFFSET);
  seed2= uint8korr(buf+RAND_SEED2_OFFSET);
}


#ifndef MYSQL_CLIENT
bool Rand_log_event::write(IO_CACHE* file)
{
  byte buf[16];
  int8store(buf + RAND_SEED1_OFFSET, seed1);
  int8store(buf + RAND_SEED2_OFFSET, seed2);
  return (write_header(file, sizeof(buf)) ||
          my_b_safe_write(file, buf, sizeof(buf)));
}
#endif


#ifdef MYSQL_CLIENT
void Rand_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char llbuff[22],llbuff2[22];
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tRand\n");
  }
  fprintf(file, "SET @@RAND_SEED1=%s, @@RAND_SEED2=%s;\n",
	  llstr(seed1, llbuff),llstr(seed2, llbuff2));
  fflush(file);
}
#endif /* MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Rand_log_event::exec_event(struct st_relay_log_info* rli)
{
  thd->rand.seed1= (ulong) seed1;
  thd->rand.seed2= (ulong) seed2;
  rli->inc_event_relay_log_pos();
  return 0;
}
#endif /* !MYSQL_CLIENT */


/**************************************************************************
  Xid_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Xid_log_event::pack_info(Protocol *protocol)
{
  char buf[128], *pos;
  pos= strmov(buf, "COMMIT /* xid=");
  pos= longlong10_to_str(xid, pos, 10);
  pos= strmov(pos, " */");
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}
#endif

/*
  NOTE it's ok not to use int8store here,
  as long as xid_t::set(ulonglong) and
  xid_t::get_my_xid doesn't do it either

  we don't care about actual values of xids as long as
  identical numbers compare identically
*/

Xid_log_event::
Xid_log_event(const char* buf,
              const Format_description_log_event *description_event)
  :Log_event(buf, description_event)
{
  buf+= description_event->common_header_len;
  memcpy((char*) &xid, buf, sizeof(xid));
}


#ifndef MYSQL_CLIENT
bool Xid_log_event::write(IO_CACHE* file)
{
  return write_header(file, sizeof(xid)) ||
         my_b_safe_write(file, (byte*) &xid, sizeof(xid));
}
#endif


#ifdef MYSQL_CLIENT
void Xid_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  if (!print_event_info->short_form)
  {
    char buf[64];
    longlong10_to_str(xid, buf, 10);

    print_header(file, print_event_info);
    fprintf(file, "\tXid = %s\n", buf);
    fflush(file);
  }
  fprintf(file, "COMMIT;\n");
}
#endif /* MYSQL_CLIENT */


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Xid_log_event::exec_event(struct st_relay_log_info* rli)
{
  /* For a slave Xid_log_event is COMMIT */
  general_log_print(thd, COM_QUERY,
                    "COMMIT /* implicit, from Xid_log_event */");
  return end_trans(thd, COMMIT) || Log_event::exec_event(rli);
}
#endif /* !MYSQL_CLIENT */


/**************************************************************************
  User_var_log_event methods
**************************************************************************/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void User_var_log_event::pack_info(Protocol* protocol)
{
  char *buf= 0;
  uint val_offset= 4 + name_len;
  uint event_len= val_offset;

  if (is_null)
  {
    buf= my_malloc(val_offset + 5, MYF(MY_WME));
    strmov(buf + val_offset, "NULL");
    event_len= val_offset + 4;
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
      double real_val;
      float8get(real_val, val);
      buf= my_malloc(val_offset + FLOATING_POINT_BUFFER, MYF(MY_WME));
      event_len+= my_sprintf(buf + val_offset,
			     (buf + val_offset, "%.14g", real_val));
      break;
    case INT_RESULT:
      buf= my_malloc(val_offset + 22, MYF(MY_WME));
      event_len= longlong10_to_str(uint8korr(val), buf + val_offset,-10)-buf;
      break;
    case DECIMAL_RESULT:
    {
      buf= my_malloc(val_offset + DECIMAL_MAX_STR_LENGTH, MYF(MY_WME));
      String str(buf+val_offset, DECIMAL_MAX_STR_LENGTH, &my_charset_bin);
      my_decimal dec;
      binary2my_decimal(E_DEC_FATAL_ERROR, val+2, &dec, val[0], val[1]);
      my_decimal2string(E_DEC_FATAL_ERROR, &dec, 0, 0, 0, &str);
      event_len= str.length() + val_offset;
      break;
    } 
    case STRING_RESULT:
      /* 15 is for 'COLLATE' and other chars */
      buf= my_malloc(event_len+val_len*2+1+2*MY_CS_NAME_SIZE+15, MYF(MY_WME));
      CHARSET_INFO *cs;
      if (!(cs= get_charset(charset_number, MYF(0))))
      {
        strmov(buf+val_offset, "???");
        event_len+= 3;
      }
      else
      {
        char *p= strxmov(buf + val_offset, "_", cs->csname, " ", NullS);
        p= str_to_hex(p, val, val_len);
        p= strxmov(p, " COLLATE ", cs->name, NullS);
        event_len= p-buf;
      }
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      return;
    }
  }
  buf[0]= '@';
  buf[1]= '`';
  buf[2+name_len]= '`';
  buf[3+name_len]= '=';
  memcpy(buf+2, name, name_len);
  protocol->store(buf, event_len, &my_charset_bin);
  my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
}
#endif /* !MYSQL_CLIENT */


User_var_log_event::
User_var_log_event(const char* buf,
                   const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  buf+= description_event->common_header_len;
  name_len= uint4korr(buf);
  name= (char *) buf + UV_NAME_LEN_SIZE;
  buf+= UV_NAME_LEN_SIZE + name_len;
  is_null= (bool) *buf;
  if (is_null)
  {
    type= STRING_RESULT;
    charset_number= my_charset_bin.number;
    val_len= 0;
    val= 0;  
  }
  else
  {
    type= (Item_result) buf[UV_VAL_IS_NULL];
    charset_number= uint4korr(buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE);
    val_len= uint4korr(buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE + 
		       UV_CHARSET_NUMBER_SIZE);
    val= (char *) (buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
		   UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE);
  }
}


#ifndef MYSQL_CLIENT
bool User_var_log_event::write(IO_CACHE* file)
{
  char buf[UV_NAME_LEN_SIZE];
  char buf1[UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE + 
	    UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE];
  char buf2[max(8, DECIMAL_MAX_FIELD_SIZE + 2)], *pos= buf2;
  uint buf1_length;
  ulong event_length;

  int4store(buf, name_len);
  
  if ((buf1[0]= is_null))
  {
    buf1_length= 1;
    val_len= 0;
  }    
  else
  {
    buf1[1]= type;
    int4store(buf1 + 2, charset_number);

    switch (type) {
    case REAL_RESULT:
      float8store(buf2, *(double*) val);
      break;
    case INT_RESULT:
      int8store(buf2, *(longlong*) val);
      break;
    case DECIMAL_RESULT:
    {
      my_decimal *dec= (my_decimal *)val;
      dec->fix_buffer_pointer();
      buf2[0]= (char)(dec->intg + dec->frac);
      buf2[1]= (char)dec->frac;
      decimal2bin((decimal_t*)val, buf2+2, buf2[0], buf2[1]);
      val_len= decimal_bin_size(buf2[0], buf2[1]) + 2;
      break;
    }
    case STRING_RESULT:
      pos= val;
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      return 0;
    }
    int4store(buf1 + 2 + UV_CHARSET_NUMBER_SIZE, val_len);
    buf1_length= 10;
  }

  /* Length of the whole event */
  event_length= sizeof(buf)+ name_len + buf1_length + val_len;

  return (write_header(file, event_length) ||
          my_b_safe_write(file, (byte*) buf, sizeof(buf))   ||
	  my_b_safe_write(file, (byte*) name, name_len)     ||
	  my_b_safe_write(file, (byte*) buf1, buf1_length) ||
	  my_b_safe_write(file, (byte*) pos, val_len));
}
#endif


/*
  User_var_log_event::print()
*/

#ifdef MYSQL_CLIENT
void User_var_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tUser_var\n");
  }

  fprintf(file, "SET @`");
  my_fwrite(file, (byte*) name, (uint) (name_len), MYF(MY_NABP | MY_WME));
  fprintf(file, "`");

  if (is_null)
  {
    fprintf(file, ":=NULL;\n");
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
      double real_val;
      float8get(real_val, val);
      fprintf(file, ":=%.14g;\n", real_val);
      break;
    case INT_RESULT:
      char int_buf[22];
      longlong10_to_str(uint8korr(val), int_buf, -10);
      fprintf(file, ":=%s;\n", int_buf);
      break;
    case DECIMAL_RESULT:
    {
      char str_buf[200];
      int str_len= sizeof(str_buf) - 1;
      int precision= (int)val[0];
      int scale= (int)val[1];
      decimal_digit_t dec_buf[10];
      decimal_t dec;
      dec.len= 10;
      dec.buf= dec_buf;

      bin2decimal(val+2, &dec, precision, scale);
      decimal2string(&dec, str_buf, &str_len, 0, 0, 0);
      str_buf[str_len]= 0;
      fprintf(file, ":=%s;\n",str_buf);
      break;
    }
    case STRING_RESULT:
    {
      /*
        Let's express the string in hex. That's the most robust way. If we
        print it in character form instead, we need to escape it with
        character_set_client which we don't know (we will know it in 5.0, but
        in 4.1 we don't know it easily when we are printing
        User_var_log_event). Explanation why we would need to bother with
        character_set_client (quoting Bar):
        > Note, the parser doesn't switch to another unescaping mode after
        > it has met a character set introducer.
        > For example, if an SJIS client says something like:
        > SET @a= _ucs2 \0a\0b'
        > the string constant is still unescaped according to SJIS, not
        > according to UCS2.
      */
      char *hex_str;
      CHARSET_INFO *cs;

      if (!(hex_str= (char *)my_alloca(2*val_len+1+2))) // 2 hex digits / byte
        break; // no error, as we are 'void'
      str_to_hex(hex_str, val, val_len);
      /*
        For proper behaviour when mysqlbinlog|mysql, we need to explicitely
        specify the variable's collation. It will however cause problems when
        people want to mysqlbinlog|mysql into another server not supporting the
        character set. But there's not much to do about this and it's unlikely.
      */
      if (!(cs= get_charset(charset_number, MYF(0))))
        /*
          Generate an unusable command (=> syntax error) is probably the best
          thing we can do here.
        */
        fprintf(file, ":=???;\n");
      else
        fprintf(file, ":=_%s %s COLLATE `%s`;\n", cs->csname, hex_str, cs->name);
      my_afree(hex_str);
    }
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      return;
    }
  }
  fflush(file);
}
#endif


/*
  User_var_log_event::exec_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int User_var_log_event::exec_event(struct st_relay_log_info* rli)
{
  Item *it= 0;
  CHARSET_INFO *charset;
  if (!(charset= get_charset(charset_number, MYF(MY_WME))))
    return 1;
  LEX_STRING user_var_name;
  user_var_name.str= name;
  user_var_name.length= name_len;
  double real_val;
  longlong int_val;

  if (is_null)
  {
    it= new Item_null();
  }
  else
  {
    switch (type) {
    case REAL_RESULT:
      float8get(real_val, val);
      it= new Item_float(real_val);
      val= (char*) &real_val;		// Pointer to value in native format
      val_len= 8;
      break;
    case INT_RESULT:
      int_val= (longlong) uint8korr(val);
      it= new Item_int(int_val);
      val= (char*) &int_val;		// Pointer to value in native format
      val_len= 8;
      break;
    case DECIMAL_RESULT:
    {
      Item_decimal *dec= new Item_decimal(val+2, val[0], val[1]);
      it= dec;
      val= (char *)dec->val_decimal(NULL);
      val_len= sizeof(my_decimal);
      break;
    }
    case STRING_RESULT:
      it= new Item_string(val, val_len, charset);
      break;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(1);
      return 0;
    }
  }
  Item_func_set_user_var e(user_var_name, it);
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  e.fix_fields(thd, 0);
  /*
    A variable can just be considered as a table with
    a single record and with a single column. Thus, like
    a column value, it could always have IMPLICIT derivation.
   */
  e.update_hash(val, val_len, type, charset, DERIVATION_IMPLICIT);
  free_root(thd->mem_root,0);

  rli->inc_event_relay_log_pos();
  return 0;
}
#endif /* !MYSQL_CLIENT */


/**************************************************************************
  Slave_log_event methods
**************************************************************************/

#ifdef HAVE_REPLICATION
#ifdef MYSQL_CLIENT
void Unknown_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(file, print_event_info);
  fputc('\n', file);
  fprintf(file, "# %s", "Unknown event\n");
}
#endif  

#ifndef MYSQL_CLIENT
void Slave_log_event::pack_info(Protocol *protocol)
{
  char buf[256+HOSTNAME_LENGTH], *pos;
  pos= strmov(buf, "host=");
  pos= strnmov(pos, master_host, HOSTNAME_LENGTH);
  pos= strmov(pos, ",port=");
  pos= int10_to_str((long) master_port, pos, 10);
  pos= strmov(pos, ",log=");
  pos= strmov(pos, master_log);
  pos= strmov(pos, ",pos=");
  pos= longlong10_to_str(master_pos, pos, 10);
  protocol->store(buf, pos-buf, &my_charset_bin);
}
#endif /* !MYSQL_CLIENT */


#ifndef MYSQL_CLIENT
Slave_log_event::Slave_log_event(THD* thd_arg,
				 struct st_relay_log_info* rli)
  :Log_event(thd_arg, 0, 0) , mem_pool(0), master_host(0)
{
  DBUG_ENTER("Slave_log_event");
  if (!rli->inited)				// QQ When can this happen ?
    DBUG_VOID_RETURN;

  MASTER_INFO* mi = rli->mi;
  // TODO: re-write this better without holding both locks at the same time
  pthread_mutex_lock(&mi->data_lock);
  pthread_mutex_lock(&rli->data_lock);
  master_host_len = strlen(mi->host);
  master_log_len = strlen(rli->group_master_log_name);
  // on OOM, just do not initialize the structure and print the error
  if ((mem_pool = (char*)my_malloc(get_data_size() + 1,
				   MYF(MY_WME))))
  {
    master_host = mem_pool + SL_MASTER_HOST_OFFSET ;
    memcpy(master_host, mi->host, master_host_len + 1);
    master_log = master_host + master_host_len + 1;
    memcpy(master_log, rli->group_master_log_name, master_log_len + 1);
    master_port = mi->port;
    master_pos = rli->group_master_log_pos;
    DBUG_PRINT("info", ("master_log: %s  pos: %d", master_log,
			(ulong) master_pos));
  }
  else
    sql_print_error("Out of memory while recording slave event");
  pthread_mutex_unlock(&rli->data_lock);
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_VOID_RETURN;
}
#endif /* !MYSQL_CLIENT */


Slave_log_event::~Slave_log_event()
{
  my_free(mem_pool, MYF(MY_ALLOW_ZERO_PTR));
}


#ifdef MYSQL_CLIENT
void Slave_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  char llbuff[22];
  if (print_event_info->short_form)
    return;
  print_header(file, print_event_info);
  fputc('\n', file);
  fprintf(file, "\
Slave: master_host: '%s'  master_port: %d  master_log: '%s'  master_pos: %s\n",
	  master_host, master_port, master_log, llstr(master_pos, llbuff));
}
#endif /* MYSQL_CLIENT */


int Slave_log_event::get_data_size()
{
  return master_host_len + master_log_len + 1 + SL_MASTER_HOST_OFFSET;
}


#ifndef MYSQL_CLIENT
bool Slave_log_event::write(IO_CACHE* file)
{
  ulong event_length= get_data_size();
  int8store(mem_pool + SL_MASTER_POS_OFFSET, master_pos);
  int2store(mem_pool + SL_MASTER_PORT_OFFSET, master_port);
  // log and host are already there

  return (write_header(file, event_length) ||
          my_b_safe_write(file, (byte*) mem_pool, event_length));
}
#endif


void Slave_log_event::init_from_mem_pool(int data_size)
{
  master_pos = uint8korr(mem_pool + SL_MASTER_POS_OFFSET);
  master_port = uint2korr(mem_pool + SL_MASTER_PORT_OFFSET);
  master_host = mem_pool + SL_MASTER_HOST_OFFSET;
  master_host_len = strlen(master_host);
  // safety
  master_log = master_host + master_host_len + 1;
  if (master_log > mem_pool + data_size)
  {
    master_host = 0;
    return;
  }
  master_log_len = strlen(master_log);
}


/* This code is not used, so has not been updated to be format-tolerant */
Slave_log_event::Slave_log_event(const char* buf, uint event_len)
  :Log_event(buf,0) /*unused event*/ ,mem_pool(0),master_host(0)
{
  if (event_len < LOG_EVENT_HEADER_LEN)
    return;
  event_len -= LOG_EVENT_HEADER_LEN;
  if (!(mem_pool = (char*) my_malloc(event_len + 1, MYF(MY_WME))))
    return;
  memcpy(mem_pool, buf + LOG_EVENT_HEADER_LEN, event_len);
  mem_pool[event_len] = 0;
  init_from_mem_pool(event_len);
}


#ifndef MYSQL_CLIENT
int Slave_log_event::exec_event(struct st_relay_log_info* rli)
{
  if (mysql_bin_log.is_open())
    mysql_bin_log.write(this);
  return Log_event::exec_event(rli);
}
#endif /* !MYSQL_CLIENT */


/**************************************************************************
	Stop_log_event methods
**************************************************************************/

/*
  Stop_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Stop_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;

  print_header(file, print_event_info);
  fprintf(file, "\tStop\n");
  fflush(file);
}
#endif /* MYSQL_CLIENT */


/*
  Stop_log_event::exec_event()

  The master stopped.
  We used to clean up all temporary tables but this is useless as, as the
  master has shut down properly, it has written all DROP TEMPORARY TABLE
  (prepared statements' deletion is TODO only when we binlog prep stmts).
  We used to clean up slave_load_tmpdir, but this is useless as it has been
  cleared at the end of LOAD DATA INFILE.
  So we have nothing to do here.
  The place were we must do this cleaning is in Start_log_event_v3::exec_event(),
  not here. Because if we come here, the master was sane.
*/

#ifndef MYSQL_CLIENT
int Stop_log_event::exec_event(struct st_relay_log_info* rli)
{
  /*
    We do not want to update master_log pos because we get a rotate event
    before stop, so by now group_master_log_name is set to the next log.
    If we updated it, we will have incorrect master coordinates and this
    could give false triggers in MASTER_POS_WAIT() that we have reached
    the target position when in fact we have not.
  */
  if (thd->options & OPTION_BEGIN)
    rli->inc_event_relay_log_pos();
  else
  {
    rli->inc_group_relay_log_pos(0);
    flush_relay_log_info(rli);
  }
  return 0;
}
#endif /* !MYSQL_CLIENT */
#endif /* HAVE_REPLICATION */


/**************************************************************************
	Create_file_log_event methods
**************************************************************************/

/*
  Create_file_log_event ctor
*/

#ifndef MYSQL_CLIENT
Create_file_log_event::
Create_file_log_event(THD* thd_arg, sql_exchange* ex,
		      const char* db_arg, const char* table_name_arg,
		      List<Item>& fields_arg, enum enum_duplicates handle_dup,
                      bool ignore,
		      char* block_arg, uint block_len_arg, bool using_trans)
  :Load_log_event(thd_arg,ex,db_arg,table_name_arg,fields_arg,handle_dup, ignore,
		  using_trans),
   fake_base(0), block(block_arg), event_buf(0), block_len(block_len_arg),
   file_id(thd_arg->file_id = mysql_bin_log.next_file_id())
{
  DBUG_ENTER("Create_file_log_event");
  sql_ex.force_new_format();
  DBUG_VOID_RETURN;
}


/*
  Create_file_log_event::write_data_body()
*/

bool Create_file_log_event::write_data_body(IO_CACHE* file)
{
  bool res;
  if ((res= Load_log_event::write_data_body(file)) || fake_base)
    return res;
  return (my_b_safe_write(file, (byte*) "", 1) ||
          my_b_safe_write(file, (byte*) block, block_len));
}


/*
  Create_file_log_event::write_data_header()
*/

bool Create_file_log_event::write_data_header(IO_CACHE* file)
{
  bool res;
  byte buf[CREATE_FILE_HEADER_LEN];
  if ((res= Load_log_event::write_data_header(file)) || fake_base)
    return res;
  int4store(buf + CF_FILE_ID_OFFSET, file_id);
  return my_b_safe_write(file, buf, CREATE_FILE_HEADER_LEN) != 0;
}


/*
  Create_file_log_event::write_base()
*/

bool Create_file_log_event::write_base(IO_CACHE* file)
{
  bool res;
  fake_base= 1;                                 // pretend we are Load event
  res= write(file);
  fake_base= 0;
  return res;
}

#endif /* !MYSQL_CLIENT */

/*
  Create_file_log_event ctor
*/

Create_file_log_event::Create_file_log_event(const char* buf, uint len,
                                             const Format_description_log_event* description_event)
  :Load_log_event(buf,0,description_event),fake_base(0),block(0),inited_from_old(0)
{
  DBUG_ENTER("Create_file_log_event::Create_file_log_event(char*,...)");
  uint block_offset;
  uint header_len= description_event->common_header_len;
  uint8 load_header_len= description_event->post_header_len[LOAD_EVENT-1];
  uint8 create_file_header_len= description_event->post_header_len[CREATE_FILE_EVENT-1];
  if (!(event_buf= my_memdup((byte*) buf, len, MYF(MY_WME))) ||
      copy_log_event(event_buf,len,
                     ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                      load_header_len + header_len :
                      (fake_base ? (header_len+load_header_len) :
                       (header_len+load_header_len) +
                       create_file_header_len)),
                     description_event))
    DBUG_VOID_RETURN;
  if (description_event->binlog_version!=1)
  {
    file_id= uint4korr(buf + 
                       header_len +
		       load_header_len + CF_FILE_ID_OFFSET);
    /*
      Note that it's ok to use get_data_size() below, because it is computed
      with values we have already read from this event (because we called
      copy_log_event()); we are not using slave's format info to decode
      master's format, we are really using master's format info.
      Anyway, both formats should be identical (except the common_header_len)
      as these Load events are not changed between 4.0 and 5.0 (as logging of
      LOAD DATA INFILE does not use Load_log_event in 5.0).

      The + 1 is for \0 terminating fname  
    */
    block_offset= (description_event->common_header_len +
                   Load_log_event::get_data_size() +
                   create_file_header_len + 1);
    if (len < block_offset)
      return;
    block = (char*)buf + block_offset;
    block_len = len - block_offset;
  }
  else
  {
    sql_ex.force_new_format();
    inited_from_old = 1;
  }
  DBUG_VOID_RETURN;
}


/*
  Create_file_log_event::print()
*/

#ifdef MYSQL_CLIENT
void Create_file_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info,
				  bool enable_local)
{
  if (print_event_info->short_form)
  {
    if (enable_local && check_fname_outside_temp_buf())
      Load_log_event::print(file, print_event_info);
    return;
  }

  if (enable_local)
  {
    Load_log_event::print(file, print_event_info,
			  !check_fname_outside_temp_buf());
    /* 
       That one is for "file_id: etc" below: in mysqlbinlog we want the #, in
       SHOW BINLOG EVENTS we don't.
    */
    fprintf(file, "#"); 
  }

  fprintf(file, " file_id: %d  block_len: %d\n", file_id, block_len);
}


void Create_file_log_event::print(FILE* file, PRINT_EVENT_INFO* print_event_info)
{
  print(file, print_event_info, 0);
}
#endif /* MYSQL_CLIENT */


/*
  Create_file_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Create_file_log_event::pack_info(Protocol *protocol)
{
  char buf[NAME_LEN*2 + 30 + 21*2], *pos;
  pos= strmov(buf, "db=");
  memcpy(pos, db, db_len);
  pos= strmov(pos + db_len, ";table=");
  memcpy(pos, table_name, table_name_len);
  pos= strmov(pos + table_name_len, ";file_id=");
  pos= int10_to_str((long) file_id, pos, 10);
  pos= strmov(pos, ";block_len=");
  pos= int10_to_str((long) block_len, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/*
  Create_file_log_event::exec_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Create_file_log_event::exec_event(struct st_relay_log_info* rli)
{
  char proc_info[17+FN_REFLEN+10], *fname_buf;
  char *ext;
  int fd = -1;
  IO_CACHE file;
  int error = 1;

  bzero((char*)&file, sizeof(file));
  fname_buf= strmov(proc_info, "Making temp file ");
  ext= slave_load_file_stem(fname_buf, file_id, server_id, ".info");
  thd->proc_info= proc_info;
  my_delete(fname_buf, MYF(0)); // old copy may exist already
  if ((fd= my_create(fname_buf, CREATE_MODE,
		     O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
		     MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, WRITE_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    slave_print_msg(ERROR_LEVEL, rli, my_errno, "Error in Create_file event: "
                    "could not open file '%s'", fname_buf);
    goto err;
  }
  
  // a trick to avoid allocating another buffer
  fname= fname_buf;
  fname_len= (uint) (strmov(ext, ".data") - fname);
  if (write_base(&file))
  {
    strmov(ext, ".info"); // to have it right in the error message
    slave_print_msg(ERROR_LEVEL, rli, my_errno,
                    "Error in Create_file event: could not write to file '%s'",
                    fname_buf);
    goto err;
  }
  end_io_cache(&file);
  my_close(fd, MYF(0));
  
  // fname_buf now already has .data, not .info, because we did our trick
  my_delete(fname_buf, MYF(0)); // old copy may exist already
  if ((fd= my_create(fname_buf, CREATE_MODE,
		     O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
		     MYF(MY_WME))) < 0)
  {
    slave_print_msg(ERROR_LEVEL, rli, my_errno, "Error in Create_file event: "
                    "could not open file '%s'", fname_buf);
    goto err;
  }
  if (my_write(fd, (byte*) block, block_len, MYF(MY_WME+MY_NABP)))
  {
    slave_print_msg(ERROR_LEVEL, rli, my_errno, "Error in Create_file event: "
                    "write to '%s' failed", fname_buf);
    goto err;
  }
  error=0;					// Everything is ok

err:
  if (error)
    end_io_cache(&file);
  if (fd >= 0)
    my_close(fd, MYF(0));
  thd->proc_info= 0;
  return error ? 1 : Log_event::exec_event(rli);
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**************************************************************************
	Append_block_log_event methods
**************************************************************************/

/*
  Append_block_log_event ctor
*/

#ifndef MYSQL_CLIENT  
Append_block_log_event::Append_block_log_event(THD* thd_arg, const char* db_arg,
					       char* block_arg,
					       uint block_len_arg,
					       bool using_trans)
  :Log_event(thd_arg,0, using_trans), block(block_arg),
   block_len(block_len_arg), file_id(thd_arg->file_id), db(db_arg)
{
}
#endif


/*
  Append_block_log_event ctor
*/

Append_block_log_event::Append_block_log_event(const char* buf, uint len,
                                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event),block(0)
{
  DBUG_ENTER("Append_block_log_event::Append_block_log_event(char*,...)");
  uint8 common_header_len= description_event->common_header_len; 
  uint8 append_block_header_len=
    description_event->post_header_len[APPEND_BLOCK_EVENT-1];
  uint total_header_len= common_header_len+append_block_header_len;
  if (len < total_header_len)
    DBUG_VOID_RETURN;
  file_id= uint4korr(buf + common_header_len + AB_FILE_ID_OFFSET);
  block= (char*)buf + total_header_len;
  block_len= len - total_header_len;
  DBUG_VOID_RETURN;
}


/*
  Append_block_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Append_block_log_event::write(IO_CACHE* file)
{
  byte buf[APPEND_BLOCK_HEADER_LEN];
  int4store(buf + AB_FILE_ID_OFFSET, file_id);
  return (write_header(file, APPEND_BLOCK_HEADER_LEN + block_len) ||
          my_b_safe_write(file, buf, APPEND_BLOCK_HEADER_LEN) ||
	  my_b_safe_write(file, (byte*) block, block_len));
}
#endif


/*
  Append_block_log_event::print()
*/

#ifdef MYSQL_CLIENT  
void Append_block_log_event::print(FILE* file,
				   PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(file, print_event_info);
  fputc('\n', file);
  fprintf(file, "#%s: file_id: %d  block_len: %d\n",
	  get_type_str(), file_id, block_len);
}
#endif /* MYSQL_CLIENT */


/*
  Append_block_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Append_block_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  uint length;
  length= (uint) my_sprintf(buf,
			    (buf, ";file_id=%u;block_len=%u", file_id,
			     block_len));
  protocol->store(buf, length, &my_charset_bin);
}


/*
  Append_block_log_event::get_create_or_append()
*/

int Append_block_log_event::get_create_or_append() const
{
  return 0; /* append to the file, fail if not exists */
}

/*
  Append_block_log_event::exec_event()
*/

int Append_block_log_event::exec_event(struct st_relay_log_info* rli)
{
  char proc_info[17+FN_REFLEN+10], *fname= proc_info+17;
  int fd;
  int error = 1;
  DBUG_ENTER("Append_block_log_event::exec_event");

  fname= strmov(proc_info, "Making temp file ");
  slave_load_file_stem(fname, file_id, server_id, ".data");
  thd->proc_info= proc_info;
  if (get_create_or_append())
  {
    my_delete(fname, MYF(0)); // old copy may exist already
    if ((fd= my_create(fname, CREATE_MODE,
		       O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
		       MYF(MY_WME))) < 0)
    {
      slave_print_msg(ERROR_LEVEL, rli, my_errno,
                      "Error in %s event: could not create file '%s'",
                      get_type_str(), fname);
      goto err;
    }
  }
  else if ((fd = my_open(fname, O_WRONLY | O_APPEND | O_BINARY | O_NOFOLLOW,
                         MYF(MY_WME))) < 0)
  {
    slave_print_msg(ERROR_LEVEL, rli, my_errno,
                    "Error in %s event: could not open file '%s'",
                    get_type_str(), fname);
    goto err;
  }
  if (my_write(fd, (byte*) block, block_len, MYF(MY_WME+MY_NABP)))
  {
    slave_print_msg(ERROR_LEVEL, rli, my_errno,
                    "Error in %s event: write to '%s' failed",
                    get_type_str(), fname);
    goto err;
  }
  error=0;

err:
  if (fd >= 0)
    my_close(fd, MYF(0));
  thd->proc_info= 0;
  DBUG_RETURN(error ? error : Log_event::exec_event(rli));
}
#endif


/**************************************************************************
	Delete_file_log_event methods
**************************************************************************/

/*
  Delete_file_log_event ctor
*/

#ifndef MYSQL_CLIENT
Delete_file_log_event::Delete_file_log_event(THD *thd_arg, const char* db_arg,
					     bool using_trans)
  :Log_event(thd_arg, 0, using_trans), file_id(thd_arg->file_id), db(db_arg)
{
}
#endif

/*
  Delete_file_log_event ctor
*/

Delete_file_log_event::Delete_file_log_event(const char* buf, uint len,
                                             const Format_description_log_event* description_event)
  :Log_event(buf, description_event),file_id(0)
{
  uint8 common_header_len= description_event->common_header_len;
  uint8 delete_file_header_len= description_event->post_header_len[DELETE_FILE_EVENT-1];
  if (len < (uint)(common_header_len + delete_file_header_len))
    return;
  file_id= uint4korr(buf + common_header_len + DF_FILE_ID_OFFSET);
}


/*
  Delete_file_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Delete_file_log_event::write(IO_CACHE* file)
{
 byte buf[DELETE_FILE_HEADER_LEN];
 int4store(buf + DF_FILE_ID_OFFSET, file_id);
 return (write_header(file, sizeof(buf)) ||
         my_b_safe_write(file, buf, sizeof(buf)));
}
#endif


/*
  Delete_file_log_event::print()
*/

#ifdef MYSQL_CLIENT  
void Delete_file_log_event::print(FILE* file,
				  PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(file, print_event_info);
  fputc('\n', file);
  fprintf(file, "#Delete_file: file_id=%u\n", file_id);
}
#endif /* MYSQL_CLIENT */

/*
  Delete_file_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Delete_file_log_event::pack_info(Protocol *protocol)
{
  char buf[64];
  uint length;
  length= (uint) my_sprintf(buf, (buf, ";file_id=%u", (uint) file_id));
  protocol->store(buf, (int32) length, &my_charset_bin);
}
#endif

/*
  Delete_file_log_event::exec_event()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Delete_file_log_event::exec_event(struct st_relay_log_info* rli)
{
  char fname[FN_REFLEN+10];
  char *ext= slave_load_file_stem(fname, file_id, server_id, ".data");
  (void) my_delete(fname, MYF(MY_WME));
  strmov(ext, ".info");
  (void) my_delete(fname, MYF(MY_WME));
  return Log_event::exec_event(rli);
}
#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**************************************************************************
	Execute_load_log_event methods
**************************************************************************/

/*
  Execute_load_log_event ctor
*/

#ifndef MYSQL_CLIENT  
Execute_load_log_event::Execute_load_log_event(THD *thd_arg, const char* db_arg,
					       bool using_trans)
  :Log_event(thd_arg, 0, using_trans), file_id(thd_arg->file_id), db(db_arg)
{
}
#endif
  

/*
  Execute_load_log_event ctor
*/

Execute_load_log_event::Execute_load_log_event(const char* buf, uint len,
                                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event), file_id(0)
{
  uint8 common_header_len= description_event->common_header_len;
  uint8 exec_load_header_len= description_event->post_header_len[EXEC_LOAD_EVENT-1];
  if (len < (uint)(common_header_len+exec_load_header_len))
    return;
  file_id= uint4korr(buf + common_header_len + EL_FILE_ID_OFFSET);
}


/*
  Execute_load_log_event::write()
*/

#ifndef MYSQL_CLIENT
bool Execute_load_log_event::write(IO_CACHE* file)
{
  byte buf[EXEC_LOAD_HEADER_LEN];
  int4store(buf + EL_FILE_ID_OFFSET, file_id);
  return (write_header(file, sizeof(buf)) || 
          my_b_safe_write(file, buf, sizeof(buf)));
}
#endif


/*
  Execute_load_log_event::print()
*/

#ifdef MYSQL_CLIENT  
void Execute_load_log_event::print(FILE* file,
				   PRINT_EVENT_INFO* print_event_info)
{
  if (print_event_info->short_form)
    return;
  print_header(file, print_event_info);
  fputc('\n', file);
  fprintf(file, "#Exec_load: file_id=%d\n",
	  file_id);
}
#endif

/*
  Execute_load_log_event::pack_info()
*/

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Execute_load_log_event::pack_info(Protocol *protocol)
{
  char buf[64];
  uint length;
  length= (uint) my_sprintf(buf, (buf, ";file_id=%u", (uint) file_id));
  protocol->store(buf, (int32) length, &my_charset_bin);
}


/*
  Execute_load_log_event::exec_event()
*/

int Execute_load_log_event::exec_event(struct st_relay_log_info* rli)
{
  char fname[FN_REFLEN+10];
  char *ext;
  int fd;
  int error= 1;
  IO_CACHE file;
  Load_log_event *lev= 0;

  ext= slave_load_file_stem(fname, file_id, server_id, ".info");
  if ((fd = my_open(fname, O_RDONLY | O_BINARY | O_NOFOLLOW,
                    MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, READ_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    slave_print_msg(ERROR_LEVEL, rli, my_errno, "Error in Exec_load event: "
                    "could not open file '%s'", fname);
    goto err;
  }
  if (!(lev = (Load_log_event*)Log_event::read_log_event(&file,
                                                         (pthread_mutex_t*)0,
                                                         rli->relay_log.description_event_for_exec)) ||
      lev->get_type_code() != NEW_LOAD_EVENT)
  {
    slave_print_msg(ERROR_LEVEL, rli, 0, "Error in Exec_load event: "
                    "file '%s' appears corrupted", fname);
    goto err;
  }

  lev->thd = thd;
  /*
    lev->exec_event should use rli only for errors
    i.e. should not advance rli's position.
    lev->exec_event is the place where the table is loaded (it calls
    mysql_load()).
  */

  rli->future_group_master_log_pos= log_pos;
  if (lev->exec_event(0,rli,1)) 
  {
    /*
      We want to indicate the name of the file that could not be loaded
      (SQL_LOADxxx).
      But as we are here we are sure the error is in rli->last_slave_error and
      rli->last_slave_errno (example of error: duplicate entry for key), so we
      don't want to overwrite it with the filename.
      What we want instead is add the filename to the current error message.
    */
    char *tmp= my_strdup(rli->last_slave_error,MYF(MY_WME));
    if (tmp)
    {
      slave_print_msg(ERROR_LEVEL, rli,
                      rli->last_slave_errno, /* ok to re-use error code */
                      "%s. Failed executing load from '%s'", 
                      tmp, fname);
      my_free(tmp,MYF(0));
    }
    goto err;
  }
  /*
    We have an open file descriptor to the .info file; we need to close it
    or Windows will refuse to delete the file in my_delete().
  */
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&file);
    fd= -1;
  }
  (void) my_delete(fname, MYF(MY_WME));
  memcpy(ext, ".data", 6);
  (void) my_delete(fname, MYF(MY_WME));
  error = 0;

err:
  delete lev;
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&file);
  }
  return error ? error : Log_event::exec_event(rli);
}

#endif /* defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**************************************************************************
	Begin_load_query_log_event methods
**************************************************************************/

#ifndef MYSQL_CLIENT
Begin_load_query_log_event::
Begin_load_query_log_event(THD* thd_arg, const char* db_arg, char* block_arg,
                           uint block_len_arg, bool using_trans)
  :Append_block_log_event(thd_arg, db_arg, block_arg, block_len_arg,
                          using_trans)
{
   file_id= thd_arg->file_id= mysql_bin_log.next_file_id();
}
#endif


Begin_load_query_log_event::
Begin_load_query_log_event(const char* buf, uint len,
                           const Format_description_log_event* desc_event)
  :Append_block_log_event(buf, len, desc_event)
{
}


#if defined( HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
int Begin_load_query_log_event::get_create_or_append() const
{
  return 1; /* create the file */
}
#endif /* defined( HAVE_REPLICATION) && !defined(MYSQL_CLIENT) */


/**************************************************************************
	Execute_load_query_log_event methods
**************************************************************************/


#ifndef MYSQL_CLIENT
Execute_load_query_log_event::
Execute_load_query_log_event(THD* thd_arg, const char* query_arg,
                     ulong query_length_arg, uint fn_pos_start_arg,
                     uint fn_pos_end_arg,
                     enum_load_dup_handling dup_handling_arg,
                     bool using_trans, bool suppress_use):
  Query_log_event(thd_arg, query_arg, query_length_arg, using_trans,
                  suppress_use),
  file_id(thd_arg->file_id), fn_pos_start(fn_pos_start_arg),
  fn_pos_end(fn_pos_end_arg), dup_handling(dup_handling_arg)
{
}
#endif /* !MYSQL_CLIENT */


Execute_load_query_log_event::
Execute_load_query_log_event(const char* buf, uint event_len,
                             const Format_description_log_event* desc_event):
  Query_log_event(buf, event_len, desc_event, EXECUTE_LOAD_QUERY_EVENT),
  file_id(0), fn_pos_start(0), fn_pos_end(0)
{
  if (!Query_log_event::is_valid())
    return;

  buf+= desc_event->common_header_len;

  fn_pos_start= uint4korr(buf + ELQ_FN_POS_START_OFFSET);
  fn_pos_end= uint4korr(buf + ELQ_FN_POS_END_OFFSET);
  dup_handling= (enum_load_dup_handling)(*(buf + ELQ_DUP_HANDLING_OFFSET));

  if (fn_pos_start > q_len || fn_pos_end > q_len ||
      dup_handling > LOAD_DUP_REPLACE)
    return;

  file_id= uint4korr(buf + ELQ_FILE_ID_OFFSET);
}


ulong Execute_load_query_log_event::get_post_header_size_for_derived()
{
  return EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN;
}


#ifndef MYSQL_CLIENT
bool
Execute_load_query_log_event::write_post_header_for_derived(IO_CACHE* file)
{
  char buf[EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN];
  int4store(buf, file_id);
  int4store(buf + 4, fn_pos_start);
  int4store(buf + 4 + 4, fn_pos_end);
  *(buf + 4 + 4 + 4)= (char)dup_handling;
  return my_b_safe_write(file, (byte*) buf, EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN);
}
#endif


#ifdef MYSQL_CLIENT
void Execute_load_query_log_event::print(FILE* file,
                                         PRINT_EVENT_INFO* print_event_info)
{
  print(file, print_event_info, 0);
}


void Execute_load_query_log_event::print(FILE* file,
                                         PRINT_EVENT_INFO* print_event_info,
                                         const char *local_fname)
{
  print_query_header(file, print_event_info);

  if (local_fname)
  {
    my_fwrite(file, (byte*) query, fn_pos_start, MYF(MY_NABP | MY_WME));
    fprintf(file, " LOCAL INFILE \'");
    fprintf(file, local_fname);
    fprintf(file, "\'");
    if (dup_handling == LOAD_DUP_REPLACE)
      fprintf(file, " REPLACE");
    fprintf(file, " INTO");
    my_fwrite(file, (byte*) query + fn_pos_end, q_len-fn_pos_end,
        MYF(MY_NABP | MY_WME));
    fprintf(file, ";\n");
  }
  else
  {
    my_fwrite(file, (byte*) query, q_len, MYF(MY_NABP | MY_WME));
    fprintf(file, ";\n");
  }

  if (!print_event_info->short_form)
    fprintf(file, "# file_id: %d \n", file_id);
}
#endif


#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Execute_load_query_log_event::pack_info(Protocol *protocol)
{
  char *buf, *pos;
  if (!(buf= my_malloc(9 + db_len + q_len + 10 + 21, MYF(MY_WME))))
    return;
  pos= buf;
  if (db && db_len)
  {
    pos= strmov(buf, "use `");
    memcpy(pos, db, db_len);
    pos= strmov(pos+db_len, "`; ");
  }
  if (query && q_len)
  {
    memcpy(pos, query, q_len);
    pos+= q_len;
  }
  pos= strmov(pos, " ;file_id=");
  pos= int10_to_str((long) file_id, pos, 10);
  protocol->store(buf, pos-buf, &my_charset_bin);
  my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
}


int
Execute_load_query_log_event::exec_event(struct st_relay_log_info* rli)
{
  char *p;
  char *buf;
  char *fname;
  char *fname_end;
  int error;

  /* Replace filename and LOCAL keyword in query before executing it */
  if (!(buf = my_malloc(q_len + 1 - (fn_pos_end - fn_pos_start) +
                        (FN_REFLEN + 10) + 10 + 8 + 5, MYF(MY_WME))))
  {
    slave_print_msg(ERROR_LEVEL, rli, my_errno, "Not enough memory");
    return 1;
  }

  p= buf;
  memcpy(p, query, fn_pos_start);
  p+= fn_pos_start;
  fname= (p= strmake(p, STRING_WITH_LEN(" INFILE \'")));
  p= slave_load_file_stem(p, file_id, server_id, ".data");
  fname_end= p= strend(p);                      // Safer than p=p+5
  *(p++)='\'';
  switch (dup_handling) {
  case LOAD_DUP_IGNORE:
    p= strmake(p, STRING_WITH_LEN(" IGNORE"));
    break;
  case LOAD_DUP_REPLACE:
    p= strmake(p, STRING_WITH_LEN(" REPLACE"));
    break;
  default:
    /* Ordinary load data */
    break;
  }
  p= strmake(p, STRING_WITH_LEN(" INTO"));
  p= strmake(p, query+fn_pos_end, q_len-fn_pos_end);

  error= Query_log_event::exec_event(rli, buf, p-buf);

  /* Forging file name for deletion in same buffer */
  *fname_end= 0;

  /*
    If there was an error the slave is going to stop, leave the
    file so that we can re-execute this event at START SLAVE.
  */
  if (!error)
    (void) my_delete(fname, MYF(MY_WME));

  my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
  return error;
}
#endif


/**************************************************************************
	sql_ex_info methods
**************************************************************************/

/*
  sql_ex_info::write_data()
*/

bool sql_ex_info::write_data(IO_CACHE* file)
{
  if (new_format())
  {
    return (write_str(file, field_term, (uint) field_term_len) ||
	    write_str(file, enclosed,   (uint) enclosed_len) ||
	    write_str(file, line_term,  (uint) line_term_len) ||
	    write_str(file, line_start, (uint) line_start_len) ||
	    write_str(file, escaped,    (uint) escaped_len) ||
	    my_b_safe_write(file,(byte*) &opt_flags,1));
  }
  else
  {
    old_sql_ex old_ex;
    old_ex.field_term= *field_term;
    old_ex.enclosed=   *enclosed;
    old_ex.line_term=  *line_term;
    old_ex.line_start= *line_start;
    old_ex.escaped=    *escaped;
    old_ex.opt_flags=  opt_flags;
    old_ex.empty_flags=empty_flags;
    return my_b_safe_write(file, (byte*) &old_ex, sizeof(old_ex)) != 0;
  }
}


/*
  sql_ex_info::init()
*/

char* sql_ex_info::init(char* buf,char* buf_end,bool use_new_format)
{
  cached_new_format = use_new_format;
  if (use_new_format)
  {
    empty_flags=0;
    /*
      The code below assumes that buf will not disappear from
      under our feet during the lifetime of the event. This assumption
      holds true in the slave thread if the log is in new format, but is not
      the case when we have old format because we will be reusing net buffer
      to read the actual file before we write out the Create_file event.
    */
    if (read_str(&buf, buf_end, &field_term, &field_term_len) ||
	read_str(&buf, buf_end, &enclosed,   &enclosed_len) ||
	read_str(&buf, buf_end, &line_term,  &line_term_len) ||
	read_str(&buf, buf_end, &line_start, &line_start_len) ||
	read_str(&buf, buf_end, &escaped,    &escaped_len))
      return 0;
    opt_flags = *buf++;
  }
  else
  {
    field_term_len= enclosed_len= line_term_len= line_start_len= escaped_len=1;
    field_term = buf++;			// Use first byte in string
    enclosed=	 buf++;
    line_term=   buf++;
    line_start=  buf++;
    escaped=     buf++;
    opt_flags =  *buf++;
    empty_flags= *buf++;
    if (empty_flags & FIELD_TERM_EMPTY)
      field_term_len=0;
    if (empty_flags & ENCLOSED_EMPTY)
      enclosed_len=0;
    if (empty_flags & LINE_TERM_EMPTY)
      line_term_len=0;
    if (empty_flags & LINE_START_EMPTY)
      line_start_len=0;
    if (empty_flags & ESCAPED_EMPTY)
      escaped_len=0;
  }
  return buf;
}


#ifdef HAVE_ROW_BASED_REPLICATION

/**************************************************************************
	Rows_log_event member functions
**************************************************************************/

#ifndef MYSQL_CLIENT
Rows_log_event::Rows_log_event(THD *thd_arg, TABLE *tbl_arg, ulong tid,
                               MY_BITMAP const *cols, bool is_transactional)
  : Log_event(thd_arg, 0, is_transactional),
    m_row_count(0),
    m_table(tbl_arg),
    m_table_id(tid),
    m_width(tbl_arg ? tbl_arg->s->fields : 1),
    m_rows_buf(0), m_rows_cur(0), m_rows_end(0),
    m_flags(0)
{
  /*
    We allow a special form of dummy event when the table, and cols
    are null and the table id is ~0UL.  This is a temporary
    solution, to be able to terminate a started statement in the
    binary log: the extreneous events will be removed in the future.
   */
  DBUG_ASSERT(tbl_arg && tbl_arg->s && tid != ~0UL ||
              !tbl_arg && !cols && tid == ~0UL);

  if (thd_arg->options & OPTION_NO_FOREIGN_KEY_CHECKS)
      set_flags(NO_FOREIGN_KEY_CHECKS_F);
  if (thd_arg->options & OPTION_RELAXED_UNIQUE_CHECKS)
      set_flags(RELAXED_UNIQUE_CHECKS_F);
  /* if bitmap_init fails, catched in is_valid() */
  if (likely(!bitmap_init(&m_cols,
                          m_width <= sizeof(m_bitbuf)*8 ? m_bitbuf : NULL,
                          (m_width + 7) & ~7UL,
                          false)))
  {
    /* Cols can be zero if this is a dummy binrows event */
    if (likely(cols != NULL))
      memcpy(m_cols.bitmap, cols->bitmap, no_bytes_in_map(cols));
  }
  else
    m_cols.bitmap= 0; // to not free it
}
#endif

Rows_log_event::Rows_log_event(const char *buf, uint event_len,
                               Log_event_type event_type,
                               const Format_description_log_event
                               *description_event)
  : Log_event(buf, description_event),
    m_row_count(0),
    m_rows_buf(0), m_rows_cur(0), m_rows_end(0)
{
  DBUG_ENTER("Rows_log_event::Rows_log_event(const char*,...)");
  uint8 const common_header_len= description_event->common_header_len;
  uint8 const post_header_len= description_event->post_header_len[event_type-1];

  DBUG_PRINT("enter",("event_len=%ld, common_header_len=%d, "
		      "post_header_len=%d",
		      event_len, common_header_len,
		      post_header_len));

  const char *post_start= buf + common_header_len;
  post_start+= RW_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    m_table_id= uint4korr(post_start);
    post_start+= 4;
  }
  else
  {
    m_table_id= uint6korr(post_start);
    post_start+= RW_FLAGS_OFFSET;
  }

  m_flags= uint2korr(post_start);

  byte const *const var_start= (const byte *)buf + common_header_len + 
    post_header_len;
  byte const *const ptr_width= var_start;
  uchar *ptr_after_width= (uchar*) ptr_width;
  m_width = net_field_length(&ptr_after_width);

  const uint byte_count= (m_width + 7) / 8;
  const byte* const ptr_rows_data= var_start + byte_count + 1;

  my_size_t const data_size= event_len - (ptr_rows_data - (const byte *) buf);
  DBUG_PRINT("info",("m_table_id=%lu, m_flags=%d, m_width=%u, data_size=%lu",
                     m_table_id, m_flags, m_width, data_size));

  m_rows_buf= (byte*)my_malloc(data_size, MYF(MY_WME));
  if (likely((bool)m_rows_buf))
  {
    /* if bitmap_init fails, catched in is_valid() */
    if (likely(!bitmap_init(&m_cols,
                            m_width <= sizeof(m_bitbuf)*8 ? m_bitbuf : NULL,
                            (m_width + 7) & ~7UL,
                            false)))
      memcpy(m_cols.bitmap, ptr_after_width, byte_count);
    m_rows_end= m_rows_buf + data_size;
    m_rows_cur= m_rows_end;
    memcpy(m_rows_buf, ptr_rows_data, data_size);
  }
  else
    m_cols.bitmap= 0; // to not free it

  DBUG_VOID_RETURN;
}

Rows_log_event::~Rows_log_event()
{
  if (m_cols.bitmap == m_bitbuf) // no my_malloc happened
    m_cols.bitmap= 0; // so no my_free in bitmap_free
  bitmap_free(&m_cols); // To pair with bitmap_init().
  my_free((gptr)m_rows_buf, MYF(MY_ALLOW_ZERO_PTR));
}

#ifndef MYSQL_CLIENT
int Rows_log_event::do_add_row_data(byte *const row_data,
                                    my_size_t const length)
{
  /*
    When the table has a primary key, we would probably want, by default, to
    log only the primary key value instead of the entire "before image". This
    would save binlog space. TODO
  */
  DBUG_ENTER("Rows_log_event::do_add_row_data");
  DBUG_PRINT("enter", ("row_data: 0x%lx  length: %lu", (ulong) row_data,
                       length));
  DBUG_DUMP("row_data", (const char*)row_data, min(length, 32));

  DBUG_ASSERT(m_rows_buf <= m_rows_cur);
  DBUG_ASSERT(!m_rows_buf || m_rows_end && m_rows_buf < m_rows_end);
  DBUG_ASSERT(m_rows_cur <= m_rows_end);

  /* The cast will always work since m_rows_cur <= m_rows_end */
  if (static_cast<my_size_t>(m_rows_end - m_rows_cur) < length)
  {
    my_size_t const block_size= 1024;
    my_ptrdiff_t const old_alloc= m_rows_end - m_rows_buf;
    my_ptrdiff_t const cur_size= m_rows_cur - m_rows_buf;
    my_ptrdiff_t const new_alloc= 
        block_size * ((cur_size + length) / block_size + block_size - 1);

    byte* const new_buf= (byte*)my_realloc((gptr)m_rows_buf, new_alloc,
                                           MYF(MY_ALLOW_ZERO_PTR|MY_WME));
    if (unlikely(!new_buf))
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);

    /* If the memory moved, we need to move the pointers */
    if (new_buf != m_rows_buf)
    {
      m_rows_buf= new_buf;
      m_rows_cur= m_rows_buf + cur_size;
    }

    /*
       The end pointer should always be changed to point to the end of
       the allocated memory.
    */
    m_rows_end= m_rows_buf + new_alloc;
  }

  DBUG_ASSERT(m_rows_cur + length < m_rows_end);
  memcpy(m_rows_cur, row_data, length);
  m_rows_cur+= length;
  m_row_count++;
  DBUG_RETURN(0);
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
/*
  Unpack a row into a record.
  
  SYNOPSIS
    unpack_row()
    rli     Relay log info
    table   Table to unpack into
    colcnt  Number of columns to read from record
    record  Record where the data should be unpacked
    row     Packed row data
    cols    Pointer to columns data to fill in
    row_end Pointer to variable that will hold the value of the
            one-after-end position for the row
    master_reclength
            Pointer to variable that will hold the length of the
            record on the master side
    rw_set  Pointer to bitmap that holds either the read_set or the
            write_set of the table

  DESCRIPTION

      The row is assumed to only consist of the fields for which the
      bitset represented by 'arr' and 'bits'; the other parts of the
      record are left alone.

      At most 'colcnt' columns are read: if the table is larger than
      that, the remaining fields are not filled in.
 */
static int
unpack_row(RELAY_LOG_INFO *rli,
           TABLE *table, uint const colcnt, byte *record,
           char const *row, MY_BITMAP const *cols,
           char const **row_end, ulong *master_reclength,
           MY_BITMAP* const rw_set)
{
  DBUG_ASSERT(record && row);
  my_ptrdiff_t const offset= record - (byte*) table->record[0];
  my_size_t master_null_bytes= table->s->null_bytes;

  if (colcnt != table->s->fields)
  {
    Field **fptr= &table->field[colcnt-1];
    do
      master_null_bytes= (*fptr)->last_null_byte();
    while (master_null_bytes == 0 && fptr-- > table->field);

    if (master_null_bytes == 0)
      master_null_bytes= table->s->null_bytes;
  }

  DBUG_ASSERT(master_null_bytes <= table->s->null_bytes);
  memcpy(record, row, master_null_bytes);            // [1]
  int error= 0;

  bitmap_set_all(rw_set);

  Field **const begin_ptr = table->field;
  Field **field_ptr;
  {
    char const *ptr= row + master_null_bytes;
    Field **const end_ptr= begin_ptr + colcnt;
    for (field_ptr= begin_ptr ; field_ptr < end_ptr ; ++field_ptr)
    {
      Field *const f= *field_ptr;

      if (bitmap_is_set(cols, field_ptr -  begin_ptr))
      {
        ptr= f->unpack(f->ptr + offset, ptr);
        /* Field...::unpack() cannot return 0 */
        DBUG_ASSERT(ptr != NULL);
      }
      else
        bitmap_clear_bit(rw_set, field_ptr - begin_ptr);
    }

    *row_end = ptr;
    if (master_reclength)
    {
      if (*field_ptr)
        *master_reclength = (*field_ptr)->ptr - table->record[0];
      else
        *master_reclength = table->s->reclength;
    }
  }

  /*
    Set properties for remaining columns, if there are any. We let the
    corresponding bit in the write_set be set, to write the value if
    it was not there already. We iterate over all remaining columns,
    even if there were an error, to get as many error messages as
    possible.  We are still able to return a pointer to the next row,
    so wedo that.
   */
  for ( ; *field_ptr ; ++field_ptr)
  {
    if ((*field_ptr)->flags & (NOT_NULL_FLAG | NO_DEFAULT_VALUE_FLAG))
    {
      slave_print_msg(ERROR_LEVEL, rli, ER_NO_DEFAULT_FOR_FIELD, 
                      "Field `%s` of table `%s`.`%s` "
                      "has no default value and cannot be NULL",
                      (*field_ptr)->field_name, table->s->db.str, 
                      table->s->table_name.str);
      error = ER_NO_DEFAULT_FOR_FIELD;
    }
    else
      (*field_ptr)->set_default();
  }

  return error;
}

int Rows_log_event::exec_event(st_relay_log_info *rli)
{
  DBUG_ENTER("Rows_log_event::exec_event(st_relay_log_info*)");
  int error= 0;
  char const *row_start= (char const *)m_rows_buf;

  /*
    If m_table_id == ~0UL, then we have a dummy event that does
    not contain any data.  In that case, we just remove all tables in
    the tables_to_lock list, close the thread tables, step the relay
    log position, and return with success.
   */
  if (m_table_id == ~0UL)
  {
    /*
       This one is supposed to be set: just an extra check so that
       nothing strange has happened.
     */
    DBUG_ASSERT(get_flags(STMT_END_F));

    rli->clear_tables_to_lock();
    close_thread_tables(thd);
    thd->clear_error();
    rli->inc_event_relay_log_pos();
    DBUG_RETURN(0);
  }

  /*
    'thd' has been set by exec_relay_log_event(), just before calling
    exec_event(). We still check here to prevent future coding errors.
  */
  DBUG_ASSERT(rli->sql_thd == thd);

  /*
    If there is no locks taken, this is the first binrow event seen
    after the table map events.  We should then lock all the tables
    used in the transaction and proceed with execution of the actual
    event.
  */
  if (!thd->lock)
  {
    bool need_reopen= 1; /* To execute the first lap of the loop below */

    /*
      lock_tables() reads the contents of thd->lex, so they must be
      initialized. Contrary to in Table_map_log_event::exec_event() we don't
      call mysql_init_query() as that may reset the binlog format.
    */
    lex_start(thd, NULL, 0);

    while ((error= lock_tables(thd, rli->tables_to_lock,
                               rli->tables_to_lock_count, &need_reopen)))
    {
      if (!need_reopen)
      {
        slave_print_msg(ERROR_LEVEL, rli, error,
                        "Error in %s event: when locking tables",
                        get_type_str());
        rli->clear_tables_to_lock();
        DBUG_RETURN(error);
      }

      /*
        So we need to reopen the tables.

        We need to flush the pending RBR event, since it keeps a
        pointer to an open table.

        ALTERNATIVE SOLUTION (not implemented): Extract a pointer to
        the pending RBR event and reset the table pointer after the
        tables has been reopened.

        NOTE: For this new scheme there should be no pending event:
        need to add code to assert that is the case.
       */
      thd->binlog_flush_pending_rows_event(false);
      close_tables_for_reopen(thd, &rli->tables_to_lock);

      if ((error= open_tables(thd, &rli->tables_to_lock,
                              &rli->tables_to_lock_count, 0)))
      {
        if (thd->query_error || thd->is_fatal_error)
        {
          /*
            Error reporting borrowed from Query_log_event with many excessive
            simplifications (we don't honour --slave-skip-errors)
          */
          uint actual_error= thd->net.last_errno;
          slave_print_msg(ERROR_LEVEL, rli, actual_error,
                          "Error '%s' on reopening tables",
                          (actual_error ? thd->net.last_error :
                           "unexpected success or fatal error"));
          thd->query_error= 1;
        }
        rli->clear_tables_to_lock();
        DBUG_RETURN(error);
      }
    }
    /*
      When the open and locking succeeded, we add all the tables to
      the table map and remove them from tables to lock.

      We also invalidate the query cache for all the tables, since
      they will now be changed.
     */
    
    TABLE_LIST *ptr;
    for (ptr= rli->tables_to_lock ; ptr ; ptr= ptr->next_global)
    {
      rli->m_table_map.set_table(ptr->table_id, ptr->table);
      rli->touching_table(ptr->db, ptr->table_name, ptr->table_id);
    }
#ifdef HAVE_QUERY_CACHE
    query_cache.invalidate_locked_for_write(rli->tables_to_lock);
#endif
    rli->clear_tables_to_lock();
  }

  DBUG_ASSERT(rli->tables_to_lock == NULL && rli->tables_to_lock_count == 0);

  TABLE* table= rli->m_table_map.get_table(m_table_id);

  if (table)
  {
    /*
      table == NULL means that this table should not be replicated
      (this was set up by Table_map_log_event::exec_event() which
      tested replicate-* rules).
    */

    /*
      It's not needed to set_time() but
      1) it continues the property that "Time" in SHOW PROCESSLIST shows how
      much slave is behind
      2) it will be needed when we allow replication from a table with no
      TIMESTAMP column to a table with one.
      So we call set_time(), like in SBR. Presently it changes nothing.
    */
    thd->set_time((time_t)when);
    /*
      There are a few flags that are replicated with each row event.
      Make sure to set/clear them before executing the main body of
      the event.
    */
    if (get_flags(NO_FOREIGN_KEY_CHECKS_F))
        thd->options|= OPTION_NO_FOREIGN_KEY_CHECKS;
    else
        thd->options&= ~OPTION_NO_FOREIGN_KEY_CHECKS;

    if (get_flags(RELAXED_UNIQUE_CHECKS_F))
        thd->options|= OPTION_RELAXED_UNIQUE_CHECKS;
    else
        thd->options&= ~OPTION_RELAXED_UNIQUE_CHECKS;
    /* A small test to verify that objects have consistent types */
    DBUG_ASSERT(sizeof(thd->options) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));

    error= do_before_row_operations(table);
    while (error == 0 && row_start < (const char*) m_rows_end)
    {
      char const *row_end= NULL;
      if ((error= do_prepare_row(thd, rli, table, row_start, &row_end)))
        break; // We should to the after-row operation even in the
               // case of error
      
      DBUG_ASSERT(row_end != NULL); // cannot happen
      DBUG_ASSERT(row_end <= (const char*)m_rows_end);

      /* in_use can have been set to NULL in close_tables_for_reopen */
      THD* old_thd= table->in_use;
      if (!table->in_use)
        table->in_use= thd;
      error= do_exec_row(table);
      table->in_use = old_thd;
      switch (error)
      {
        /* Some recoverable errors */
      case HA_ERR_RECORD_CHANGED:
      case HA_ERR_KEY_NOT_FOUND:	/* Idempotency support: OK if
                                           tuple does not exist */
	error= 0;
      case 0:
	break;

      default:
	slave_print_msg(ERROR_LEVEL, rli, error,
                        "Error in %s event: row application failed",
                        get_type_str());
	thd->query_error= 1;
	break;
      }

      row_start= row_end;
    }
    DBUG_EXECUTE_IF("STOP_SLAVE_after_first_Rows_event",
                    rli->abort_slave=1;);
    error= do_after_row_operations(table, error);
    if (!cache_stmt)
    {
      DBUG_PRINT("info", ("Marked that we need to keep log"));
      thd->options|= OPTION_KEEP_LOG;
    }
  }

  if (error)
  {                     /* error has occured during the transaction */
    slave_print_msg(ERROR_LEVEL, rli, error,
                    "Error in %s event: error during transaction execution "
                    "on table %s.%s",
                    get_type_str(), table->s->db.str, 
                    table->s->table_name.str);
     /*
      If one day we honour --skip-slave-errors in row-based replication, and
      the error should be skipped, then we would clear mappings, rollback,
      close tables, but the slave SQL thread would not stop and then may
      assume the mapping is still available, the tables are still open...
      So then we should clear mappings/rollback/close here only if this is a
      STMT_END_F.
      For now we code, knowing that error is not skippable and so slave SQL
      thread is certainly going to stop.
    */
    thd->reset_current_stmt_binlog_row_based();
    rli->cleanup_context(thd, 1);
    thd->query_error= 1;
    DBUG_RETURN(error);
  }

  if (get_flags(STMT_END_F))
  {
    /*
      This is the end of a statement or transaction, so close (and
      unlock) the tables we opened when processing the
      Table_map_log_event starting the statement.

      OBSERVER.  This will clear *all* mappings, not only those that
      are open for the table. There is not good handle for on-close
      actions for tables.

      NOTE. Even if we have no table ('table' == 0) we still need to be
      here, so that we increase the group relay log position. If we didn't, we
      could have a group relay log position which lags behind "forever"
      (assume the last master's transaction is ignored by the slave because of
      replicate-ignore rules).
    */
    thd->binlog_flush_pending_rows_event(true);
    /*
      If this event is not in a transaction, the call below will, if some
      transactional storage engines are involved, commit the statement into
      them and flush the pending event to binlog.
      If this event is in a transaction, the call will do nothing, but a
      Xid_log_event will come next which will, if some transactional engines
      are involved, commit the transaction and flush the pending event to the
      binlog.
    */
    error= ha_autocommit_or_rollback(thd, 0);
    /*
      Now what if this is not a transactional engine? we still need to
      flush the pending event to the binlog; we did it with
      thd->binlog_flush_pending_rows_event(). Note that we imitate
      what is done for real queries: a call to
      ha_autocommit_or_rollback() (sometimes only if involves a
      transactional engine), and a call to be sure to have the pending
      event flushed.
    */

    thd->reset_current_stmt_binlog_row_based();
    rli->cleanup_context(thd, 0);
    rli->transaction_end(thd);

    if (error == 0)
    {
      /*
        Clear any errors pushed in thd->net.last_err* if for example "no key
        found" (as this is allowed). This is a safety measure; apparently
        those errors (e.g. when executing a Delete_rows_log_event of a
        non-existing row, like in rpl_row_mystery22.test,
        thd->net.last_error = "Can't find record in 't1'" and last_errno=1032)
        do not become visible. We still prefer to wipe them out.
      */
      thd->clear_error();
      error= Log_event::exec_event(rli);
    }
    else
      slave_print_msg(ERROR_LEVEL, rli, error,
                      "Error in %s event: commit of row events failed, "
                      "table `%s`.`%s`",
                      get_type_str(), table->s->db.str, 
                      table->s->table_name.str);
    DBUG_RETURN(error);
  }

  if (table && (table->s->primary_key == MAX_KEY) && !cache_stmt)
  {
    /*
      ------------ Temporary fix until WL#2975 is implemented ---------

      This event is not the last one (no STMT_END_F). If we stop now
      (in case of terminate_slave_thread()), how will we restart? We
      have to restart from Table_map_log_event, but as this table is
      not transactional, the rows already inserted will still be
      present, and idempotency is not guaranteed (no PK) so we risk
      that repeating leads to double insert. So we desperately try to
      continue, hope we'll eventually leave this buggy situation (by
      executing the final Rows_log_event). If we are in a hopeless
      wait (reached end of last relay log and nothing gets appended
      there), we timeout after one minute, and notify DBA about the
      problem.  When WL#2975 is implemented, just remove the member
      st_relay_log_info::unsafe_to_stop_at and all its occurences.
    */
    rli->unsafe_to_stop_at= time(0);
  }

  DBUG_ASSERT(error == 0);
  thd->clear_error();
  rli->inc_event_relay_log_pos();
  
  DBUG_RETURN(0);
}
#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifndef MYSQL_CLIENT
bool Rows_log_event::write_data_header(IO_CACHE *file)
{
  byte buf[ROWS_HEADER_LEN];	// No need to init the buffer
  DBUG_ASSERT(m_table_id != ~0UL);
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  {
                    int4store(buf + 0, m_table_id);
                    int2store(buf + 4, m_flags);
                    return (my_b_safe_write(file, buf, 6));
                  });
  int6store(buf + RW_MAPID_OFFSET, (ulonglong)m_table_id);
  int2store(buf + RW_FLAGS_OFFSET, m_flags);
  return (my_b_safe_write(file, buf, ROWS_HEADER_LEN));
}

bool Rows_log_event::write_data_body(IO_CACHE*file)
{
  /*
     Note that this should be the number of *bits*, not the number of
     bytes.
  */
  char sbuf[sizeof(m_width)];
  my_ptrdiff_t const data_size= m_rows_cur - m_rows_buf;

  char *const sbuf_end= net_store_length((char*) sbuf, (uint) m_width);
  DBUG_ASSERT(static_cast<my_size_t>(sbuf_end - sbuf) <= sizeof(sbuf));

  return (my_b_safe_write(file, reinterpret_cast<byte*>(sbuf),
                          sbuf_end - sbuf) ||
          my_b_safe_write(file, reinterpret_cast<byte*>(m_cols.bitmap),
                          no_bytes_in_map(&m_cols)) ||
          my_b_safe_write(file, m_rows_buf, data_size));
}
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Rows_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  char const *const flagstr=
    get_flags(STMT_END_F) ? " flags: STMT_END_F" : "";
  my_size_t bytes= my_snprintf(buf, sizeof(buf),
                               "table_id: %lu%s", m_table_id, flagstr);
  protocol->store(buf, bytes, &my_charset_bin);
}
#endif

/**************************************************************************
	Table_map_log_event member functions and support functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
  Mats says tbl->s lives longer than this event so it's ok to copy pointers
  (tbl->s->db etc) and not pointer content.
 */
#if !defined(MYSQL_CLIENT)
Table_map_log_event::Table_map_log_event(THD *thd, TABLE *tbl, ulong tid,
                                         bool is_transactional, uint16 flags)
  : Log_event(thd, 0, is_transactional),
    m_table(tbl),
    m_dbnam(tbl->s->db.str),
    m_dblen(m_dbnam ? tbl->s->db.length : 0),
    m_tblnam(tbl->s->table_name.str),
    m_tbllen(tbl->s->table_name.length),
    m_colcnt(tbl->s->fields), m_coltype(0),
    m_table_id(tid),
    m_flags(flags)
{
  DBUG_ASSERT(m_table_id != ~0UL);
  /*
    In TABLE_SHARE, "db" and "table_name" are 0-terminated (see this comment in
    table.cc / alloc_table_share():
      Use the fact the key is db/0/table_name/0
    As we rely on this let's assert it.
  */
  DBUG_ASSERT((tbl->s->db.str == 0) ||
              (tbl->s->db.str[tbl->s->db.length] == 0));
  DBUG_ASSERT(tbl->s->table_name.str[tbl->s->table_name.length] == 0);


  m_data_size=  TABLE_MAP_HEADER_LEN;
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master", m_data_size= 6;);
  m_data_size+= m_dblen + 2;	// Include length and terminating \0
  m_data_size+= m_tbllen + 2;	// Include length and terminating \0
  m_data_size+= 1 + m_colcnt;	// COLCNT and column types

  /* If malloc fails, catched in is_valid() */
  if ((m_memory= my_malloc(m_colcnt, MYF(MY_WME))))
  {
    m_coltype= reinterpret_cast<uchar*>(m_memory);
    for (unsigned int i= 0 ; i < m_table->s->fields ; ++i)
      m_coltype[i]= m_table->field[i]->type();
  }
}
#endif /* !defined(MYSQL_CLIENT) */

/*
  Constructor used by slave to read the event from the binary log.
 */
#if defined(HAVE_REPLICATION)
Table_map_log_event::Table_map_log_event(const char *buf, uint event_len,
                                         const Format_description_log_event
                                         *description_event)

  : Log_event(buf, description_event),
#ifndef MYSQL_CLIENT
  m_table(NULL),
#endif
  m_memory(NULL)
{
  DBUG_ENTER("Table_map_log_event::Table_map_log_event(const char*,uint,...)");

  uint8 common_header_len= description_event->common_header_len;
  uint8 post_header_len= description_event->post_header_len[TABLE_MAP_EVENT-1];
  DBUG_PRINT("info",("event_len=%ld, common_header_len=%d, post_header_len=%d",
                     event_len, common_header_len, post_header_len));

  DBUG_DUMP("event buffer", buf, event_len);

  /* Read the post-header */
  const char *post_start= buf + common_header_len;

  post_start+= TM_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    m_table_id= uint4korr(post_start);
    post_start+= 4;
  }
  else
  {
    DBUG_ASSERT(post_header_len == TABLE_MAP_HEADER_LEN);
    m_table_id= uint6korr(post_start);
    post_start+= TM_FLAGS_OFFSET;
  }

  DBUG_ASSERT(m_table_id != ~0UL);

  m_flags= uint2korr(post_start);

  /* Read the variable part of the event */
  const char *const vpart= buf + common_header_len + post_header_len;

  /* Extract the length of the various parts from the buffer */
  byte const* const ptr_dblen= (byte const*)vpart + 0;
  m_dblen= *(uchar*) ptr_dblen;

  /* Length of database name + counter + terminating null */
  byte const* const ptr_tbllen= ptr_dblen + m_dblen + 2;
  m_tbllen= *(uchar*) ptr_tbllen;

  /* Length of table name + counter + terminating null */
  byte const* const ptr_colcnt= ptr_tbllen + m_tbllen + 2;
  uchar *ptr_after_colcnt= (uchar*) ptr_colcnt;
  m_colcnt= net_field_length(&ptr_after_colcnt);

  DBUG_PRINT("info",("m_dblen=%d off=%d m_tbllen=%d off=%d m_colcnt=%d off=%d",
                     m_dblen, ptr_dblen-(const byte*)vpart, 
                     m_tbllen, ptr_tbllen-(const byte*)vpart,
                     m_colcnt, ptr_colcnt-(const byte*)vpart));

  /* Allocate mem for all fields in one go. If fails, catched in is_valid() */
  m_memory= my_multi_malloc(MYF(MY_WME),
			    &m_dbnam, m_dblen + 1,
			    &m_tblnam, m_tbllen + 1,
			    &m_coltype, m_colcnt,
			    NULL);

  if (m_memory)
  {
    /* Copy the different parts into their memory */
    strncpy(const_cast<char*>(m_dbnam), (const char*)ptr_dblen  + 1, m_dblen + 1);
    strncpy(const_cast<char*>(m_tblnam), (const char*)ptr_tbllen + 1, m_tbllen + 1);
    memcpy(m_coltype, ptr_after_colcnt, m_colcnt);
  }

  DBUG_VOID_RETURN;
}
#endif

Table_map_log_event::~Table_map_log_event()
{
  my_free(m_memory, MYF(MY_ALLOW_ZERO_PTR));
}

/*
  Return value is an error code, one of:

      -1     Failure to open table   [from open_tables()]
       0     Success
       1     No room for more tables [from set_table()]
       2     Out of memory           [from set_table()]
       3     Wrong table definition
       4     Daisy-chaining RBR with SBR not possible
 */

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int Table_map_log_event::exec_event(st_relay_log_info *rli)
{
  DBUG_ENTER("Table_map_log_event::exec_event(st_relay_log_info*)");

  DBUG_ASSERT(rli->sql_thd == thd);

  /* Step the query id to mark what columns that are actually used. */
  pthread_mutex_lock(&LOCK_thread_count);
  thd->query_id= next_query_id();
  pthread_mutex_unlock(&LOCK_thread_count);

  TABLE_LIST *table_list;
  char *db_mem, *tname_mem;
  void *const memory=
    my_multi_malloc(MYF(MY_WME),
                    &table_list, sizeof(TABLE_LIST),
                    &db_mem, NAME_LEN + 1,
                    &tname_mem, NAME_LEN + 1,
                    NULL);

  if (memory == NULL)
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  uint dummy_len;
  bzero(table_list, sizeof(*table_list));
  table_list->db = db_mem;
  table_list->alias= table_list->table_name = tname_mem;
  table_list->lock_type= TL_WRITE;
  table_list->next_global= table_list->next_local= 0;
  table_list->table_id= m_table_id;
  table_list->updating= 1;
  strmov(table_list->db, rpl_filter->get_rewrite_db(m_dbnam, &dummy_len));
  strmov(table_list->table_name, m_tblnam);

  int error= 0;

  if (!rpl_filter->db_ok(table_list->db) ||
      (rpl_filter->is_on() && !rpl_filter->tables_ok("", table_list)))
  {
    my_free((gptr) memory, MYF(MY_WME));
  }
  else
  {
    /*
      open_tables() reads the contents of thd->lex, so they must be
      initialized, so we should call lex_start(); to be even safer, we
      call mysql_init_query() which does a more complete set of inits.
    */
    mysql_init_query(thd, NULL, 0);
    /*
      Check if the slave is set to use SBR.  If so, it should switch
      to using RBR until the end of the "statement", i.e., next
      STMT_END_F or next error.
    */
    if (!thd->current_stmt_binlog_row_based &&
        mysql_bin_log.is_open() && (thd->options & OPTION_BIN_LOG))
    {
      thd->set_current_stmt_binlog_row_based();
    }

    /*
      Open the table if it is not already open and add the table to table map.
      Note that for any table that should not be replicated, a filter is needed.
    */
    uint count;
    if ((error= open_tables(thd, &table_list, &count, 0)))
    {
      if (thd->query_error || thd->is_fatal_error)
      {
        /*
          Error reporting borrowed from Query_log_event with many excessive
          simplifications (we don't honour --slave-skip-errors)
        */
        uint actual_error= thd->net.last_errno;
        slave_print_msg(ERROR_LEVEL, rli, actual_error,
                        "Error '%s' on opening table `%s`.`%s`",
                        (actual_error ? thd->net.last_error :
                         "unexpected success or fatal error"),
                        table_list->db, table_list->table_name);
        thd->query_error= 1;
      }
      goto err;
    }

    m_table= table_list->table;

    /*
      This will fail later otherwise, the 'in_use' field should be
      set to the current thread.
    */
    DBUG_ASSERT(m_table->in_use);

    table_def const def(m_coltype, m_colcnt);
    if (def.compatible_with(rli, m_table))
    {
      thd->query_error= 1;
      error= ERR_BAD_TABLE_DEF;
      goto err;
      /* purecov: end */
    }

    /*
      We record in the slave's information that the table should be
      locked by linking the table into the list of tables to lock, and
      tell the RLI that we are touching a table.
    */
    table_list->next_global= table_list->next_local= rli->tables_to_lock;
    rli->tables_to_lock= table_list;
    rli->tables_to_lock_count++;
    /* 'memory' is freed in clear_tables_to_lock */
  }

  /*
    We explicitly do not call Log_event::exec_event() here since we do not
    want the relay log position to be flushed to disk. The flushing will be
    done by the last Rows_log_event that either ends a statement (outside a
    transaction) or a transaction.

    A table map event can *never* end a transaction or a statement, so we
    just step the relay log position.
  */

  if (likely(!error))
    rli->inc_event_relay_log_pos();
  DBUG_RETURN(error);

err:
  my_free((gptr) memory, MYF(MY_WME));
  DBUG_RETURN(error);
}
#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifndef MYSQL_CLIENT
bool Table_map_log_event::write_data_header(IO_CACHE *file)
{
  DBUG_ASSERT(m_table_id != ~0UL);
  byte buf[TABLE_MAP_HEADER_LEN];
  DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                  {
                    int4store(buf + 0, m_table_id);
                    int2store(buf + 4, m_flags);
                    return (my_b_safe_write(file, buf, 6));
                  });
  int6store(buf + TM_MAPID_OFFSET, (ulonglong)m_table_id);
  int2store(buf + TM_FLAGS_OFFSET, m_flags);
  return (my_b_safe_write(file, buf, TABLE_MAP_HEADER_LEN));
}

bool Table_map_log_event::write_data_body(IO_CACHE *file)
{
  DBUG_ASSERT(m_dbnam != NULL);
  DBUG_ASSERT(m_tblnam != NULL);
  /* We use only one byte per length for storage in event: */
  DBUG_ASSERT(m_dblen < 128);
  DBUG_ASSERT(m_tbllen < 128);

  byte const dbuf[]= { m_dblen };
  byte const tbuf[]= { m_tbllen };

  char cbuf[sizeof(m_colcnt)];
  char *const cbuf_end= net_store_length((char*) cbuf, (uint) m_colcnt);
  DBUG_ASSERT(static_cast<my_size_t>(cbuf_end - cbuf) <= sizeof(cbuf));

  return (my_b_safe_write(file, dbuf,      sizeof(dbuf)) ||
          my_b_safe_write(file, (const byte*)m_dbnam,   m_dblen+1) ||
          my_b_safe_write(file, tbuf,      sizeof(tbuf)) ||
          my_b_safe_write(file, (const byte*)m_tblnam,  m_tbllen+1) ||
          my_b_safe_write(file, reinterpret_cast<byte*>(cbuf),
                          cbuf_end - (char*) cbuf) ||
          my_b_safe_write(file, reinterpret_cast<byte*>(m_coltype), m_colcnt));
 }
#endif

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)

/*
  Print some useful information for the SHOW BINARY LOG information
  field.
 */

#if defined(HAVE_REPLICATION) && !defined(MYSQL_CLIENT)
void Table_map_log_event::pack_info(Protocol *protocol)
{
    char buf[256];
    my_size_t bytes= my_snprintf(buf, sizeof(buf),
                                 "table_id: %lu (%s.%s)",
                              m_table_id, m_dbnam, m_tblnam);
    protocol->store(buf, bytes, &my_charset_bin);
}
#endif


#endif


#ifdef MYSQL_CLIENT
void Table_map_log_event::print(FILE *file, PRINT_EVENT_INFO *print_event_info)
{
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tTable_map: `%s`.`%s` mapped to number %lu\n",
            m_dbnam, m_tblnam, m_table_id);
    print_base64(file, print_event_info);
  }
}
#endif

/**************************************************************************
	Write_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
#if !defined(MYSQL_CLIENT)
Write_rows_log_event::Write_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                           ulong tid_arg,
                                           MY_BITMAP const *cols,
                                           bool is_transactional)
  : Rows_log_event(thd_arg, tbl_arg, tid_arg, cols, is_transactional)
{
}
#endif

/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Write_rows_log_event::Write_rows_log_event(const char *buf, uint event_len,
                                           const Format_description_log_event
                                           *description_event)
: Rows_log_event(buf, event_len, WRITE_ROWS_EVENT, description_event)
{
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int Write_rows_log_event::do_before_row_operations(TABLE *table)
{
  int error= 0;

  /*
    We are using REPLACE semantics and not INSERT IGNORE semantics
    when writing rows, that is: new rows replace old rows.  We need to
    inform the storage engine that it should use this behaviour.
  */

  /* Tell the storage engine that we are using REPLACE semantics. */
  thd->lex->duplicates= DUP_REPLACE;

  /*
    Pretend we're executing a REPLACE command: this is needed for
    InnoDB and NDB Cluster since they are not (properly) checking the
    lex->duplicates flag.
  */
  thd->lex->sql_command= SQLCOM_REPLACE;

  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);  // Needed for ndbcluster
  table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);  // Needed for ndbcluster
  table->file->extra(HA_EXTRA_IGNORE_NO_KEY);   // Needed for ndbcluster
  /*
    TODO: the cluster team (Tomas?) says that it's better if the engine knows
    how many rows are going to be inserted, then it can allocate needed memory
    from the start.
  */
  table->file->ha_start_bulk_insert(0);
  /*
    We need TIMESTAMP_NO_AUTO_SET otherwise ha_write_row() will not use fill
    any TIMESTAMP column with data from the row but instead will use
    the event's current time.
    As we replicate from TIMESTAMP to TIMESTAMP and slave has no extra
    columns, we know that all TIMESTAMP columns on slave will receive explicit
    data from the row, so TIMESTAMP_NO_AUTO_SET is ok.
    When we allow a table without TIMESTAMP to be replicated to a table having
    more columns including a TIMESTAMP column, or when we allow a TIMESTAMP
    column to be replicated into a BIGINT column and the slave's table has a
    TIMESTAMP column, then the slave's TIMESTAMP column will take its value
    from set_time() which we called earlier (consistent with SBR). And then in
    some cases we won't want TIMESTAMP_NO_AUTO_SET (will require some code to
    analyze if explicit data is provided for slave's TIMESTAMP columns).
  */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  return error;
}

int Write_rows_log_event::do_after_row_operations(TABLE *table, int error)
{
  if (error == 0)
    error= table->file->ha_end_bulk_insert();
  return error;
}

int Write_rows_log_event::do_prepare_row(THD *thd, RELAY_LOG_INFO *rli,
                                         TABLE *table,
                                         char const *row_start,
                                         char const **row_end)
{
  DBUG_ASSERT(table != NULL);
  DBUG_ASSERT(row_start && row_end);

  int error;
  error= unpack_row(rli,
                    table, m_width, table->record[0],
                    row_start, &m_cols, row_end, &m_master_reclength,
                    table->write_set);
  bitmap_copy(table->read_set, table->write_set);
  return error;
}

/*
  Check if there are more UNIQUE keys after the given key.
*/
static int
last_uniq_key(TABLE *table, uint keyno)
{
  while (++keyno < table->s->keys)
    if (table->key_info[keyno].flags & HA_NOSAME)
      return 0;
  return 1;
}

/* Anonymous namespace for template functions/classes */
namespace {

  /*
    Smart pointer that will automatically call my_afree (a macro) when
    the pointer goes out of scope.  This is used so that I do not have
    to remember to call my_afree() before each return.  There is no
    overhead associated with this, since all functions are inline.

    I (Matz) would prefer to use the free function as a template
    parameter, but that is not possible when the "function" is a
    macro.
  */
  template <class Obj>
  class auto_afree_ptr
  {
    Obj* m_ptr;
  public:
    auto_afree_ptr(Obj* ptr) : m_ptr(ptr) { }
    ~auto_afree_ptr() { if (m_ptr) my_afree(m_ptr); }
    void assign(Obj* ptr) {
      /* Only to be called if it hasn't been given a value before. */
      DBUG_ASSERT(m_ptr == NULL);
      m_ptr= ptr;
    }
    Obj* get() { return m_ptr; }
  };

}


/*
  Copy "extra" columns from record[1] to record[0].

  Copy the extra fields that are not present on the master but are
  present on the slave from record[1] to record[0].  This is used
  after fetching a record that are to be updated, either inside
  replace_record() or as part of executing an update_row().
 */
static int
copy_extra_record_fields(TABLE *table,
                         my_size_t master_reclength,
                         my_ptrdiff_t master_fields)
{
  DBUG_PRINT("info", ("Copying to %p from field %d at offset %u to field %d at offset %u", 
                      table->record[0], 
                      master_fields, master_reclength,
                      table->s->fields, table->s->reclength));
  if (master_reclength < table->s->reclength)
    bmove_align(table->record[0] + master_reclength,
                table->record[1] + master_reclength,
                table->s->reclength - master_reclength);
    
  /*
    Bit columns are special.  We iterate over all the remaining
    columns and copy the "extra" bits to the new record.  This is
    not a very good solution: it should be refactored on
    opportunity.

    REFACTORING SUGGESTION (Matz).  Introduce a member function
    similar to move_field_offset() called copy_field_offset() to
    copy field values and implement it for all Field subclasses. Use
    this function to copy data from the found record to the record
    that are going to be inserted.

    The copy_field_offset() function need to be a virtual function,
    which in this case will prevent copying an entire range of
    fields efficiently.
  */
  {
    Field **field_ptr= table->field + master_fields;
    for ( ; *field_ptr ; ++field_ptr)
    {
      /*
        Set the null bit according to the values in record[1]
       */
      if ((*field_ptr)->maybe_null() &&
          (*field_ptr)->is_null_in_record(reinterpret_cast<uchar*>(table->record[1])))
        (*field_ptr)->set_null();
      else
        (*field_ptr)->set_notnull();

      /*
        Do the extra work for special columns.
       */
      switch ((*field_ptr)->real_type())
      {
      default:
        /* Nothing to do */
        break;

      case FIELD_TYPE_BIT:
        Field_bit *f= static_cast<Field_bit*>(*field_ptr);
        my_ptrdiff_t const offset= table->record[1] - table->record[0];
        uchar const bits=
          get_rec_bits(f->bit_ptr + offset, f->bit_ofs, f->bit_len);
        set_rec_bits(bits, f->bit_ptr, f->bit_ofs, f->bit_len);
        break;
      }
    }
  }
  return 0;                                     // All OK
}

/*
  Replace the provided record in the database.

  SYNOPSIS
      replace_record()
      thd    Thread context for writing the record.
      table  Table to which record should be written.
      master_reclength
             Offset to first column that is not present on the master,
             alternatively the length of the record on the master
             side.

  RETURN VALUE
      Error code on failure, 0 on success.

  DESCRIPTION
      Similar to how it is done in mysql_insert(), we first try to do
      a ha_write_row() and of that fails due to duplicated keys (or
      indices), we do an ha_update_row() or a ha_delete_row() instead.
 */
static int
replace_record(THD *thd, TABLE *table,
               ulong const master_reclength,
               uint const master_fields)
{
  DBUG_ENTER("replace_record");
  DBUG_ASSERT(table != NULL && thd != NULL);

  int error;
  int keynum;
  auto_afree_ptr<char> key(NULL);

  while ((error= table->file->ha_write_row(table->record[0])))
  {
    if ((keynum= table->file->get_dup_key(error)) < 0)
    {
      /* We failed to retrieve the duplicate key */
      DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);
    }

    /*
       We need to retrieve the old row into record[1] to be able to
       either update or delete the offending record.  We either:

       - use rnd_pos() with a row-id (available as dupp_row) to the
         offending row, if that is possible (MyISAM and Blackhole), or else

       - use index_read_idx() with the key that is duplicated, to
         retrieve the offending row.
     */
    if (table->file->ha_table_flags() & HA_DUPLICATE_POS)
    {
      error= table->file->rnd_pos(table->record[1], table->file->dup_ref);
      if (error)
        DBUG_RETURN(error);
    }
    else
    {
      if (table->file->extra(HA_EXTRA_FLUSH_CACHE))
      {
        DBUG_RETURN(my_errno);
      }

      if (key.get() == NULL)
      {
        key.assign(static_cast<char*>(my_alloca(table->s->max_unique_length)));
        if (key.get() == NULL)
          DBUG_RETURN(ENOMEM);
      }

      key_copy((byte*)key.get(), table->record[0], table->key_info + keynum, 0);
      error= table->file->index_read_idx(table->record[1], keynum, 
                                         (const byte*)key.get(),
                                         table->key_info[keynum].key_length,
                                         HA_READ_KEY_EXACT);
      if (error)
        DBUG_RETURN(error);
    }

    /*
       Now, table->record[1] should contain the offending row.  That
       will enable us to update it or, alternatively, delete it (so
       that we can insert the new row afterwards).

       First we copy the columns into table->record[0] that are not
       present on the master from table->record[1], if there are any.
    */
    copy_extra_record_fields(table, master_reclength, master_fields);
    
    /*
       REPLACE is defined as either INSERT or DELETE + INSERT.  If
       possible, we can replace it with an UPDATE, but that will not
       work on InnoDB if FOREIGN KEY checks are necessary.

       I (Matz) am not sure of the reason for the last_uniq_key()
       check as, but I'm guessing that it's something along the
       following lines.

       Suppose that we got the duplicate key to be a key that is not
       the last unique key for the table and we perform an update:
       then there might be another key for which the unique check will
       fail, so we're better off just deleting the row and inserting
       the correct row.
     */
    if (last_uniq_key(table, keynum) &&
        !table->file->referenced_by_foreign_key())
    {
      error=table->file->ha_update_row(table->record[1],
                                       table->record[0]);
      DBUG_RETURN(error);
    }
    else
    {
      if ((error= table->file->ha_delete_row(table->record[1])))
        DBUG_RETURN(error);
      /* Will retry ha_write_row() with the offending row removed. */
    }
  }
  DBUG_RETURN(error);
}

int Write_rows_log_event::do_exec_row(TABLE *table)
{
  DBUG_ASSERT(table != NULL);
  int error= replace_record(thd, table, m_master_reclength, m_width);
  return error;
}
#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifdef MYSQL_CLIENT
void Write_rows_log_event::print(FILE *file, PRINT_EVENT_INFO* print_event_info)
{
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tWrite_rows: table id %lu", m_table_id);
    print_base64(file, print_event_info);
  }
}
#endif

/**************************************************************************
	Delete_rows_log_event member functions
**************************************************************************/

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
/*
  Compares table->record[0] and table->record[1]

  Returns TRUE if different.
*/
static bool record_compare(TABLE *table)
{
  if (table->s->blob_fields + table->s->varchar_fields == 0)
    return cmp_record(table,record[1]);
  /* Compare null bits */
  if (memcmp(table->null_flags,
	     table->null_flags+table->s->rec_buff_length,
	     table->s->null_bytes))
    return TRUE;				// Diff in NULL value
  /* Compare updated fields */
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->cmp_binary_offset(table->s->rec_buff_length))
      return TRUE;
  }
  return FALSE;
}


/*
  Find the row given by 'key', if the table has keys, or else use a table scan
  to find (and fetch) the row.  If the engine allows random access of the
  records, a combination of position() and rnd_pos() will be used.
 */

static int find_and_fetch_row(TABLE *table, byte *key)
{
  DBUG_ENTER("find_and_fetch_row(TABLE *table, byte *key, byte *record)");
  DBUG_PRINT("enter", ("table: 0x%lx, key: 0x%lx  record: 0x%lx",
		       (long) table, (long) key, (long) table->record[1]));

  DBUG_ASSERT(table->in_use != NULL);

  if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      table->s->primary_key < MAX_KEY)
  {
    /*
      Use a more efficient method to fetch the record given by
      table->record[0] if the engine allows it.  We first compute a
      row reference using the position() member function (it will be
      stored in table->file->ref) and the use rnd_pos() to position
      the "cursor" (i.e., record[0] in this case) at the correct row.
    */
    table->file->position(table->record[0]);
    DBUG_RETURN(table->file->rnd_pos(table->record[0], table->file->ref));
  }

  DBUG_ASSERT(table->record[1]);

  /* We need to retrieve all fields */
  /* TODO: Move this out from this function to main loop */
  table->use_all_columns();

  if (table->s->keys > 0)
  {
    int error;
    /* We have a key: search the table using the index */
    if (!table->file->inited && (error= table->file->ha_index_init(0, FALSE)))
      return error;
    
    /*
      We need to set the null bytes to ensure that the filler bit are
      all set when returning.  There are storage engines that just set
      the necessary bits on the bytes and don't set the filler bits
      correctly.
    */
    my_ptrdiff_t const pos=
      table->s->null_bytes > 0 ? table->s->null_bytes - 1 : 0;
    table->record[1][pos]= 0xFF;
    if ((error= table->file->index_read(table->record[1], key,
                                        table->key_info->key_length,
                                        HA_READ_KEY_EXACT)))
    {
      table->file->print_error(error, MYF(0));
      table->file->ha_index_end();
      DBUG_RETURN(error);
    }

    /*
      Below is a minor "optimization".  If the key (i.e., key number
      0) has the HA_NOSAME flag set, we know that we have found the
      correct record (since there can be no duplicates); otherwise, we
      have to compare the record with the one found to see if it is
      the correct one.

      CAVEAT! This behaviour is essential for the replication of,
      e.g., the mysql.proc table since the correct record *shall* be
      found using the primary key *only*.  There shall be no
      comparison of non-PK columns to decide if the correct record is
      found.  I can see no scenario where it would be incorrect to
      chose the row to change only using a PK or an UNNI.
    */
    if (table->key_info->flags & HA_NOSAME)
    {
      table->file->ha_index_end();
      DBUG_RETURN(0);
    }

    while (record_compare(table))
    {
      int error;
      /*
        We need to set the null bytes to ensure that the filler bit
        are all set when returning.  There are storage engines that
        just set the necessary bits on the bytes and don't set the
        filler bits correctly.
      */
      my_ptrdiff_t const pos=
        table->s->null_bytes > 0 ? table->s->null_bytes - 1 : 0;
      table->record[1][pos]= 0xFF;
      if ((error= table->file->index_next(table->record[1])))
      {
	table->file->print_error(error, MYF(0));
        table->file->ha_index_end();
	DBUG_RETURN(error);
      }
    }

    /*
      Have to restart the scan to be able to fetch the next row.
    */
    table->file->ha_index_end();
  }
  else
  {
    int restart_count= 0; // Number of times scanning has restarted from top
    int error;

    /* We don't have a key: search the table using rnd_next() */
    if ((error= table->file->ha_rnd_init(1)))
      return error;

    /* Continue until we find the right record or have made a full loop */
    do
    {
      /*
        We need to set the null bytes to ensure that the filler bit
        are all set when returning.  There are storage engines that
        just set the necessary bits on the bytes and don't set the
        filler bits correctly.
      */
      my_ptrdiff_t const pos=
        table->s->null_bytes > 0 ? table->s->null_bytes - 1 : 0;
      table->record[1][pos]= 0xFF;
      error= table->file->rnd_next(table->record[1]);
      switch (error)
      {
      case 0:
      case HA_ERR_RECORD_DELETED:
	break;

      case HA_ERR_END_OF_FILE:
	if (++restart_count < 2)
	  table->file->ha_rnd_init(1);
	break;

      default:
	table->file->print_error(error, MYF(0));
        table->file->ha_rnd_end();
	DBUG_RETURN(error);
      }
    }
    while (restart_count < 2 && record_compare(table));

    /*
      Have to restart the scan to be able to fetch the next row.
    */
    table->file->ha_rnd_end();

    DBUG_ASSERT(error == HA_ERR_END_OF_FILE || error == 0);
    DBUG_RETURN(error);
  }

  DBUG_RETURN(0);
}
#endif

/*
  Constructor used to build an event for writing to the binary log.
 */

#ifndef MYSQL_CLIENT
Delete_rows_log_event::Delete_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                             ulong tid, MY_BITMAP const *cols,
                                             bool is_transactional)
  : Rows_log_event(thd_arg, tbl_arg, tid, cols, is_transactional)
#ifdef HAVE_REPLICATION
  ,m_memory(NULL), m_key(NULL), m_after_image(NULL)
#endif
{
}
#endif /* #if !defined(MYSQL_CLIENT) */

/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Delete_rows_log_event::Delete_rows_log_event(const char *buf, uint event_len,
                                             const Format_description_log_event
                                             *description_event)
#if defined(MYSQL_CLIENT)
  : Rows_log_event(buf, event_len, DELETE_ROWS_EVENT, description_event)
#else
  : Rows_log_event(buf, event_len, DELETE_ROWS_EVENT, description_event),
    m_memory(NULL), m_key(NULL), m_after_image(NULL)
#endif
{
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int Delete_rows_log_event::do_before_row_operations(TABLE *table)
{
  DBUG_ASSERT(m_memory == NULL);

  if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      table->s->primary_key < MAX_KEY)
  {
    /*
      We don't need to allocate any memory for m_after_image and
      m_key since they are not used.
    */
    return 0;
  }

  int error= 0;

  if (table->s->keys > 0)
  {
    m_memory=
      my_multi_malloc(MYF(MY_WME),
		      &m_after_image, table->s->reclength,
		      &m_key, table->key_info->key_length,
		      NULL);
  }
  else
  {
    m_after_image= (byte*)my_malloc(table->s->reclength, MYF(MY_WME));
    m_memory= (gptr)m_after_image;
    m_key= NULL;
  }
  if (!m_memory)
    return HA_ERR_OUT_OF_MEM;

  return error;
}

int Delete_rows_log_event::do_after_row_operations(TABLE *table, int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  table->file->ha_index_or_rnd_end();
  my_free(m_memory, MYF(MY_ALLOW_ZERO_PTR)); // Free for multi_malloc
  m_memory= NULL;
  m_after_image= NULL;
  m_key= NULL;

  return error;
}

int Delete_rows_log_event::do_prepare_row(THD *thd, RELAY_LOG_INFO *rli,
                                          TABLE *table,
                                          char const *row_start,
                                          char const **row_end)
{
  int error;
  DBUG_ASSERT(row_start && row_end);
  /*
    This assertion actually checks that there is at least as many
    columns on the slave as on the master.
  */
  DBUG_ASSERT(table->s->fields >= m_width);

  error= unpack_row(rli,
                    table, m_width, table->record[0], 
                    row_start, &m_cols, row_end, &m_master_reclength,
                    table->read_set);
  /*
    If we will access rows using the random access method, m_key will
    be set to NULL, so we do not need to make a key copy in that case.
   */
  if (m_key)
  {
    KEY *const key_info= table->key_info;

    key_copy(m_key, table->record[0], key_info, 0);
  }

  return error;
}

int Delete_rows_log_event::do_exec_row(TABLE *table)
{
  int error;
  DBUG_ASSERT(table != NULL);

  if (!(error= find_and_fetch_row(table, m_key)))
  { 
    /*
      Now we should have the right row to delete.  We are using
      record[0] since it is guaranteed to point to a record with the
      correct value.
    */
    error= table->file->ha_delete_row(table->record[0]);
  }
  return error;
}

#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifdef MYSQL_CLIENT
void Delete_rows_log_event::print(FILE *file,
                                  PRINT_EVENT_INFO* print_event_info)
{
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tDelete_rows: table id %lu", m_table_id);
    print_base64(file, print_event_info);
  }
}
#endif


/**************************************************************************
	Update_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
#if !defined(MYSQL_CLIENT)
Update_rows_log_event::Update_rows_log_event(THD *thd_arg, TABLE *tbl_arg,
                                             ulong tid, MY_BITMAP const *cols,
                                             bool is_transactional)
: Rows_log_event(thd_arg, tbl_arg, tid, cols, is_transactional)
#ifdef HAVE_REPLICATION
  , m_memory(NULL), m_key(NULL)
#endif
{
}
#endif /* !defined(MYSQL_CLIENT) */

/*
  Constructor used by slave to read the event from the binary log.
 */
#ifdef HAVE_REPLICATION
Update_rows_log_event::Update_rows_log_event(const char *buf, uint event_len,
                                             const
                                             Format_description_log_event
                                             *description_event)
#if defined(MYSQL_CLIENT)
  : Rows_log_event(buf, event_len, UPDATE_ROWS_EVENT, description_event)
#else
  : Rows_log_event(buf, event_len, UPDATE_ROWS_EVENT, description_event),
    m_memory(NULL), m_key(NULL)
#endif
{
}
#endif

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int Update_rows_log_event::do_before_row_operations(TABLE *table)
{
  DBUG_ASSERT(m_memory == NULL);

  int error= 0;

  if (table->s->keys > 0)
  {
    m_memory=
      my_multi_malloc(MYF(MY_WME),
		      &m_after_image, table->s->reclength,
		      &m_key, table->key_info->key_length,
		      NULL);
  }
  else
  {
    m_after_image= (byte*)my_malloc(table->s->reclength, MYF(MY_WME));
    m_memory= (gptr)m_after_image;
    m_key= NULL;
  }
  if (!m_memory)
    return HA_ERR_OUT_OF_MEM;

  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  return error;
}

int Update_rows_log_event::do_after_row_operations(TABLE *table, int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  table->file->ha_index_or_rnd_end();
  my_free(m_memory, MYF(MY_ALLOW_ZERO_PTR));
  m_memory= NULL;
  m_after_image= NULL;
  m_key= NULL;

  return error;
}

int Update_rows_log_event::do_prepare_row(THD *thd, RELAY_LOG_INFO *rli,
                                          TABLE *table,
                                          char const *row_start,
                                          char const **row_end)
{
  int error;
  DBUG_ASSERT(row_start && row_end);
  /*
    This assertion actually checks that there is at least as many
    columns on the slave as on the master.
  */
  DBUG_ASSERT(table->s->fields >= m_width);

  /* record[0] is the before image for the update */
  error= unpack_row(rli,
                    table, m_width, table->record[0],
                    row_start, &m_cols, row_end, &m_master_reclength,
                    table->read_set);
  row_start = *row_end;
  /* m_after_image is the after image for the update */
  error= unpack_row(rli,
                    table, m_width, m_after_image,
                    row_start, &m_cols, row_end, &m_master_reclength,
                    table->write_set);

  /*
    If we will access rows using the random access method, m_key will
    be set to NULL, so we do not need to make a key copy in that case.
   */
  if (m_key)
  {
    KEY *const key_info= table->key_info;

    key_copy(m_key, table->record[0], key_info, 0);
  }

  return error;
}

int Update_rows_log_event::do_exec_row(TABLE *table)
{
  DBUG_ASSERT(table != NULL);

  int error= find_and_fetch_row(table, m_key);
  if (error)
    return error;

  /*
    We have to ensure that the new record (i.e., the after image) is
    in record[0] and the old record (i.e., the before image) is in
    record[1].  This since some storage engines require this (for
    example, the partition engine).

    Since find_and_fetch_row() puts the fetched record (i.e., the old
    record) in record[1], we can keep it there. We put the new record
    (i.e., the after image) into record[0], and copy the fields that
    are on the slave (i.e., in record[1]) into record[0], effectively
    overwriting the default values that where put there by the
    unpack_row() function.
  */
  bmove_align(table->record[0], m_after_image, table->s->reclength);
  copy_extra_record_fields(table, m_master_reclength, m_width);

  /*
    Now we have the right row to update.  The old row (the one we're
    looking for) is in record[1] and the new row has is in record[0].
    We also have copied the original values already in the slave's
    database into the after image delivered from the master.
  */
  error= table->file->ha_update_row(table->record[1], table->record[0]);

  return error;
}
#endif /* !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION) */

#ifdef MYSQL_CLIENT
void Update_rows_log_event::print(FILE *file,
				  PRINT_EVENT_INFO* print_event_info)
{
  if (!print_event_info->short_form)
  {
    print_header(file, print_event_info);
    fprintf(file, "\tUpdate_rows: table id %lu", m_table_id);
    print_base64(file, print_event_info);
  }
}
#endif

#endif /* defined(HAVE_ROW_BASED_REPLICATION) */
