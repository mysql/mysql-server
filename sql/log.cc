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
  length=end-start+1;

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



MYSQL_LOG::MYSQL_LOG(): file(0),index_file(0),last_time(0),query_start(0),
			name(0), log_type(LOG_CLOSED),write_error(0),inited(0)
{
  /*
    We don't want to intialize LOCK_Log here as the thread system may
    not have been initailized yet. We do it instead at 'open'.
  */
    index_file_name[0] = 0;
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

  if (!inited)
  {
    inited=1;
    (void) pthread_mutex_init(&LOCK_log,NULL);
    (void) pthread_mutex_init(&LOCK_index, NULL);
  }

  log_type=log_type_arg;
  name=my_strdup(log_name,MYF(0));
  if (new_name)
    strmov(log_file_name,new_name);
  else if (generate_new_name(log_file_name, name))
    return;

  if (log_type == LOG_BIN && !index_file_name[0])
    fn_format(index_file_name, name, mysql_data_home, ".index", 6);
  
  db[0]=0;
  file=my_fopen(log_file_name,O_APPEND | O_WRONLY,MYF(MY_WME | ME_WAITTANG));
  if (!file)
  {
    my_free(name,MYF(0));    
    name=0;
    log_type=LOG_CLOSED;
    return;
  }

  if (log_type == LOG_NORMAL)
  {
#ifdef __NT__
    fprintf( file, "%s, Version: %s, started with:\nTCP Port: %d, Named Pipe: %s\n", my_progname, server_version, mysql_port, mysql_unix_port);
#else
    fprintf(file,"%s, Version: %s, started with:\nTcp port: %d  Unix socket: %s\n", my_progname,server_version,mysql_port,mysql_unix_port);
#endif
    fprintf(file,"Time                 Id Command    Argument\n");
    (void) fflush(file);
  }
  else if (log_type == LOG_NEW)
  {
    time_t skr=time(NULL);
    struct tm tm_tmp;
    localtime_r(&skr,&tm_tmp);

    fprintf(file,"# %s, Version: %s at %02d%02d%02d %2d:%02d:%02d\n",
	    my_progname,server_version,
	    tm_tmp.tm_year % 100,
	    tm_tmp.tm_mon+1,
	    tm_tmp.tm_mday,
	    tm_tmp.tm_hour,
	    tm_tmp.tm_min,
	    tm_tmp.tm_sec);
    (void) fflush(file);
  }
  else if (log_type == LOG_BIN)
  {
    Start_log_event s;
    if(!index_file && 
       !(index_file = my_fopen(index_file_name,O_APPEND | O_RDWR,
			       MYF(MY_WME))))
    {
      my_fclose(file,MYF(MY_WME));
      my_free(name,MYF(0));    
      name=0;
      file=0;
      log_type=LOG_CLOSED;
      return;
    }
    s.write(file);
    pthread_mutex_lock(&LOCK_index);
    my_fseek(index_file, 0L, MY_SEEK_END, MYF(MY_WME));
    fprintf(index_file, "%s\n", log_file_name);
    fflush(index_file);
    pthread_mutex_unlock(&LOCK_index);
  }
}

int MYSQL_LOG::get_current_log(LOG_INFO* linfo)
{
  pthread_mutex_lock(&LOCK_log);
  strmake(linfo->log_file_name, log_file_name, sizeof(linfo->log_file_name));
  linfo->pos = my_ftell(file, MYF(MY_WME));
  pthread_mutex_unlock(&LOCK_log);
  return 0;
}

