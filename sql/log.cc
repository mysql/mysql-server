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


/* logging of commands */

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_repl.h"

#include <my_dir.h>
#include <stdarg.h>
#include <m_ctype.h>				// For test_if_number

MYSQL_LOG mysql_log,mysql_update_log,mysql_slow_log,mysql_bin_log;
extern I_List<i_string> binlog_do_db, binlog_ignore_db;

static bool test_if_number(const char *str,
			   long *res, bool allow_wildcards);

/****************************************************************************
** Find a uniq filename for 'filename.#'.
** Set # to a number as low as possible
** returns != 0 if not possible to get uniq filename
****************************************************************************/

static int find_uniq_filename(char *name)
{
  long		number;
  uint		i,length;
  char		buff[FN_REFLEN];
  struct st_my_dir *dir_info;
  reg1 struct fileinfo *file_info;
  ulong		max_found=0;
  DBUG_ENTER("find_uniq_filename");

  length=dirname_part(buff,name);
  char *start=name+length,*end=strend(start);
  *end='.';
  length= (uint) (end-start+1);

  if (!(dir_info = my_dir(buff,MYF(MY_DONT_SORT))))
  {						// This shouldn't happen
    strmov(end,".1");				// use name+1
    DBUG_RETURN(0);
  }
  file_info= dir_info->dir_entry;
  for (i=dir_info->number_off_files ; i-- ; file_info++)
  {
    if (bcmp(file_info->name,start,length) == 0 &&
	test_if_number(file_info->name+length, &number,0))
    {
      set_if_bigger(max_found,(ulong) number);
    }
  }
  my_dirend(dir_info);

  *end++='.';
  sprintf(end,"%03ld",max_found+1);
  DBUG_RETURN(0);
}

MYSQL_LOG::MYSQL_LOG(): last_time(0), query_start(0),index_file(-1),
			name(0), log_type(LOG_CLOSED),write_error(0),
			inited(0), opened(0), no_rotate(0)
{
  /*
    We don't want to intialize LOCK_Log here as the thread system may
    not have been initailized yet. We do it instead at 'open'.
  */
  index_file_name[0] = 0;
  bzero((char*) &log_file,sizeof(log_file));
}

MYSQL_LOG::~MYSQL_LOG()
{
  if (inited)
  {
    (void) pthread_mutex_destroy(&LOCK_log);
    (void) pthread_mutex_destroy(&LOCK_index);
  }
}

void MYSQL_LOG::set_index_file_name(const char* index_file_name)
{
  if (index_file_name)
    fn_format(this->index_file_name,index_file_name,mysql_data_home,"-index",
	      4);
  else
    this->index_file_name[0] = 0;
}


int MYSQL_LOG::generate_new_name(char *new_name, const char *log_name)
{      
  if (log_type == LOG_NORMAL)
    fn_format(new_name,log_name,mysql_data_home,"",4);
  else
  {
    fn_format(new_name,log_name,mysql_data_home,"",4);
    if (!fn_ext(log_name)[0])
    {
      if (find_uniq_filename(new_name))
      {
	sql_print_error(ER(ER_NO_UNIQUE_LOGFILE), log_name);
	return 1;
      }
    }
  }
  return 0;
}


