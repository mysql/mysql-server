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
/* TODO: Abort logging when we get an error in reading or writing log files */

#ifdef __EMX__
#include <io.h>
#endif

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_repl.h"

#include <my_dir.h>
#include <stdarg.h>
#include <m_ctype.h>				// For test_if_number
#include <assert.h>

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


MYSQL_LOG::MYSQL_LOG()
  :bytes_written(0), last_time(0), query_start(0), name(0),
   file_id(1), open_count(1), log_type(LOG_CLOSED), write_error(0), inited(0),
   no_rotate(0), need_start_event(1)
{
  /*
    We don't want to intialize LOCK_Log here as the thread system may
    not have been initailized yet. We do it instead at 'open'.
  */
  index_file_name[0] = 0;
  bzero((char*) &log_file,sizeof(log_file));
  bzero((char*) &index_file, sizeof(index_file));
}


MYSQL_LOG::~MYSQL_LOG()
{
  cleanup();
}

void MYSQL_LOG::cleanup()
{
  if (inited)
  {
    close(1);
    inited= 0;
    (void) pthread_mutex_destroy(&LOCK_log);
    (void) pthread_mutex_destroy(&LOCK_index);
    (void) pthread_cond_destroy(&update_cond);
  }
}


int MYSQL_LOG::generate_new_name(char *new_name, const char *log_name)
{      
  fn_format(new_name,log_name,mysql_data_home,"",4);
  if (log_type != LOG_NORMAL)
  {
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


void MYSQL_LOG::init(enum_log_type log_type_arg,
		     enum cache_type io_cache_type_arg,
		     bool no_auto_events_arg)
{
  log_type = log_type_arg;
  io_cache_type = io_cache_type_arg;
  no_auto_events = no_auto_events_arg;
  if (!inited)
  {
    inited= 1;
    (void) pthread_mutex_init(&LOCK_log,MY_MUTEX_INIT_SLOW);
    (void) pthread_mutex_init(&LOCK_index, MY_MUTEX_INIT_SLOW);
    (void) pthread_cond_init(&update_cond, 0);
  }
}


/*
  Open a (new) log file.

  DESCRIPTION
  - If binary logs, also open the index file and register the new
    file name in it
  - When calling this when the file is in use, you must have a locks
    on LOCK_log and LOCK_index.

  RETURN VALUES
    0	ok
    1	error
*/

bool MYSQL_LOG::open(const char *log_name, enum_log_type log_type_arg,
		     const char *new_name, const char *index_file_name_arg,
		     enum cache_type io_cache_type_arg,
		     bool no_auto_events_arg)
{
  char buff[512];
  File file= -1, index_file_nr= -1;
  int open_flags = O_CREAT | O_APPEND | O_BINARY;
  DBUG_ENTER("MYSQL_LOG::open");
  DBUG_PRINT("enter",("log_type: %d",(int) log_type));

  last_time=query_start=0;
  write_error=0;

  if (!inited && log_type_arg == LOG_BIN && *fn_ext(log_name))
    no_rotate = 1;
  init(log_type_arg,io_cache_type_arg,no_auto_events_arg);
  
  if (!(name=my_strdup(log_name,MYF(MY_WME))))
    goto err;
  if (new_name)
    strmov(log_file_name,new_name);
  else if (generate_new_name(log_file_name, name))
    goto err;
  
  if (io_cache_type == SEQ_READ_APPEND)
    open_flags |= O_RDWR;
  else
    open_flags |= O_WRONLY;
  
  db[0]=0;
  open_count++;
  if ((file=my_open(log_file_name,open_flags,
		    MYF(MY_WME | ME_WAITTANG))) < 0 ||
      init_io_cache(&log_file, file, IO_SIZE, io_cache_type,
		    my_tell(file,MYF(MY_WME)), 0, MYF(MY_WME | MY_NABP)))
    goto err;

  switch (log_type) {
  case LOG_NORMAL:
  {
    char *end;
#ifdef __NT__
    sprintf(buff, "%s, Version: %s, started with:\nTCP Port: %d, Named Pipe: %s\n", my_progname, server_version, mysql_port, mysql_unix_port);
#else
    sprintf(buff, "%s, Version: %s, started with:\nTcp port: %d  Unix socket: %s\n", my_progname,server_version,mysql_port,mysql_unix_port);
#endif
    end=strmov(strend(buff),"Time                 Id Command    Argument\n");
    if (my_b_write(&log_file, (byte*) buff,(uint) (end-buff)) ||
	flush_io_cache(&log_file))
      goto err;
    break;
  }
  case LOG_NEW:
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
    if (my_b_write(&log_file, (byte*) buff,(uint) strlen(buff)) ||
	flush_io_cache(&log_file))
      goto err;
    break;
  }
  case LOG_BIN:
  {
    bool write_file_name_to_index_file=0;

    myf opt= MY_UNPACK_FILENAME;
    if (!index_file_name_arg)
    {
      index_file_name_arg= name;	// Use same basename for index file
      opt= MY_UNPACK_FILENAME | MY_REPLACE_EXT;
    }
  
    if (!my_b_filelength(&log_file))
    {
      /*
	The binary log file was empty (probably newly created)
	This is the normal case and happens when the user doesn't specify
	an extension for the binary log files.
	In this case we write a standard header to it.
      */
      if (my_b_write(&log_file, (byte*) BINLOG_MAGIC, BIN_LOG_HEADER_SIZE))
        goto err;
      bytes_written += BIN_LOG_HEADER_SIZE;
      write_file_name_to_index_file=1;
    }

    if (!my_b_inited(&index_file))
    {
      /*
	First open of this class instance
	Create an index file that will hold all file names uses for logging.
	Add new entries to the end of it.
      */
      fn_format(index_file_name, index_file_name_arg, mysql_data_home,
		".index", opt);
      if ((index_file_nr= my_open(index_file_name,
				  O_RDWR | O_CREAT | O_BINARY ,
				  MYF(MY_WME))) < 0 ||
	  init_io_cache(&index_file, index_file_nr,
			IO_SIZE, WRITE_CACHE,
			my_seek(index_file_nr,0L,MY_SEEK_END,MYF(0)),
			0, MYF(MY_WME)))
	goto err;
    }
    else
    {
      safe_mutex_assert_owner(&LOCK_index);
      reinit_io_cache(&index_file, WRITE_CACHE, my_b_filelength(&index_file),
		      0, 0);
    }
    if (need_start_event && !no_auto_events)
    {
      need_start_event=0;
      Start_log_event s;
      s.set_log_pos(this);
      s.write(&log_file);
    }
    if (flush_io_cache(&log_file))
      goto err;

    if (write_file_name_to_index_file)
    {
      /* As this is a new log file, we write the file name to the index file */
      if (my_b_write(&index_file, (byte*) log_file_name,
		     strlen(log_file_name)) ||
	  my_b_write(&index_file, (byte*) "\n", 1) ||
	  flush_io_cache(&index_file))
	goto err;
    }
    break;
  }
  case LOG_CLOSED:				// Impossible
    DBUG_ASSERT(1);
    break;
  }
  DBUG_RETURN(0);

err:
  sql_print_error("Could not use %s for logging (error %d)", log_name, errno);
  if (file >= 0)
    my_close(file,MYF(0));
  if (index_file_nr >= 0)
    my_close(index_file_nr,MYF(0));
  end_io_cache(&log_file);
  end_io_cache(&index_file);
  safeFree(name);
  log_type=LOG_CLOSED;
  DBUG_RETURN(1);
}


int MYSQL_LOG::get_current_log(LOG_INFO* linfo)
{
  pthread_mutex_lock(&LOCK_log);
  strmake(linfo->log_file_name, log_file_name, sizeof(linfo->log_file_name)-1);
  linfo->pos = my_b_tell(&log_file);
  pthread_mutex_unlock(&LOCK_log);
  return 0;
}


/*
  Move all data up in a file in an filename index file

  SYNOPSIS
    copy_up_file_and_fill()
    index_file			File to move
    offset			Move everything from here to beginning

  NOTE
    File will be truncated to be 'offset' shorter or filled up with
    newlines

  IMPLEMENTATION
    We do the copy outside of the IO_CACHE as the cache buffers would just
    make things slower and more complicated.
    In most cases the copy loop should only do one read.

  RETURN VALUES
    0	ok
*/

static bool copy_up_file_and_fill(IO_CACHE *index_file, my_off_t offset)
{
  int bytes_read;
  my_off_t init_offset= offset;
  File file= index_file->file;
  byte io_buf[IO_SIZE*2];
  DBUG_ENTER("copy_up_file_and_fill");

  for (;; offset+= bytes_read)
  {
    (void) my_seek(file, offset, MY_SEEK_SET, MYF(0));
    if ((bytes_read= (int) my_read(file, io_buf, sizeof(io_buf), MYF(MY_WME)))
	< 0)
      goto err;
    if (!bytes_read)
      break;					// end of file
    (void) my_seek(file, offset-init_offset, MY_SEEK_SET, MYF(0));
    if (my_write(file, (byte*) io_buf, bytes_read, MYF(MY_WME | MY_NABP)))
      goto err;
  }
  /* The following will either truncate the file or fill the end with \n' */
  if (my_chsize(file, offset - init_offset, '\n', MYF(MY_WME)))
    goto err;

  /* Reset data in old index cache */
  reinit_io_cache(index_file, READ_CACHE, (my_off_t) 0, 0, 1);
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}


/*
  Find the position in the log-index-file for the given log name

  SYNOPSIS
    find_log_pos()
    linfo		Store here the found log file name and position to
			the NEXT log file name in the index file.
    log_name		Filename to find in the index file.
			Is a null pointer if we want to read the first entry
    need_lock		Set this to 1 if the parent doesn't already have a
			lock on LOCK_index

  NOTE
    On systems without the truncate function the file will end with one or
    more empty lines.  These will be ignored when reading the file.

  RETURN VALUES
    0			ok
    LOG_INFO_EOF	End of log-index-file found
    LOG_INFO_IO		Got IO error while reading file
*/

int MYSQL_LOG::find_log_pos(LOG_INFO *linfo, const char *log_name,
			    bool need_lock)
{
  int error= 0;
  char *fname= linfo->log_file_name;
  uint log_name_len= log_name ? (uint) strlen(log_name) : 0;
  DBUG_ENTER("find_log_pos");
  DBUG_PRINT("enter",("log_name: %s", log_name ? log_name : "NULL"));

  /*
    Mutex needed because we need to make sure the file pointer does not move
    from under our feet
  */
  if (need_lock)
    pthread_mutex_lock(&LOCK_index);
  safe_mutex_assert_owner(&LOCK_index);

  /* As the file is flushed, we can't get an error here */
  (void) reinit_io_cache(&index_file, READ_CACHE, (my_off_t) 0, 0, 0);

  for (;;)
  {
    uint length;
    my_off_t offset= my_b_tell(&index_file);
    /* If we get 0 or 1 characters, this is the end of the file */

    if ((length= my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
    {
      /* Did not find the given entry; Return not found or error */
      error= !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
      break;
    }

    // if the log entry matches, null string matching anything
    if (!log_name ||
	(log_name_len == length-1 && fname[log_name_len] == '\n' &&
	 !memcmp(fname, log_name, log_name_len)))
    {
      DBUG_PRINT("info",("Found log file entry"));
      fname[length-1]=0;			// remove last \n
      linfo->index_file_start_offset= offset;
      linfo->index_file_offset = my_b_tell(&index_file);
      break;
    }
  }

  if (need_lock)
    pthread_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}


/*
  Find the position in the log-index-file for the given log name

  SYNOPSIS
    find_next_log()
    linfo		Store here the next log file name and position to
			the file name after that.
    need_lock		Set this to 1 if the parent doesn't already have a
			lock on LOCK_index

  NOTE
    - Before calling this function, one has to call find_log_pos()
      to set up 'linfo'
    - Mutex needed because we need to make sure the file pointer does not move
      from under our feet

  RETURN VALUES
    0			ok
    LOG_INFO_EOF	End of log-index-file found
    LOG_INFO_SEEK	Could not allocate IO cache
    LOG_INFO_IO		Got IO error while reading file
*/

int MYSQL_LOG::find_next_log(LOG_INFO* linfo, bool need_lock)
{
  int error= 0;
  uint length;
  char *fname= linfo->log_file_name;

  if (need_lock)
    pthread_mutex_lock(&LOCK_index);
  safe_mutex_assert_owner(&LOCK_index);

  /* As the file is flushed, we can't get an error here */
  (void) reinit_io_cache(&index_file, READ_CACHE, linfo->index_file_offset, 0,
			 0);

  linfo->index_file_start_offset= linfo->index_file_offset;
  if ((length=my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
  {
    error = !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
    goto err;
  }
  fname[length-1]=0;				// kill /n
  linfo->index_file_offset = my_b_tell(&index_file);

err:
  if (need_lock)
    pthread_mutex_unlock(&LOCK_index);
  return error;
}


/*
  Delete all logs refered to in the index file
  Start writing to a new log file.  The new index file will only contain
  this file.

  SYNOPSIS
     reset_logs()
     thd		Thread

  NOTE
    If not called from slave thread, write start event to new log


  RETURN VALUES
    0	ok
    1   error
*/

bool MYSQL_LOG::reset_logs(THD* thd)
{
  LOG_INFO linfo;
  bool error=0;
  const char* save_name;
  enum_log_type save_log_type;
  DBUG_ENTER("reset_logs");

  /*
    We need to get both locks to be sure that no one is trying to
    write to the index log file.
  */
  pthread_mutex_lock(&LOCK_log);
  pthread_mutex_lock(&LOCK_index);

  /* Save variables so that we can reopen the log */
  save_name=name;
  name=0;					// Protect against free
  save_log_type=log_type;
  close(0);					// Don't close the index file

  /* First delete all old log files */

  if (find_log_pos(&linfo, NullS, 0))
  {
    error=1;
    goto err;
  }
  
  for (;;)
  {
    my_delete(linfo.log_file_name, MYF(MY_WME));
    if (find_next_log(&linfo, 0))
      break;
  }

  /* Start logging with a new file */
  close(1);					// Close index file
  my_delete(index_file_name, MYF(MY_WME));	// Reset (open will update)
  if (!thd->slave_thread)
    need_start_event=1;
  open(save_name, save_log_type, 0, index_file_name,
       io_cache_type, no_auto_events);
  my_free((gptr) save_name, MYF(0));

err:  
  pthread_mutex_unlock(&LOCK_index);
  pthread_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}


/*
  Delete the current log file, remove it from index file and start on next 

  SYNOPSIS
    purge_first_log()
    rli		Relay log information

  NOTE
    - This is only called from the slave-execute thread when it has read
      all commands from a log and want to switch to a new log.
    - When this happens, we should never be in an active transaction as
      a transaction is always written as a single block to the binary log.

  IMPLEMENTATION
    - Protects index file with LOCK_index
    - Delete first log file,
    - Copy all file names after this one to the front of the index file
    - If the OS has truncate, truncate the file, else fill it with \n'
    - Read the first file name from the index file and store in rli->linfo

  RETURN VALUES
    0			ok
    LOG_INFO_EOF	End of log-index-file found
    LOG_INFO_SEEK	Could not allocate IO cache
    LOG_INFO_IO		Got IO error while reading file
*/

int MYSQL_LOG::purge_first_log(struct st_relay_log_info* rli)
{
  int error;
  DBUG_ENTER("purge_first_log");

  /*
    Test pre-conditions.

    Assume that we have previously read the first log and
    stored it in rli->relay_log_name
  */
  DBUG_ASSERT(is_open());
  DBUG_ASSERT(rli->slave_running == 1);
  DBUG_ASSERT(!strcmp(rli->linfo.log_file_name,rli->relay_log_name));
  DBUG_ASSERT(rli->linfo.index_file_offset ==
	      strlen(rli->relay_log_name) + 1);

  /* We have already processed the relay log, so it's safe to delete it */
  my_delete(rli->relay_log_name, MYF(0));
  pthread_mutex_lock(&LOCK_index);
  if (copy_up_file_and_fill(&index_file, rli->linfo.index_file_offset))
  {
    error= LOG_INFO_IO;
    goto err;
  }

  /*
    Update the space counter used by all relay logs
    Ok to broadcast after the critical region as there is no risk of
    the mutex being destroyed by this thread later - this helps save
    context switches
  */
  pthread_mutex_lock(&rli->log_space_lock);
  rli->log_space_total -= rli->relay_log_pos;
  pthread_mutex_unlock(&rli->log_space_lock);
  pthread_cond_broadcast(&rli->log_space_cond);
  
  /*
    Read the next log file name from the index file and pass it back to
    the caller
  */
  if ((error=find_log_pos(&rli->linfo, NullS, 0 /*no mutex*/)))
  {
    char buff[22];
    sql_print_error("next log error: %d  offset: %s  log: %s",
		    error,
		    llstr(rli->linfo.index_file_offset,buff),
		    rli->linfo.log_file_name);
    goto err;
  }
  /*
    Reset position to current log.  This involves setting both of the
    position variables:
  */
  rli->relay_log_pos = BIN_LOG_HEADER_SIZE;
  rli->pending = 0;
  strmake(rli->relay_log_name,rli->linfo.log_file_name,
	  sizeof(rli->relay_log_name)-1);

  /* Store where we are in the new file for the execution thread */
  flush_relay_log_info(rli);

err:
  pthread_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}


/*
  Remove all logs before the given log from disk and from the index file.

  SYNOPSIS
    purge_logs()
    thd		Thread pointer
    to_log	Delete all log file name before this file. This file is not
		deleted

  NOTES
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  RETURN VALUES
    0				ok
    LOG_INFO_PURGE_NO_ROTATE	Binary file that can't be rotated
    LOG_INFO_EOF		to_log not found
*/

int MYSQL_LOG::purge_logs(THD* thd, const char* to_log)
{
  int error;
  LOG_INFO log_info;
  DBUG_ENTER("purge_logs");

  if (no_rotate)
    DBUG_RETURN(LOG_INFO_PURGE_NO_ROTATE);

  pthread_mutex_lock(&LOCK_index);
  if ((error=find_log_pos(&log_info, to_log, 0 /*no mutex*/)))
    goto err;

  /*
    File name exists in index file; Delete until we find this file
    or a file that is used.
  */
  if ((error=find_log_pos(&log_info, NullS, 0 /*no mutex*/)))
    goto err;
  while (strcmp(to_log,log_info.log_file_name) &&
	 !log_in_use(log_info.log_file_name))
  {
    /* It's not fatal even if we can't delete a log file */
    my_delete(log_info.log_file_name, MYF(0));
    if (find_next_log(&log_info, 0))
      break;
  }

  /*
    If we get killed -9 here, the sysadmin would have to edit
    the log index file after restart - otherwise, this should be safe
  */

  if (copy_up_file_and_fill(&index_file, log_info.index_file_start_offset))
  {
    error= LOG_INFO_IO;
    goto err;
  }

  // now update offsets in index file for running threads
  adjust_linfo_offsets(log_info.index_file_start_offset);

err:
  pthread_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}


/*
  Create a new log file name

  SYNOPSIS
    make_log_name()
    buf			buf of at least FN_REFLEN where new name is stored

  NOTE
    If file name will be longer then FN_REFLEN it will be truncated
*/

void MYSQL_LOG::make_log_name(char* buf, const char* log_ident)
{
  if (inited)					// QQ When is this not true ?
  {
    uint dir_len = dirname_length(log_file_name); 
    if (dir_len > FN_REFLEN)
      dir_len=FN_REFLEN-1;
    strnmov(buf, log_file_name, dir_len);
    strmake(buf+dir_len, log_ident, FN_REFLEN - dir_len);
  }
}


/*
  Check if we are writing/reading to the given log file
*/

bool MYSQL_LOG::is_active(const char *log_file_name_arg)
{
  return inited && !strcmp(log_file_name, log_file_name_arg);
}


/*
  Start writing to a new log file or reopen the old file

  SYNOPSIS
    new_file()
    need_lock		Set to 1 (default) if caller has not locked
			LOCK_log and LOCK_index

  NOTE
    The new file name is stored last in the index file
*/

void MYSQL_LOG::new_file(bool need_lock)
{
  char new_name[FN_REFLEN], *new_name_ptr, *old_name;
  enum_log_type save_log_type;

  if (!is_open())
    return;					// Should never happen

  if (need_lock)
  {
    pthread_mutex_lock(&LOCK_log);
    pthread_mutex_lock(&LOCK_index);
  }    
  safe_mutex_assert_owner(&LOCK_log);
  safe_mutex_assert_owner(&LOCK_index);

  // Reuse old name if not binlog and not update log
  new_name_ptr= name;

  /*
    Only rotate open logs that are marked non-rotatable
    (binlog with constant name are non-rotatable)
  */
  if (!no_rotate)
  {
    /*
      If user hasn't specified an extension, generate a new log name
      We have to do this here and not in open as we want to store the
      new file name in the current binary log file.
    */
    if (generate_new_name(new_name, name))
      goto end;
    new_name_ptr=new_name;

    if (log_type == LOG_BIN)
    {
      if (!no_auto_events)
      {
	/*
	  We log the whole file name for log file as the user may decide
	  to change base names at some point.
	*/
	THD* thd = current_thd;
	Rotate_log_event r(thd,new_name+dirname_length(new_name));
	r.set_log_pos(this);

	/*
	  Because this log rotation could have been initiated by a master of
	  the slave running with log-bin, we set the flag on rotate
	  event to prevent infinite log rotation loop
	*/
	if (thd->slave_thread)
	  r.flags|= LOG_EVENT_FORCED_ROTATE_F;
	r.write(&log_file);
	bytes_written += r.get_event_len();
      }
      /*
	Update needs to be signalled even if there is no rotate event
	log rotation should give the waiting thread a signal to
	discover EOF and move on to the next log.
      */
      signal_update(); 
    }
  }
  old_name=name;
  save_log_type=log_type;
  name=0;				// Don't free name
  close();
  open(old_name, save_log_type, new_name_ptr, index_file_name, io_cache_type,
       no_auto_events);
  my_free(old_name,MYF(0));

end:
  if (need_lock)
  {
    pthread_mutex_unlock(&LOCK_index);
    pthread_mutex_unlock(&LOCK_log);
  }
}


bool MYSQL_LOG::append(Log_event* ev)
{
  bool error = 0;
  pthread_mutex_lock(&LOCK_log);
  
  DBUG_ASSERT(log_file.type == SEQ_READ_APPEND);
  /*
    Log_event::write() is smart enough to use my_b_write() or
    my_b_append() depending on the kind of cache we have.
  */
  if (ev->write(&log_file))
  {
    error=1;
    goto err;
  }
  bytes_written += ev->get_event_len();
  if ((uint) my_b_append_tell(&log_file) > max_binlog_size)
  {
    pthread_mutex_lock(&LOCK_index);
    new_file(0);
    pthread_mutex_unlock(&LOCK_index);
  }

err:  
  pthread_mutex_unlock(&LOCK_log);
  signal_update();				// Safe as we don't call close
  return error;
}


bool MYSQL_LOG::appendv(const char* buf, uint len,...)
{
  bool error= 0;
  va_list(args);
  va_start(args,len);
  
  DBUG_ASSERT(log_file.type == SEQ_READ_APPEND);
  
  pthread_mutex_lock(&LOCK_log);
  do
  {
    if (my_b_append(&log_file,(byte*) buf,len))
    {
      error= 1;
      goto err;
    }
    bytes_written += len;
  } while ((buf=va_arg(args,const char*)) && (len=va_arg(args,uint)));
  
  if ((uint) my_b_append_tell(&log_file) > max_binlog_size)
  {
    pthread_mutex_lock(&LOCK_index);
    new_file(0);
    pthread_mutex_unlock(&LOCK_index);
  }

err:
  pthread_mutex_unlock(&LOCK_log);
  if (!error)
    signal_update();
  return error;
}


/*
  Write to normal (not rotable) log
  This is the format for the 'normal', 'slow' and 'update' logs.
*/

bool MYSQL_LOG::write(THD *thd,enum enum_server_command command,
		      const char *format,...)
{
  if (is_open() && (what_to_log & (1L << (uint) command)))
  {
    int error=0;
    VOID(pthread_mutex_lock(&LOCK_log));

    /* Test if someone closed between the is_open test and lock */
    if (is_open())
    {
      time_t skr;
      ulong id;
      va_list args;
      va_start(args,format);
      char buff[32];

      if (thd)
      {						// Normal thread
	if ((thd->options & OPTION_LOG_OFF) &&
	    (thd->master_access & SUPER_ACL))
	{
	  VOID(pthread_mutex_unlock(&LOCK_log));
	  return 0;				// No logging
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
	if (my_b_write(&log_file, (byte*) buff,16))
	  error=errno;
      }
      else if (my_b_write(&log_file, (byte*) "\t\t",2) < 0)
	error=errno;
      sprintf(buff,"%7ld %-11.11s", id,command_name[(uint) command]);
      if (my_b_write(&log_file, (byte*) buff,strlen(buff)))
	error=errno;
      if (format)
      {
	if (my_b_write(&log_file, (byte*) " ",1) ||
	    my_b_vprintf(&log_file,format,args) == (uint) -1)
	  error=errno;
      }
      if (my_b_write(&log_file, (byte*) "\n",1) ||
	  flush_io_cache(&log_file))
	error=errno;
      if (error && ! write_error)
      {
	write_error=1;
	sql_print_error(ER(ER_ERROR_ON_WRITE),name,error);
      }
      va_end(args);
    }
    VOID(pthread_mutex_unlock(&LOCK_log));
    return error != 0;
  }
  return 0;
}


/*
  Write an event to the binary log
*/

bool MYSQL_LOG::write(Log_event* event_info)
{
  bool error=0;
  DBUG_ENTER("MYSQL_LOG::write(event)");
  
  if (!inited)					// Can't use mutex if not init
  {
    DBUG_PRINT("error",("not initied"));
    DBUG_RETURN(0);
  }
  pthread_mutex_lock(&LOCK_log);

  /* In most cases this is only called if 'is_open()' is true */
  if (is_open())
  {
    bool should_rotate = 0;
    THD *thd=event_info->thd;
    const char *local_db = event_info->get_db();
#ifdef USING_TRANSACTIONS    
    IO_CACHE *file = ((event_info->get_cache_stmt()) ?
		      &thd->transaction.trans_log :
		      &log_file);
#else
    IO_CACHE *file = &log_file;
#endif    
    if ((thd && !(thd->options & OPTION_BIN_LOG) &&
	 (thd->master_access & SUPER_ACL)) ||
	(local_db && !db_ok(local_db, binlog_do_db, binlog_ignore_db)))
    {
      VOID(pthread_mutex_unlock(&LOCK_log));
      DBUG_PRINT("error",("!db_ok"));
      DBUG_RETURN(0);
    }

    error=1;
    /*
      No check for auto events flag here - this write method should
      never be called if auto-events are enabled
    */
    if (thd)
    {
      if (thd->last_insert_id_used)
      {
	Intvar_log_event e(thd,(uchar) LAST_INSERT_ID_EVENT,
			   thd->current_insert_id);
	e.set_log_pos(this);
	if (thd->server_id)
	  e.server_id = thd->server_id;
	if (e.write(file))
	  goto err;
      }
      if (thd->insert_id_used)
      {
	Intvar_log_event e(thd,(uchar) INSERT_ID_EVENT,thd->last_insert_id);
	e.set_log_pos(this);
	if (thd->server_id)
	  e.server_id = thd->server_id;
	if (e.write(file))
	  goto err;
      }
      if (thd->rand_used)
      {
	Rand_log_event e(thd,thd->rand_saved_seed1,thd->rand_saved_seed2);
	e.set_log_pos(this);
	if (e.write(file))
	  goto err;
      }
      if (thd->variables.convert_set)
      {
	char buf[256], *p;
	p= strmov(strmov(buf, "SET CHARACTER SET "),
		  thd->variables.convert_set->name);
	Query_log_event e(thd, buf, (ulong) (p - buf), 0);
	e.set_log_pos(this);
	if (e.write(file))
	  goto err;
      }
    }
    event_info->set_log_pos(this);
    if (event_info->write(file) ||
	file == &log_file && flush_io_cache(file))
      goto err;
    error=0;

    /*
      Tell for transactional table handlers up to which position in the
      binlog file we wrote. The table handler can store this info, and
      after crash recovery print for the user the offset of the last
      transactions which were recovered. Actually, we must also call
      the table handler commit here, protected by the LOCK_log mutex,
      because otherwise the transactions may end up in a different order
      in the table handler log!
    */

    if (file == &log_file)
    {
      /*
	LOAD DATA INFILE in AUTOCOMMIT=1 mode writes to the binlog
	chunks also before it is successfully completed. We only report
	the binlog write and do the commit inside the transactional table
	handler if the log event type is appropriate.
      */

      if (event_info->get_type_code() == QUERY_EVENT
          || event_info->get_type_code() == EXEC_LOAD_EVENT)
      {
	error = ha_report_binlog_offset_and_commit(thd, log_file_name,
                                                 file->pos_in_file);
      }

      should_rotate= (my_b_tell(file) >= (my_off_t) max_binlog_size); 
    }

err:
    if (error)
    {
      if (my_errno == EFBIG)
	my_error(ER_TRANS_CACHE_FULL, MYF(0));
      else
	my_error(ER_ERROR_ON_WRITE, MYF(0), name, errno);
      write_error=1;
    }
    if (file == &log_file)
      signal_update();
    if (should_rotate)
    {
      pthread_mutex_lock(&LOCK_index);      
      new_file(0); // inside mutex
      pthread_mutex_unlock(&LOCK_index);
    }
  }

  pthread_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}


uint MYSQL_LOG::next_file_id()
{
  uint res;
  pthread_mutex_lock(&LOCK_log);
  res = file_id++;
  pthread_mutex_unlock(&LOCK_log);
  return res;
}


/*
  Write a cached log entry to the binary log

  NOTE
    - We only come here if there is something in the cache.
    - The thing in the cache is always a complete transaction
    - 'cache' needs to be reinitialized after this functions returns.

  IMPLEMENTATION
    - To support transaction over replication, we wrap the transaction
      with BEGIN/COMMIT in the binary log.
*/

bool MYSQL_LOG::write(THD *thd, IO_CACHE *cache)
{
  VOID(pthread_mutex_lock(&LOCK_log));
  DBUG_ENTER("MYSQL_LOG::write(cache");
  
  if (is_open())				// Should always be true
  {
    uint length;

    /*
      Add the "BEGIN" and "COMMIT" in the binlog around transactions
      which may contain more than 1 SQL statement. If we run with
      AUTOCOMMIT=1, then MySQL immediately writes each SQL statement to
      the binlog when the statement has been completed. No need to add
      "BEGIN" ... "COMMIT" around such statements. Otherwise, MySQL uses
      thd->transaction.trans_log to cache the SQL statements until the
      explicit commit, and at the commit writes the contents in .trans_log
      to the binlog.

      We write the "BEGIN" mark first in the buffer (.trans_log) where we
      store the SQL statements for a transaction. At the transaction commit
      we will add the "COMMIT mark and write the buffer to the binlog.
    */
    {
      Query_log_event qinfo(thd, "BEGIN", 5, TRUE);
      /*
        Now this Query_log_event has artificial log_pos 0. It must be adjusted
        to reflect the real position in the log. Not doing it would confuse the
        slave: it would prevent this one from knowing where he is in the master's
        binlog, which would result in wrong positions being shown to the user,
        MASTER_POS_WAIT undue waiting etc.
      */
      qinfo.set_log_pos(this);
      if (qinfo.write(&log_file))
	goto err;
    }
    /* Read from the file used to cache the queries .*/
    if (reinit_io_cache(cache, READ_CACHE, 0, 0, 0))
      goto err;
    length=my_b_bytes_in_cache(cache);
    do
    {
      /* Write data to the binary log file */
      if (my_b_write(&log_file, cache->read_pos, length))
	goto err;
      cache->read_pos=cache->read_end;		// Mark buffer used up
    } while ((length=my_b_fill(cache)));

    /*
      We write the command "COMMIT" as the last SQL command in the
      binlog segment cached for this transaction
    */

    {
      Query_log_event qinfo(thd, "COMMIT", 6, TRUE);
      qinfo.set_log_pos(this);
      if (qinfo.write(&log_file) || flush_io_cache(&log_file))
	goto err;
    }
    if (cache->error)				// Error on read
    {
      sql_print_error(ER(ER_ERROR_ON_READ), cache->file_name, errno);
      write_error=1;				// Don't give more errors
      goto err;
    }
    if ((ha_report_binlog_offset_and_commit(thd, log_file_name,
					    log_file.pos_in_file)))
      goto err;
    signal_update();
    if (my_b_tell(&log_file) >= (my_off_t) max_binlog_size)
    {
      pthread_mutex_lock(&LOCK_index);
      new_file(0); // inside mutex
      pthread_mutex_unlock(&LOCK_index);
    }

  }
  VOID(pthread_mutex_unlock(&LOCK_log));
  DBUG_RETURN(0);

err:
  if (!write_error)
  {
    write_error= 1;
    sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
  }
  VOID(pthread_mutex_unlock(&LOCK_log));
  DBUG_RETURN(1);
}


/*
  Write update log in a format suitable for incremental backup

  NOTE
   - This code should be deleted in MySQL 5,0 as the binary log
     is a full replacement for the update log.

*/

bool MYSQL_LOG::write(THD *thd,const char *query, uint query_length,
		      time_t query_start_arg)
{
  bool error=0;
  if (is_open())
  {
    time_t current_time;
    VOID(pthread_mutex_lock(&LOCK_log));
    if (is_open())
    {						// Safety agains reopen
      int tmp_errno=0;
      char buff[80],*end;
      end=buff;
      if (!(thd->options & OPTION_UPDATE_LOG) &&
	  (thd->master_access & SUPER_ACL))
      {
	VOID(pthread_mutex_unlock(&LOCK_log));
	return 0;
      }
      if ((specialflag & SPECIAL_LONG_LOG_FORMAT) || query_start_arg)
      {
	current_time=time(NULL);
	if (current_time != last_time)
	{
	  last_time=current_time;
	  struct tm tm_tmp;
	  struct tm *start;
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
	  if (my_b_write(&log_file, (byte*) buff,24))
	    tmp_errno=errno;
	}
	if (my_b_printf(&log_file, "# User@Host: %s[%s] @ %s [%s]\n",
			thd->priv_user,
			thd->user,
			thd->host ? thd->host : "",
			thd->ip ? thd->ip : "") == (uint) -1)
	  tmp_errno=errno;
      }
      if (query_start_arg)
      {
	/* For slow query log */
	if (my_b_printf(&log_file,
			"# Query_time: %lu  Lock_time: %lu  Rows_sent: %lu  Rows_examined: %lu\n",
			(ulong) (current_time - query_start_arg),
			(ulong) (thd->time_after_lock - query_start_arg),
			(ulong) thd->sent_row_count,
			(ulong) thd->examined_row_count) == (uint) -1)
	  tmp_errno=errno;
      }
      if (thd->db && strcmp(thd->db,db))
      {						// Database changed
	if (my_b_printf(&log_file,"use %s;\n",thd->db) == (uint) -1)
	  tmp_errno=errno;
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
	if (query_start_arg != thd->query_start())
	{
	  query_start_arg=thd->query_start();
	  end=strmov(end,",timestamp=");
	  end=int10_to_str((long) query_start_arg,end,10);
	}
      }
      if (end != buff)
      {
	*end++=';';
	*end='\n';
	if (my_b_write(&log_file, (byte*) "SET ",4) ||
	    my_b_write(&log_file, (byte*) buff+1,(uint) (end-buff)))
	  tmp_errno=errno;
      }
      if (!query)
      {
	end=strxmov(buff, "# administrator command: ",
		    command_name[thd->command], NullS);
	query_length=(ulong) (end-buff);
	query=buff;
      }
      if (my_b_write(&log_file, (byte*) query,query_length) ||
	  my_b_write(&log_file, (byte*) ";\n",2) ||
	  flush_io_cache(&log_file))
	tmp_errno=errno;
      if (tmp_errno)
      {
	error=1;
	if (! write_error)
	{
	  write_error=1;
	  sql_print_error(ER(ER_ERROR_ON_WRITE),name,error);
	}
      }
    }
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
  return error;
}

/*
  Wait until we get a signal that the binary log has been updated

  SYNOPSIS
    wait_for_update()
    thd			Thread variable

  NOTES
    One must have a lock on LOCK_log before calling this function.
    This lock will be freed before return!

    The reason for the above is that for enter_cond() / exit_cond() to
    work the mutex must be got before enter_cond() but releases before
    exit_cond().
    If you don't do it this way, you will get a deadlock in THD::awake()
*/


void MYSQL_LOG:: wait_for_update(THD* thd)
{
  safe_mutex_assert_owner(&LOCK_log);
  const char* old_msg = thd->enter_cond(&update_cond, &LOCK_log,
					"Slave: waiting for binlog update");
  pthread_cond_wait(&update_cond, &LOCK_log);
  pthread_mutex_unlock(&LOCK_log);		// See NOTES
  thd->exit_cond(old_msg);
}


/*
  Close the log file

  SYNOPSIS
    close()
    exiting	Set to 1 if we should also close the index file
    		This can be set to 0 if we are going to do call open
		at once after close, in which case we don't want to
		close the index file.
		We only write a 'stop' event to the log if exiting is set

  NOTES
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void MYSQL_LOG::close(bool exiting)
{					// One can't set log_type here!
  DBUG_ENTER("MYSQL_LOG::close");
  DBUG_PRINT("enter",("exiting: %d", (int) exiting));
  if (is_open())
  {
    if (log_type == LOG_BIN && !no_auto_events && exiting)
    {
      Stop_log_event s;
      s.set_log_pos(this);
      s.write(&log_file);
      signal_update();
    }
    end_io_cache(&log_file);
    if (my_close(log_file.file,MYF(0)) < 0 && ! write_error)
    {
      write_error=1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
    }
  }

  /*
    The following test is needed even if is_open() is not set, as we may have
    called a not complete close earlier and the index file is still open.
  */

  if (exiting && my_b_inited(&index_file))
  {
    end_io_cache(&index_file);
    if (my_close(index_file.file, MYF(0)) < 0 && ! write_error)
    {
      write_error= 1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), index_file_name, errno);
    }
  }
  log_type= LOG_CLOSED;
  safeFree(name);
  DBUG_VOID_RETURN;
}


/*
  Check if a string is a valid number

  SYNOPSIS
    test_if_number()
    str			String to test
    res			Store value here
    allow_wildcards	Set to 1 if we should ignore '%' and '_'

  NOTE
    For the moment the allow_wildcards argument is not used
    Should be move to some other file.

  RETURN VALUES
    1	String is a number
    0	Error
*/

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
    my_vsnprintf(buff,sizeof(buff)-1,format,args);
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

bool flush_error_log()
{
  bool result=0;
  if (opt_error_log)
  {
    char err_renamed[FN_REFLEN], *end;
    end= strmake(err_renamed,log_error_file,FN_REFLEN-4);
    strmov(end, "-old");
#ifdef __WIN__
    char err_temp[FN_REFLEN+4];
    /*
     On Windows is necessary a temporary file for to rename
     the current error file.
    */
    strmov(strmov(err_temp, err_renamed),"-tmp");
    (void) my_delete(err_temp, MYF(0)); 
    if (freopen(err_temp,"a+",stdout))
    {
      freopen(err_temp,"a+",stderr);
      (void) my_delete(err_renamed, MYF(0));
      my_rename(log_error_file,err_renamed,MYF(0));
      if (freopen(log_error_file,"a+",stdout))
        freopen(log_error_file,"a+",stderr);
      int fd, bytes;
      char buf[IO_SIZE];
      if ((fd = my_open(err_temp, O_RDONLY, MYF(0))) >= 0)
      {
        while ((bytes = (int) my_read(fd, (byte*) buf, IO_SIZE, MYF(0))) > 0)
             my_fwrite(stderr, (byte*) buf, (uint) strlen(buf),MYF(0));
        my_close(fd, MYF(0));
      }
      (void) my_delete(err_temp, MYF(0)); 
    }
    else
     result= 1;
#else
   my_rename(log_error_file,err_renamed,MYF(0));
   if (freopen(log_error_file,"a+",stdout))
     freopen(log_error_file,"a+",stderr);
   else
     result= 1;
#endif
  }
   return result;
}