// if log_name is "" we stop at the first entry
int MYSQL_LOG::find_first_log(LOG_INFO* linfo, const char* log_name)
{
  // mutex needed because we need to make sure the file pointer does not move
  // from under our feet
  if(!index_file) return LOG_INFO_INVALID;
  int error = 0;
  char* fname = linfo->log_file_name;
  int log_name_len = strlen(log_name);

  pthread_mutex_lock(&LOCK_index);
  if(my_fseek(index_file, 0L, MY_SEEK_SET, MYF(MY_WME) ) == MY_FILEPOS_ERROR)
    {
      error = LOG_INFO_SEEK;
      goto err;
    }

  for(;;)
    {
      if(!fgets(fname, FN_REFLEN, index_file))
	{
	  error = feof(index_file) ? LOG_INFO_EOF : LOG_INFO_IO;
	  goto err;
	}

      // if the log entry matches, empty string matching anything
      if(!log_name_len || (fname[log_name_len] == '\n' && !memcmp(fname, log_name, log_name_len)))
	{
	  if(log_name_len)
	    fname[log_name_len] = 0; // to kill \n
	  else
	    {
	      *(strend(fname) - 1) = 0;
	    }
	  linfo->index_file_offset = my_ftell(index_file, MYF(MY_WME));
	  break;
	}
    }

  error = 0;
err:
  pthread_mutex_unlock(&LOCK_index);
  return error;
     
}
int MYSQL_LOG::find_next_log(LOG_INFO* linfo)
{
  // mutex needed because we need to make sure the file pointer does not move
  // from under our feet
  if(!index_file) return LOG_INFO_INVALID;
  int error = 0;
  char* fname = linfo->log_file_name;
  char* end ;
  
  pthread_mutex_lock(&LOCK_index);
  if(my_fseek(index_file, linfo->index_file_offset, MY_SEEK_SET, MYF(MY_WME) ) == MY_FILEPOS_ERROR)
    {
      error = LOG_INFO_SEEK;
      goto err;
    }

  if(!fgets(fname, FN_REFLEN, index_file))
    {
      error = feof(index_file) ? LOG_INFO_EOF : LOG_INFO_IO;
      goto err;
   }

  end = strend(fname) - 1;
  *end = 0; // kill /n
  linfo->index_file_offset = ftell(index_file);
  error = 0;
err:
  pthread_mutex_unlock(&LOCK_index);
  return error;
}


// we assume that buf has at least FN_REFLEN bytes alloced
void MYSQL_LOG::make_log_name(char* buf, const char* log_ident)
{
  if(inited)
    {
      int dir_len = dirname_length(log_file_name); 
      int ident_len = strlen(log_ident);
      if(dir_len + ident_len + 1 > FN_REFLEN)
	{
	  buf[0] = 0;
	  return; // protection agains malicious buffer overflow
	}
      
      memcpy(buf, log_file_name, dir_len);
      memcpy(buf + dir_len, log_ident, ident_len + 1); // this takes care of  \0
      // at the end
    }
  else
    buf[0] = 0;
}

bool MYSQL_LOG::is_active(const char* log_file_name)
{
  return inited && !strcmp(log_file_name, this->log_file_name);
}

void MYSQL_LOG::new_file()
{
  if (file)
  {
    char new_name[FN_REFLEN], *old_name=name;
    VOID(pthread_mutex_lock(&LOCK_log));
    if (generate_new_name(new_name, name))
      return;					// Something went wrong
    if (log_type == LOG_BIN)
    {
      /*
	We log the whole file name for log file as the user may decide
	to change base names at some point.
      */
      Rotate_log_event r(new_name+dirname_length(new_name));
      r.write(file);
      VOID(pthread_cond_broadcast(&COND_binlog_update));
    }
    name=0;
    close();
    open(old_name, log_type, new_name);
    my_free(old_name,MYF(0));
    if (!file)					// Something got wrong
      log_type=LOG_CLOSED;
    last_time=query_start=0;
    write_error=0;
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}


void MYSQL_LOG::write(enum enum_server_command command,
		      const char *format,...)
{
  if (name && (what_to_log & (1L << (uint) command)))
  {
    va_list args;
    va_start(args,format);
    VOID(pthread_mutex_lock(&LOCK_log));
    if (log_type != LOG_CLOSED)
    {
      time_t skr;
      ulong id;
      THD *thd=current_thd;
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
	if (fprintf(file,"%02d%02d%02d %2d:%02d:%02d\t",
		    start->tm_year % 100,
		    start->tm_mon+1,
		    start->tm_mday,
		    start->tm_hour,
		    start->tm_min,
		    start->tm_sec) < 0)
	  error=errno;
      }
      else if (fputs("\t\t",file) < 0)
	error=errno;
      if (fprintf(file,"%7ld %-10.10s",
		  id,command_name[(uint) command]) < 0)
	error=errno;
      if (format)
      {
	if (fputc(' ',file) < 0 || vfprintf(file,format,args) < 0)
	  error=errno;
      }
      if (fputc('\n',file) < 0)
	error=errno;
      if (fflush(file) < 0)
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
  if (name)
  {
    VOID(pthread_mutex_lock(&LOCK_log));
    if(file)
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
	if (e.write(file))
	{
	  sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
	  goto err;
	}
      }

      if (thd->insert_id_used)
      {
	Intvar_log_event e((uchar)INSERT_ID_EVENT, thd->last_insert_id);
	if(e.write(file))
	{
	  sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
	  goto err;
	}
      }
	  
      if(event_info->write(file))
      {
	sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
      }
  err:
      VOID(pthread_cond_broadcast(&COND_binlog_update));

      VOID(pthread_mutex_unlock(&LOCK_log));
    }
  }
  
}