void MYSQL_LOG::open(const char *log_name, enum_log_type log_type_arg,
		     const char *new_name)
{
  MY_STAT tmp_stat;
  char buff[512];
  File file= -1;
  bool do_magic;

  if (!inited)
  {
    inited=1;
    (void) pthread_mutex_init(&LOCK_log,NULL);
    (void) pthread_mutex_init(&LOCK_index, NULL);
    if (log_type_arg == LOG_BIN && *fn_ext(log_name))
      no_rotate = 1;
  }
  
  log_type=log_type_arg;
  if (!(name=my_strdup(log_name,MYF(MY_WME))))
    goto err;
  if (new_name)
    strmov(log_file_name,new_name);
  else if (generate_new_name(log_file_name, name))
    goto err;

  if (log_type == LOG_BIN && !index_file_name[0])
    fn_format(index_file_name, name, mysql_data_home, ".index", 6);
  
  db[0]=0;
  do_magic = ((log_type == LOG_BIN) && !my_stat(log_file_name,
						&tmp_stat, MYF(0)));
  
  if ((file=my_open(log_file_name,O_APPEND | O_WRONLY | O_BINARY|O_CREAT,
		    MYF(MY_WME | ME_WAITTANG))) < 0 ||
      init_io_cache(&log_file, file, IO_SIZE, WRITE_CACHE,
		    my_tell(file,MYF(MY_WME)), 0, MYF(MY_WME | MY_NABP)))
    goto err;

  if (log_type == LOG_NORMAL)
  {
    char *end;
#ifdef __NT__
    sprintf(buff, "%s, Version: %s, started with:\nTCP Port: %d, Named Pipe: %s\n", my_progname, server_version, mysql_port, mysql_unix_port);
#else
    sprintf(buff, "%s, Version: %s, started with:\nTcp port: %d  Unix socket: %s\n", my_progname,server_version,mysql_port,mysql_unix_port);
#endif
    end=strmov(strend(buff),"Time                 Id Command    Argument\n");
    if (my_b_write(&log_file,buff,(uint) (end-buff)) ||
	flush_io_cache(&log_file))
      goto err;
  }
  else if (log_type == LOG_NEW)
  {
    time_t skr=time(NULL);
    struct tm tm_tmp;
    localtime_r(&skr,&tm_tmp);
    sprintf(buff,"# %s, Version: %s at %02d%02d%02d %2d:%02d:%02d\n",
	    my_progname,server_version,
	    tm_tmp.tm_year % 100,
	    tm_tmp.tm_mon+1,
	    tm_tmp.tm_mday,
	    tm_tmp.tm_hour,
	    tm_tmp.tm_min,
	    tm_tmp.tm_sec);
    if (my_b_write(&log_file,buff,(uint) strlen(buff)) ||
	flush_io_cache(&log_file))
      goto err;
  }
  else if (log_type == LOG_BIN)
  {
    // Explanation of the boolean black magic:
    //
    // if we are supposed to write magic number try write
    // clean up if failed
    // then if index_file has not been previously opened, try to open it
    // clean up if failed

    if ((do_magic && my_b_write(&log_file, (byte*) BINLOG_MAGIC, 4)) ||
	(index_file < 0 && 
	 (index_file = my_open(index_file_name,
			       O_APPEND | O_BINARY | O_RDWR |O_CREAT,
			       MYF(MY_WME))) < 0))
      goto err;
    Start_log_event s;
    s.write(&log_file);
    flush_io_cache(&log_file);
    pthread_mutex_lock(&LOCK_index);
    my_write(index_file, log_file_name,strlen(log_file_name), MYF(0));
    my_write(index_file, "\n",1, MYF(0));
    pthread_mutex_unlock(&LOCK_index);
  }
  return;

err:
  if (file >= 0)
    my_close(file,MYF(0));
  end_io_cache(&log_file);
  x_free(name); name=0;
  log_type=LOG_CLOSED;

  return;
  
}

int MYSQL_LOG::get_current_log(LOG_INFO* linfo)
{
  pthread_mutex_lock(&LOCK_log);
  strmake(linfo->log_file_name, log_file_name, sizeof(linfo->log_file_name)-1);
  linfo->pos = my_b_tell(&log_file);
  pthread_mutex_unlock(&LOCK_log);
  return 0;
}