void MYSQL_LOG::write(Load_log_event* event_info)
{
  if(name)
  {
    VOID(pthread_mutex_lock(&LOCK_log));
    if(file)
    {
      THD *thd=event_info->thd;
      if (!(thd->options & OPTION_BIN_LOG) &&
	  (thd->master_access & PROCESS_ACL))
      {
	VOID(pthread_mutex_unlock(&LOCK_log));
	return;
      }
	  
	  
      if (event_info->write(file))
	sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
      VOID(pthread_cond_broadcast(&COND_binlog_update));

      VOID(pthread_mutex_unlock(&LOCK_log));
    }
  }
}


/* Write update log in a format suitable for incremental backup */

void MYSQL_LOG::write(const char *query, uint query_length,
		      ulong time_for_query)
{
  if (name)
  {
    VOID(pthread_mutex_lock(&LOCK_log));
    if (file)
    {						// Safety agains reopen
      int error=0;
      THD *thd=current_thd;
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
	time_t skr=time(NULL);
	if (skr != last_time)
	{
	  last_time=skr;
	  struct tm tm_tmp;
	  struct tm *start;
	  localtime_r(&skr,&tm_tmp);
	  start=&tm_tmp;
	  if (fprintf(file,"# Time: %02d%02d%02d %2d:%02d:%02d\n",
		      start->tm_year % 100,
		      start->tm_mon+1,
		      start->tm_mday,
		      start->tm_hour,
		      start->tm_min,
		      start->tm_sec) < 0)
	    error=errno;
	}
	if (fprintf(file, "# User@Host: %s [%s] @ %s [%s]\n",
		    thd->priv_user,
		    thd->user,
		    thd->host ? thd->host : "",
		    thd->ip ? thd->ip : "") < 0)
	  error=errno;;
      }
      if (time_for_query)
	fprintf(file,"# Time: %lu\n",time_for_query);
      if (thd->db && strcmp(thd->db,db))
      {						// Database changed
	if (fprintf(file,"use %s;\n",thd->db) < 0)
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
	if (fputs("SET ",file) < 0 || fputs(buff+1,file) < 0)
	  error=errno;
      }
      if (!query)
      {
	query="#adminstrator command";
	query_length=21;
      }
      if (my_fwrite(file,(byte*) query,query_length,MYF(MY_NABP)) ||
	  fputc(';',file) < 0 || fputc('\n',file) < 0)
	error=errno;
      if (fflush(file) < 0)
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



void MYSQL_LOG::flush()
{
  if (file)
    if (fflush(file) < 0 && ! write_error)
    {
      write_error=1;
      sql_print_error(ER(ER_ERROR_ON_WRITE),name,errno);
    }
}


void MYSQL_LOG::close(bool exiting)
{					// One can't set log_type here!
  if (file)
  {
    if (log_type == LOG_BIN)
    {
      Stop_log_event s;
      s.write(file);
      VOID(pthread_cond_broadcast(&COND_binlog_update));
    }
    if (my_fclose(file,MYF(0)) < 0 && ! write_error)
    {
      write_error=1;
      sql_print_error(ER(ER_ERROR_ON_WRITE),name,errno);
    }
    file=0;
  }
  if (name)
  {
    my_free(name,MYF(0));
    name=0;
  }

  if(exiting && index_file)
    {
      if (my_fclose(index_file,MYF(0)) < 0 && ! write_error)
	{
	  write_error=1;
	  sql_print_error(ER(ER_ERROR_ON_WRITE),name,errno);
	}
      index_file=0;
     
    }
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