// if log_name is "" we stop at the first entry
int MYSQL_LOG::find_first_log(LOG_INFO* linfo, const char* log_name)
{
  if (index_file < 0)
    return LOG_INFO_INVALID;
  int error = 0;
  char* fname = linfo->log_file_name;
  uint log_name_len = (uint) strlen(log_name);
  IO_CACHE io_cache;

  // mutex needed because we need to make sure the file pointer does not move
  // from under our feet
  pthread_mutex_lock(&LOCK_index);
  if (init_io_cache(&io_cache, index_file, IO_SIZE, READ_CACHE, (my_off_t) 0,
		    0, MYF(MY_WME)))
  {
    error = LOG_INFO_SEEK;
    goto err;
  }
  for(;;)
  {
    uint length;
    if (!(length=my_b_gets(&io_cache, fname, FN_REFLEN)))
    {
      error = !io_cache.error ? LOG_INFO_EOF : LOG_INFO_IO;
      goto err;
    }

    // if the log entry matches, empty string matching anything
    if (!log_name_len ||
	(log_name_len == length-1 && fname[log_name_len] == '\n' &&
	 !memcmp(fname, log_name, log_name_len)))
    {
      fname[length-1]=0;			// remove last \n
      linfo->index_file_offset = my_b_tell(&io_cache);
      break;
    }
  }
  error = 0;

err:
  pthread_mutex_unlock(&LOCK_index);
  end_io_cache(&io_cache);
  return error;
     
}


int MYSQL_LOG::find_next_log(LOG_INFO* linfo)
{
  // mutex needed because we need to make sure the file pointer does not move
  // from under our feet
  if (index_file < 0) return LOG_INFO_INVALID;
  int error = 0;
  char* fname = linfo->log_file_name;
  IO_CACHE io_cache;
  uint length;

  pthread_mutex_lock(&LOCK_index);
  if (init_io_cache(&io_cache, index_file, IO_SIZE, 
		    READ_CACHE, (my_off_t) linfo->index_file_offset, 0,
		    MYF(MY_WME)))
  {
    error = LOG_INFO_SEEK;
    goto err;
  }
  if (!(length=my_b_gets(&io_cache, fname, FN_REFLEN)))
  {
    error = !io_cache.error ? LOG_INFO_EOF : LOG_INFO_IO;
    goto err;
  }
  fname[length-1]=0;				// kill /n
  linfo->index_file_offset = my_b_tell(&io_cache);
  error = 0;

err:
  pthread_mutex_unlock(&LOCK_index);
  end_io_cache(&io_cache);
  return error;
}

 
int MYSQL_LOG::purge_logs(THD* thd, const char* to_log)
{
  if (index_file < 0) return LOG_INFO_INVALID;
  if (no_rotate) return LOG_INFO_PURGE_NO_ROTATE;
  int error;
  char fname[FN_REFLEN];
  char *p;
  uint fname_len, i;
  bool logs_to_purge_inited = 0, logs_to_keep_inited = 0, found_log = 0;
  DYNAMIC_ARRAY logs_to_purge, logs_to_keep;
  my_off_t purge_offset ;
  LINT_INIT(purge_offset);
  IO_CACHE io_cache;
  
  pthread_mutex_lock(&LOCK_index);
  
  if (init_io_cache(&io_cache,index_file, IO_SIZE*2, READ_CACHE, (my_off_t) 0,
		    0, MYF(MY_WME)))
  {
    error = LOG_INFO_MEM;
    goto err;
  }
  if (init_dynamic_array(&logs_to_purge, sizeof(char*), 1024, 1024))
  {
    error = LOG_INFO_MEM;
    goto err;
  }
  logs_to_purge_inited = 1;
  
  if (init_dynamic_array(&logs_to_keep, sizeof(char*), 1024, 1024))
  {
    error = LOG_INFO_MEM;
    goto err;
  }
  logs_to_keep_inited = 1;

  
  for(;;)
  {
    my_off_t init_purge_offset= my_b_tell(&io_cache);
    if (!(fname_len=my_b_gets(&io_cache, fname, FN_REFLEN)))
    {
      if(!io_cache.error)
	break;
      error = LOG_INFO_IO;
      goto err;
    }

    fname[--fname_len]=0;			// kill \n
    if(!memcmp(fname, to_log, fname_len + 1 ))
    {
      found_log = 1;
      purge_offset = init_purge_offset;
    }
      
    // if one of the logs before the target is in use
    if(!found_log && log_in_use(fname))
    {
      error = LOG_INFO_IN_USE;
      goto err;
    }
      
    if (!(p = sql_memdup(fname, fname_len+1)) ||
	insert_dynamic(found_log ? &logs_to_keep : &logs_to_purge,
		       (gptr) &p))
    {
      error = LOG_INFO_MEM;
      goto err;
    }
  }
  
  end_io_cache(&io_cache);
  if(!found_log)
  {
    error = LOG_INFO_EOF;
    goto err;
  }
  
  for(i = 0; i < logs_to_purge.elements; i++)
  {
    char* l;
    get_dynamic(&logs_to_purge, (gptr)&l, i);
    if (my_delete(l, MYF(MY_WME)))
      sql_print_error("Error deleting %s during purge", l);
  }
  
  // if we get killed -9 here, the sysadmin would have to do a small
  // vi job on the log index file after restart - otherwise, this should
  // be safe
#ifdef HAVE_FTRUNCATE
  if (ftruncate(index_file,0))
  {
    sql_print_error("Ouch! Could not truncate the binlog index file \
during log purge for write");
    error = LOG_INFO_FATAL;
    goto err;
  }
  my_seek(index_file, 0, MY_SEEK_CUR,MYF(MY_WME));
#else
  my_close(index_file, MYF(MY_WME));
  my_delete(index_file_name, MYF(MY_WME));
  if(!(index_file = my_open(index_file_name,
			    O_BINARY | O_RDWR | O_APPEND |O_CREAT,
			    MYF(MY_WME))))
  {
    sql_print_error("Ouch! Could not re-open the binlog index file \
during log purge for write");
    error = LOG_INFO_FATAL;
    goto err;
  }
#endif
  
  for(i = 0; i < logs_to_keep.elements; i++)
  {
    char* l;
    get_dynamic(&logs_to_keep, (gptr)&l, i);
    if (my_write(index_file, l, strlen(l), MYF(MY_WME|MY_NABP)) ||
	my_write(index_file, "\n", 1, MYF(MY_WME|MY_NABP)))
    {
      error = LOG_INFO_FATAL;
      goto err;
    }
  }

  // now update offsets
  adjust_linfo_offsets(purge_offset);
  error = 0;

err:
  pthread_mutex_unlock(&LOCK_index);
  if(logs_to_purge_inited)
    delete_dynamic(&logs_to_purge);
  if(logs_to_keep_inited)
    delete_dynamic(&logs_to_keep);
  end_io_cache(&io_cache);
  return error;
}


// we assume that buf has at least FN_REFLEN bytes alloced
void MYSQL_LOG::make_log_name(char* buf, const char* log_ident)
{
  buf[0] = 0;					// In case of error
  if (inited)
  {
    int dir_len = dirname_length(log_file_name); 
    int ident_len = (uint) strlen(log_ident);
    if (dir_len + ident_len + 1 > FN_REFLEN)
      return; // protection agains malicious buffer overflow
      
    memcpy(buf, log_file_name, dir_len);
    // copy filename + end null
    memcpy(buf + dir_len, log_ident, ident_len + 1);
  }
}

bool MYSQL_LOG::is_active(const char* log_file_name)
{
  return inited && !strcmp(log_file_name, this->log_file_name);
}

void MYSQL_LOG::new_file()
{
  // only rotate open logs that are marked non-rotatable
  // (binlog with constant name are non-rotatable)
  if (is_open() && ! no_rotate)
  {
    char new_name[FN_REFLEN], *old_name=name;
    VOID(pthread_mutex_lock(&LOCK_log));
    if (generate_new_name(new_name, name))
    {
      VOID(pthread_mutex_unlock(&LOCK_log));
      return;					// Something went wrong
    }
    if (log_type == LOG_BIN)
    {
      /*
	We log the whole file name for log file as the user may decide
	to change base names at some point.
      */
      Rotate_log_event r(new_name+dirname_length(new_name));
      r.write(&log_file);
      VOID(pthread_cond_broadcast(&COND_binlog_update));
    }
    name=0;
    close();
    open(old_name, log_type, new_name);
    my_free(old_name,MYF(0));
    last_time=query_start=0;
    write_error=0;
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}


void MYSQL_LOG::write(THD *thd,enum enum_server_command command,
		      const char *format,...)
{
  if (is_open() && (what_to_log & (1L << (uint) command)))
  {
    va_list args;
    va_start(args,format);
    char buff[32];
    VOID(pthread_mutex_lock(&LOCK_log));

    /* Test if someone closed after the is_open test */
    if (log_type != LOG_CLOSED)
    {
      time_t skr;
      ulong id;
      int error=0;
      if (thd)
      {						// Normal thread
	if ((thd->options & OPTION_LOG_OFF) &&
	    (thd->master_access & PROCESS_ACL))
	{
	  VOID(pthread_mutex_unlock(&LOCK_log));
	  return;				// No logging
	}
	id=thd->thread_id;
	if (thd->user_time || !(skr=thd->query_start()))
	  skr=time(NULL);			// Connected
      }
      else
      {						// Log from connect handler
	skr=time(NULL);
	id=0;
      }
      if (skr != last_time)
      {
	last_time=skr;
	struct tm tm_tmp;
	struct tm *start;
	localtime_r(&skr,&tm_tmp);
	start=&tm_tmp;
	/* Note that my_b_write() assumes it knows the length for this */
	sprintf(buff,"%02d%02d%02d %2d:%02d:%02d\t",
		start->tm_year % 100,
		start->tm_mon+1,
		start->tm_mday,
		start->tm_hour,
		start->tm_min,
		start->tm_sec);
	if (my_b_write(&log_file,buff,16))
	  error=errno;
      }
      else if (my_b_write(&log_file,"\t\t",2) < 0)
	error=errno;
      sprintf(buff,"%7ld %-10.10s", id,command_name[(uint) command]);
      if (my_b_write(&log_file,buff,strlen(buff)))
	error=errno;
      if (format)
      {
	if (my_b_write(&log_file," ",1) ||
	    my_b_printf(&log_file,format,args) == (uint) -1)
	  error=errno;
      }
      if (my_b_write(&log_file,"\n",1) ||
	  flush_io_cache(&log_file))
	error=errno;
      if (error && ! write_error)
      {
	write_error=1;
	sql_print_error(ER(ER_ERROR_ON_WRITE),name,error);
      }
    }
    va_end(args);
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}

/* Write to binary log in a format to be used for replication */

void MYSQL_LOG::write(Query_log_event* event_info)
{
  if (is_open())
  {
    VOID(pthread_mutex_lock(&LOCK_log));
    if (is_open())
    {
      THD *thd=event_info->thd;
      if ((!(thd->options & OPTION_BIN_LOG) &&
	   thd->master_access & PROCESS_ACL) ||
	  !db_ok(event_info->db, binlog_do_db, binlog_ignore_db))
      {
	VOID(pthread_mutex_unlock(&LOCK_log));
	return;
      }
	  
      if (thd->last_insert_id_used)
      {
	Intvar_log_event e((uchar)LAST_INSERT_ID_EVENT, thd->last_insert_id);
	if (e.write(&log_file))
	{
	  sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
	  goto err;
	}
      }
      if (thd->insert_id_used)
      {
	Intvar_log_event e((uchar)INSERT_ID_EVENT, thd->last_insert_id);
	if (e.write(&log_file))
	{
	  sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
	  goto err;
	}
      }
      if (thd->convert_set)
      {
	char buf[1024] = "SET CHARACTER SET ";
	char* p = strend(buf);
	p = strmov(p, thd->convert_set->name);
	int save_query_length = thd->query_length;
	// just in case somebody wants it later
	thd->query_length = (uint)(p - buf);
	Query_log_event e(thd, buf);
	if (e.write(&log_file))
	{
	  sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
	  goto err;
	}
	thd->query_length = save_query_length; // clean up
      }
      if (event_info->write(&log_file) || flush_io_cache(&log_file))
      {
	sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
      }
  err:
      VOID(pthread_cond_broadcast(&COND_binlog_update));
    }
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}

void MYSQL_LOG::write(Load_log_event* event_info)
{
  if (is_open())
  {
    VOID(pthread_mutex_lock(&LOCK_log));
    if (is_open())
    {
      THD *thd=event_info->thd;
      if ((thd->options & OPTION_BIN_LOG) ||
	  !(thd->master_access & PROCESS_ACL))
      {
	if (event_info->write(&log_file) || flush_io_cache(&log_file))
	  sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
	VOID(pthread_cond_broadcast(&COND_binlog_update));
      }
    }
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}


/* Write update log in a format suitable for incremental backup */

void MYSQL_LOG::write(THD *thd,const char *query, uint query_length,
		      time_t query_start)
{
  if (is_open())
  {
    time_t current_time;
    VOID(pthread_mutex_lock(&LOCK_log));
    if (is_open())
    {						// Safety agains reopen
      int error=0;
      char buff[80],*end;
      end=buff;
      if (!(thd->options & OPTION_UPDATE_LOG) &&
	  (thd->master_access & PROCESS_ACL))
      {
	VOID(pthread_mutex_unlock(&LOCK_log));
	return;
      }
      if (specialflag & SPECIAL_LONG_LOG_FORMAT)
      {
	current_time=time(NULL);
	if (current_time != last_time)
	{
	  last_time=current_time;
	  struct tm tm_tmp;
	  struct tm *start;
	  char buff[32];
	  localtime_r(&current_time,&tm_tmp);
	  start=&tm_tmp;
	  /* Note that my_b_write() assumes it knows the length for this */
	  sprintf(buff,"# Time: %02d%02d%02d %2d:%02d:%02d\n",
		  start->tm_year % 100,
		  start->tm_mon+1,
		  start->tm_mday,
		  start->tm_hour,
		  start->tm_min,
		  start->tm_sec);
	  if (my_b_write(&log_file,buff,24))
	    error=errno;
	}
	if (my_b_printf(&log_file, "# User@Host: %s [%s] @ %s [%s]\n",
			thd->priv_user,
			thd->user,
			thd->host ? thd->host : "",
			thd->ip ? thd->ip : ""))
	  error=errno;
      }
      if (query_start)
      {
	/* For slow query log */
	if (!(specialflag & SPECIAL_LONG_LOG_FORMAT))
	  current_time=time(NULL);
	if (my_b_printf(&log_file,
			"# Time: %lu  Lock_time: %lu  Rows_sent: %lu\n",
			(ulong) (current_time - query_start),
			(ulong) (thd->time_after_lock - query_start),
			(ulong) thd->sent_row_count))
	    error=errno;
      }
      if (thd->db && strcmp(thd->db,db))
      {						// Database changed
	if (my_b_printf(&log_file,"use %s;\n",thd->db))
	  error=errno;
	strmov(db,thd->db);
      }
      if (thd->last_insert_id_used)
      {
	end=strmov(end,",last_insert_id=");
	end=longlong10_to_str((longlong) thd->current_insert_id,end,-10);
      }
      // Save value if we do an insert.
      if (thd->insert_id_used)
      {
	if (specialflag & SPECIAL_LONG_LOG_FORMAT)
	{
	  end=strmov(end,",insert_id=");
	  end=longlong10_to_str((longlong) thd->last_insert_id,end,-10);
	}
      }
      if (thd->query_start_used)
      {
	if (query_start != thd->query_start())
	{
	  query_start=thd->query_start();
	  end=strmov(end,",timestamp=");
	  end=int10_to_str((long) query_start,end,10);
	}
      }
      if (end != buff)
      {
	*end++=';';
	*end++='\n';
	*end=0;
	if (my_b_write(&log_file,"SET ",4) ||
	    my_b_write(&log_file,buff+1,(uint) (end-buff)-1))
	  error=errno;
      }
      if (!query)
      {
	query="#adminstrator command";
	query_length=21;
      }
      if (my_b_write(&log_file,(byte*) query,query_length) ||
	  my_b_write(&log_file,";\n",2) ||
	  flush_io_cache(&log_file))
	error=errno;
      if (error && ! write_error)
      {
	write_error=1;
	sql_print_error(ER(ER_ERROR_ON_WRITE),name,error);
      }
    }
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}

#ifdef TO_BE_REMOVED
void MYSQL_LOG::flush()
{
  if (is_open())
    if (flush_io_cache(log_file) && ! write_error)
    {
      write_error=1;
      sql_print_error(ER(ER_ERROR_ON_WRITE),name,errno);
    }
}
#endif


void MYSQL_LOG::close(bool exiting)
{					// One can't set log_type here!
  if (is_open())
  {
    File file=log_file.file;
    if (log_type == LOG_BIN)
    {
      Stop_log_event s;
      s.write(&log_file);
      VOID(pthread_cond_broadcast(&COND_binlog_update));
    }
    end_io_cache(&log_file);
    if (my_close(file,MYF(0)) < 0 && ! write_error)
    {
      write_error=1;
      sql_print_error(ER(ER_ERROR_ON_WRITE),name,errno);
    }
  }
  if (exiting && index_file >= 0)
  {
    if (my_close(index_file,MYF(0)) < 0 && ! write_error)
    {
      write_error=1;
      sql_print_error(ER(ER_ERROR_ON_WRITE),name,errno);
    }
    index_file=-1;
    log_type=LOG_CLOSED;
  }
  safeFree(name);
}


	/* Check if a string is a valid number */
	/* Output: TRUE -> number */

static bool test_if_number(register const char *str,
			   long *res, bool allow_wildcards)
{
  reg2 int flag;
  const char *start;
  DBUG_ENTER("test_if_number");

  flag=0; start=str;
  while (*str++ == ' ') ;
  if (*--str == '-' || *str == '+')
    str++;
  while (isdigit(*str) || (allow_wildcards &&
			   (*str == wild_many || *str == wild_one)))
  {
    flag=1;
    str++;
  }
  if (*str == '.')
  {
    for (str++ ;
	 isdigit(*str) ||
	   (allow_wildcards && (*str == wild_many || *str == wild_one)) ;
	 str++, flag=1) ;
  }
  if (*str != 0 || flag == 0)
    DBUG_RETURN(0);
  if (res)
    *res=atol(start);
  DBUG_RETURN(1);			/* Number ok */
} /* test_if_number */


void sql_print_error(const char *format,...)
{
  va_list args;
  time_t skr;
  struct tm tm_tmp;
  struct tm *start;
  va_start(args,format);
  DBUG_ENTER("sql_print_error");

  VOID(pthread_mutex_lock(&LOCK_error_log));
#ifndef DBUG_OFF
  {
    char buff[1024];
    vsprintf(buff,format,args);
    DBUG_PRINT("error",("%s",buff));
  }
#endif
  skr=time(NULL);
  localtime_r(&skr,&tm_tmp);
  start=&tm_tmp;
  fprintf(stderr,"%02d%02d%02d %2d:%02d:%02d  ",
	  start->tm_year % 100,
	  start->tm_mon+1,
	  start->tm_mday,
	  start->tm_hour,
	  start->tm_min,
	  start->tm_sec);
  (void) vfprintf(stderr,format,args);
  (void) fputc('\n',stderr);
  fflush(stderr);
  va_end(args);

  VOID(pthread_mutex_unlock(&LOCK_error_log));
  DBUG_VOID_RETURN;
}



void sql_perror(const char *message)
{
#ifdef HAVE_STRERROR
  sql_print_error("%s: %s",message, strerror(errno));
#else
  perror(message);
#endif
}
