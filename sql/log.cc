/* Copyright (C) 2000-2003 MySQL AB

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
#include "ha_innodb.h" // necessary to cut the binlog when crash recovery

#include <my_dir.h>
#include <stdarg.h>
#include <m_ctype.h>				// For test_if_number

#ifdef __NT__
#include "message.h"
#endif

MYSQL_LOG mysql_log, mysql_slow_log, mysql_bin_log;
ulong sync_binlog_counter= 0;

static bool test_if_number(const char *str,
			   long *res, bool allow_wildcards);

#ifdef __NT__
static int eventSource = 0;

void setup_windows_event_source() 
{
  HKEY    hRegKey= NULL; 
  DWORD   dwError= 0;
  TCHAR   szPath[MAX_PATH];
  DWORD dwTypes;
    
  if (eventSource)               // Ensure that we are only called once
    return;
  eventSource= 1;

  // Create the event source registry key
  dwError= RegCreateKey(HKEY_LOCAL_MACHINE, 
                          "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\MySQL", 
                          &hRegKey);

  /* Name of the PE module that contains the message resource */
  GetModuleFileName(NULL, szPath, MAX_PATH);

  /* Register EventMessageFile */
  dwError = RegSetValueEx(hRegKey, "EventMessageFile", 0, REG_EXPAND_SZ, 
                          (PBYTE) szPath, strlen(szPath)+1);
    

  /* Register supported event types */
  dwTypes= (EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
            EVENTLOG_INFORMATION_TYPE);
  dwError= RegSetValueEx(hRegKey, "TypesSupported", 0, REG_DWORD,
                         (LPBYTE) &dwTypes, sizeof dwTypes);

  RegCloseKey(hRegKey);
}

#endif /* __NT__ */


/****************************************************************************
** Find a uniq filename for 'filename.#'.
** Set # to a number as low as possible
** returns != 0 if not possible to get uniq filename
****************************************************************************/

static int find_uniq_filename(char *name)
{
  long                  number;
  uint                  i;
  char                  buff[FN_REFLEN];
  struct st_my_dir     *dir_info;
  reg1 struct fileinfo *file_info;
  ulong                 max_found=0;

  DBUG_ENTER("find_uniq_filename");

  uint  length = dirname_part(buff,name);
  char *start  = name + length;
  char *end    = strend(start);

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
  sprintf(end,"%06ld",max_found+1);
  DBUG_RETURN(0);
}


MYSQL_LOG::MYSQL_LOG()
  :bytes_written(0), last_time(0), query_start(0), name(0),
   file_id(1), open_count(1), log_type(LOG_CLOSED), write_error(0), inited(0),
   need_start_event(1), description_event_for_exec(0),
   description_event_for_queue(0)
{
  /*
    We don't want to initialize LOCK_Log here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main(). 
  */
  index_file_name[0] = 0;
  bzero((char*) &log_file,sizeof(log_file));
  bzero((char*) &index_file, sizeof(index_file));
}


MYSQL_LOG::~MYSQL_LOG()
{
  cleanup();
}

/* this is called only once */

void MYSQL_LOG::cleanup()
{
  DBUG_ENTER("cleanup");
  if (inited)
  {
    inited= 0;
    close(LOG_CLOSE_INDEX);
    delete description_event_for_queue;
    delete description_event_for_exec;
    (void) pthread_mutex_destroy(&LOCK_log);
    (void) pthread_mutex_destroy(&LOCK_index);
    (void) pthread_cond_destroy(&update_cond);
  }
  DBUG_VOID_RETURN;
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
		     bool no_auto_events_arg,
                     ulong max_size_arg)
{
  DBUG_ENTER("MYSQL_LOG::init");
  log_type = log_type_arg;
  io_cache_type = io_cache_type_arg;
  no_auto_events = no_auto_events_arg;
  max_size=max_size_arg;
  DBUG_PRINT("info",("log_type: %d max_size: %lu", log_type, max_size));
  DBUG_VOID_RETURN;
}


void MYSQL_LOG::init_pthread_objects()
{
  DBUG_ASSERT(inited == 0);
  inited= 1;
  (void) pthread_mutex_init(&LOCK_log,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_index, MY_MUTEX_INIT_SLOW);
  (void) pthread_cond_init(&update_cond, 0);
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
		     bool no_auto_events_arg,
                     ulong max_size_arg,
                     bool null_created_arg)
{
  char buff[512];
  File file= -1, index_file_nr= -1;
  int open_flags = O_CREAT | O_APPEND | O_BINARY;
  DBUG_ENTER("MYSQL_LOG::open");
  DBUG_PRINT("enter",("log_type: %d",(int) log_type));

  last_time=query_start=0;
  write_error=0;

  init(log_type_arg,io_cache_type_arg,no_auto_events_arg,max_size_arg);
  
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
    int len=my_snprintf(buff, sizeof(buff), "%s, Version: %s. "
#ifdef EMBEDDED_LIBRARY
		        "embedded library\n", my_progname, server_version
#elif __NT__
			"started with:\nTCP Port: %d, Named Pipe: %s\n",
			my_progname, server_version, mysqld_port, mysqld_unix_port
#else
			"started with:\nTcp port: %d  Unix socket: %s\n",
			my_progname,server_version,mysqld_port,mysqld_unix_port
#endif
                       );
    end=strnmov(buff+len,"Time                 Id Command    Argument\n",
                sizeof(buff)-len);
    if (my_b_write(&log_file, (byte*) buff,(uint) (end-buff)) ||
	flush_io_cache(&log_file))
      goto err;
    break;
  }
  case LOG_NEW:
  {
    uint len;
    time_t skr=time(NULL);
    struct tm tm_tmp;

    localtime_r(&skr,&tm_tmp);
    len= my_snprintf(buff,sizeof(buff),
		     "# %s, Version: %s at %02d%02d%02d %2d:%02d:%02d\n",
		     my_progname,server_version,
		     tm_tmp.tm_year % 100,
		     tm_tmp.tm_mon+1,
		     tm_tmp.tm_mday,
		     tm_tmp.tm_hour,
		     tm_tmp.tm_min,
		     tm_tmp.tm_sec);
    if (my_b_write(&log_file, (byte*) buff, len) ||
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
      if (my_b_safe_write(&log_file, (byte*) BINLOG_MAGIC,
			  BIN_LOG_HEADER_SIZE))
        goto err;
      bytes_written+= BIN_LOG_HEADER_SIZE;
      write_file_name_to_index_file= 1;
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
          my_sync(index_file_nr, MYF(MY_WME)) ||
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
      /*
        In 4.x we set need_start_event=0 here, but in 5.0 we want a Start event
        even if this is not the very first binlog.
      */
      Format_description_log_event s(BINLOG_VERSION);
      if (!s.is_valid())
        goto err;
      if (null_created_arg)
        s.created= 0;
      if (s.write(&log_file))
        goto err;
      bytes_written+= s.data_written;
    }
    if (description_event_for_queue &&
        description_event_for_queue->binlog_version>=4)
    {
      /*
        This is a relay log written to by the I/O slave thread.
        Write the event so that others can later know the format of this relay
        log.
        Note that this event is very close to the original event from the
        master (it has binlog version of the master, event types of the
        master), so this is suitable to parse the next relay log's event. It
        has been produced by
        Format_description_log_event::Format_description_log_event(char*
        buf,).
        Why don't we want to write the description_event_for_queue if this
        event is for format<4 (3.23 or 4.x): this is because in that case, the
        description_event_for_queue describes the data received from the
        master, but not the data written to the relay log (*conversion*),
        which is in format 4 (slave's).
      */
      /*
        Set 'created' to 0, so that in next relay logs this event does not
        trigger cleaning actions on the slave in
        Format_description_log_event::exec_event().
      */
      description_event_for_queue->created= 0;
      /* Don't set log_pos in event header */
      description_event_for_queue->artificial_event=1;
      
      if (description_event_for_queue->write(&log_file))
        goto err;
      bytes_written+= description_event_for_queue->data_written;
    }
    if (flush_io_cache(&log_file) ||
        my_sync(log_file.file, MYF(MY_WME)))
      goto err;

    if (write_file_name_to_index_file)
    {
      /*
        As this is a new log file, we write the file name to the index
        file. As every time we write to the index file, we sync it.
      */
      if (my_b_write(&index_file, (byte*) log_file_name,
		     strlen(log_file_name)) ||
	  my_b_write(&index_file, (byte*) "\n", 1) ||
	  flush_io_cache(&index_file) ||
          my_sync(index_file.file, MYF(MY_WME)))
	goto err;
    }
    break;
  }
  case LOG_CLOSED:				// Impossible
  case LOG_TO_BE_OPENED:
    DBUG_ASSERT(1);
    break;
  }
  DBUG_RETURN(0);

err:
  sql_print_error("Could not use %s for logging (error %d). \
Turning logging off for the whole duration of the MySQL server process. \
To turn it on again: fix the cause, \
shutdown the MySQL server and restart it.", log_name, errno);
  if (file >= 0)
    my_close(file,MYF(0));
  if (index_file_nr >= 0)
    my_close(index_file_nr,MYF(0));
  end_io_cache(&log_file);
  end_io_cache(&index_file);
  safeFree(name);
  log_type= LOG_CLOSED;
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
  if (my_chsize(file, offset - init_offset, '\n', MYF(MY_WME)) ||
      my_sync(file, MYF(MY_WME)))
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
  close(LOG_CLOSE_TO_BE_OPENED);

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
  close(LOG_CLOSE_INDEX);
  my_delete(index_file_name, MYF(MY_WME));	// Reset (open will update)
  if (!thd->slave_thread)
    need_start_event=1;
  open(save_name, save_log_type, 0, index_file_name,
       io_cache_type, no_auto_events, max_size, 0);
  my_free((gptr) save_name, MYF(0));

err:  
  pthread_mutex_unlock(&LOCK_index);
  pthread_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}


/*
  Delete relay log files prior to rli->group_relay_log_name
  (i.e. all logs which are not involved in a non-finished group
  (transaction)), remove them from the index file and start on next relay log.

  SYNOPSIS
    purge_first_log()
    rli		 Relay log information
    included     If false, all relay logs that are strictly before
                 rli->group_relay_log_name are deleted ; if true, the latter is
                 deleted too (i.e. all relay logs
                 read by the SQL slave thread are deleted).
    
  NOTE
    - This is only called from the slave-execute thread when it has read
      all commands from a relay log and want to switch to a new relay log.
    - When this happens, we can be in an active transaction as
      a transaction can span over two relay logs
      (although it is always written as a single block to the master's binary 
      log, hence cannot span over two master's binary logs).

  IMPLEMENTATION
    - Protects index file with LOCK_index
    - Delete relevant relay log files
    - Copy all file names after these ones to the front of the index file
    - If the OS has truncate, truncate the file, else fill it with \n'
    - Read the next file name from the index file and store in rli->linfo

  RETURN VALUES
    0			ok
    LOG_INFO_EOF	End of log-index-file found
    LOG_INFO_SEEK	Could not allocate IO cache
    LOG_INFO_IO		Got IO error while reading file
*/

#ifdef HAVE_REPLICATION

int MYSQL_LOG::purge_first_log(struct st_relay_log_info* rli, bool included) 
{
  int error;
  DBUG_ENTER("purge_first_log");

  DBUG_ASSERT(is_open());
  DBUG_ASSERT(rli->slave_running == 1);
  DBUG_ASSERT(!strcmp(rli->linfo.log_file_name,rli->event_relay_log_name));

  pthread_mutex_lock(&LOCK_index);
  pthread_mutex_lock(&rli->log_space_lock);
  rli->relay_log.purge_logs(rli->group_relay_log_name, included,
                            0, 0, &rli->log_space_total);
  // Tell the I/O thread to take the relay_log_space_limit into account
  rli->ignore_log_space_limit= 0;
  pthread_mutex_unlock(&rli->log_space_lock);

  /*
    Ok to broadcast after the critical region as there is no risk of
    the mutex being destroyed by this thread later - this helps save
    context switches
  */
  pthread_cond_broadcast(&rli->log_space_cond);
  
  /*
    Read the next log file name from the index file and pass it back to
    the caller
    If included is true, we want the first relay log;
    otherwise we want the one after event_relay_log_name.
  */
  if ((included && (error=find_log_pos(&rli->linfo, NullS, 0))) ||
      (!included &&
       ((error=find_log_pos(&rli->linfo, rli->event_relay_log_name, 0)) ||
        (error=find_next_log(&rli->linfo, 0)))))
  {
    char buff[22];
    sql_print_error("next log error: %d  offset: %s  log: %s included: %d",
                    error,
                    llstr(rli->linfo.index_file_offset,buff),
                    rli->group_relay_log_name,
                    included);
    goto err;
  }

  /*
    Reset rli's coordinates to the current log.
  */
  rli->event_relay_log_pos= BIN_LOG_HEADER_SIZE;
  strmake(rli->event_relay_log_name,rli->linfo.log_file_name,
	  sizeof(rli->event_relay_log_name)-1);

  /*
    If we removed the rli->group_relay_log_name file,
    we must update the rli->group* coordinates, otherwise do not touch it as the
    group's execution is not finished (e.g. COMMIT not executed)
  */
  if (included)
  {
    rli->group_relay_log_pos = BIN_LOG_HEADER_SIZE;
    strmake(rli->group_relay_log_name,rli->linfo.log_file_name,
            sizeof(rli->group_relay_log_name)-1);
    rli->notify_group_relay_log_name_update();
  }

  /* Store where we are in the new file for the execution thread */
  flush_relay_log_info(rli);

err:
  pthread_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}

/*
  Update log index_file
*/

int MYSQL_LOG::update_log_index(LOG_INFO* log_info, bool need_update_threads)
{
  if (copy_up_file_and_fill(&index_file, log_info->index_file_start_offset))
    return LOG_INFO_IO;

  // now update offsets in index file for running threads
  if (need_update_threads)
    adjust_linfo_offsets(log_info->index_file_start_offset);
  return 0;
}

/*
  Remove all logs before the given log from disk and from the index file.

  SYNOPSIS
    purge_logs()
    to_log	        Delete all log file name before this file. 
    included            If true, to_log is deleted too.
    need_mutex
    need_update_threads If we want to update the log coordinates of
                        all threads. False for relay logs, true otherwise.
    freed_log_space     If not null, decrement this variable of
                        the amount of log space freed

  NOTES
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  RETURN VALUES
    0				ok
    LOG_INFO_EOF		to_log not found
*/

int MYSQL_LOG::purge_logs(const char *to_log, 
                          bool included,
                          bool need_mutex, 
                          bool need_update_threads, 
                          ulonglong *decrease_log_space)
{
  int error;
  bool exit_loop= 0;
  LOG_INFO log_info;
  DBUG_ENTER("purge_logs");
  DBUG_PRINT("info",("to_log= %s",to_log));

  if (need_mutex)
    pthread_mutex_lock(&LOCK_index);
  if ((error=find_log_pos(&log_info, to_log, 0 /*no mutex*/)))
    goto err;

  /*
    File name exists in index file; delete until we find this file
    or a file that is used.
  */
  if ((error=find_log_pos(&log_info, NullS, 0 /*no mutex*/)))
    goto err;
  while ((strcmp(to_log,log_info.log_file_name) || (exit_loop=included)) &&
         !log_in_use(log_info.log_file_name))
  {
    ulong file_size= 0;
    if (decrease_log_space) //stat the file we want to delete
    {
      MY_STAT s;

      /* 
         If we could not stat, we can't know the amount
         of space that deletion will free. In most cases,
         deletion won't work either, so it's not a problem.
      */
      if (my_stat(log_info.log_file_name,&s,MYF(0)))
        file_size= s.st_size;
    }
    /*
      It's not fatal if we can't delete a log file ;
      if we could delete it, take its size into account
    */
    DBUG_PRINT("info",("purging %s",log_info.log_file_name));
    if (!my_delete(log_info.log_file_name, MYF(0)) && decrease_log_space)
      *decrease_log_space-= file_size;
    if (find_next_log(&log_info, 0) || exit_loop)
      break;
  }

  /*
    If we get killed -9 here, the sysadmin would have to edit
    the log index file after restart - otherwise, this should be safe
  */
  error= update_log_index(&log_info, need_update_threads);

err:
  if (need_mutex)
    pthread_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}

/*
  Remove all logs before the given file date from disk and from the
  index file.

  SYNOPSIS
    purge_logs_before_date()
    thd		Thread pointer
    before_date	Delete all log files before given date.

  NOTES
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  RETURN VALUES
    0				ok
    LOG_INFO_PURGE_NO_ROTATE	Binary file that can't be rotated
*/

int MYSQL_LOG::purge_logs_before_date(time_t purge_time)
{
  int error;
  LOG_INFO log_info;
  MY_STAT stat_area;

  DBUG_ENTER("purge_logs_before_date");

  pthread_mutex_lock(&LOCK_index);

  /*
    Delete until we find curren file
    or a file that is used or a file
    that is older than purge_time.
  */
  if ((error=find_log_pos(&log_info, NullS, 0 /*no mutex*/)))
    goto err;

  while (strcmp(log_file_name, log_info.log_file_name) &&
	 !log_in_use(log_info.log_file_name))
  {
    /* It's not fatal even if we can't delete a log file */
    if (!my_stat(log_info.log_file_name, &stat_area, MYF(0)) ||
	stat_area.st_mtime >= purge_time)
      break;
    my_delete(log_info.log_file_name, MYF(0));
    if (find_next_log(&log_info, 0))
      break;
  }

  /*
    If we get killed -9 here, the sysadmin would have to edit
    the log index file after restart - otherwise, this should be safe
  */
  error= update_log_index(&log_info, 1);

err:
  pthread_mutex_unlock(&LOCK_index);
  DBUG_RETURN(error);
}


#endif /* HAVE_REPLICATION */


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
  uint dir_len = dirname_length(log_file_name); 
  if (dir_len > FN_REFLEN)
    dir_len=FN_REFLEN-1;
  strnmov(buf, log_file_name, dir_len);
  strmake(buf+dir_len, log_ident, FN_REFLEN - dir_len);
}


/*
  Check if we are writing/reading to the given log file
*/

bool MYSQL_LOG::is_active(const char *log_file_name_arg)
{
  return !strcmp(log_file_name, log_file_name_arg);
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

  DBUG_ENTER("MYSQL_LOG::new_file");
  if (!is_open())
  {
    DBUG_PRINT("info",("log is closed"));
    DBUG_VOID_RETURN;
  }

  if (need_lock)
  {
    pthread_mutex_lock(&LOCK_log);
    pthread_mutex_lock(&LOCK_index);
  }    
  safe_mutex_assert_owner(&LOCK_log);
  safe_mutex_assert_owner(&LOCK_index);

  /* Reuse old name if not binlog and not update log */
  new_name_ptr= name;

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
      THD *thd = current_thd; /* may be 0 if we are reacting to SIGHUP */
      Rotate_log_event r(thd,new_name+dirname_length(new_name));
      r.write(&log_file);
      bytes_written += r.data_written;
    }
    /*
      Update needs to be signalled even if there is no rotate event
      log rotation should give the waiting thread a signal to
      discover EOF and move on to the next log.
    */
    signal_update(); 
  }
  old_name=name;
  save_log_type=log_type;
  name=0;				// Don't free name
  close(LOG_CLOSE_TO_BE_OPENED);

  /* 
     Note that at this point, log_type != LOG_CLOSED (important for is_open()).
  */

  /* 
     new_file() is only used for rotation (in FLUSH LOGS or because size >
     max_binlog_size or max_relay_log_size). 
     If this is a binary log, the Format_description_log_event at the beginning of
     the new file should have created=0 (to distinguish with the
     Format_description_log_event written at server startup, which should
     trigger temp tables deletion on slaves.
  */ 

  open(old_name, save_log_type, new_name_ptr, index_file_name, io_cache_type,
       no_auto_events, max_size, 1);
  if (this == &mysql_bin_log)
    report_pos_in_innodb();
  my_free(old_name,MYF(0));

end:
  if (need_lock)
  {
    pthread_mutex_unlock(&LOCK_index);
    pthread_mutex_unlock(&LOCK_log);
  }
  DBUG_VOID_RETURN;
}


bool MYSQL_LOG::append(Log_event* ev)
{
  bool error = 0;
  pthread_mutex_lock(&LOCK_log);
  DBUG_ENTER("MYSQL_LOG::append");

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
  bytes_written+= ev->data_written;
  DBUG_PRINT("info",("max_size: %lu",max_size));
  if ((uint) my_b_append_tell(&log_file) > max_size)
  {
    pthread_mutex_lock(&LOCK_index);
    new_file(0);
    pthread_mutex_unlock(&LOCK_index);
  }

err:  
  pthread_mutex_unlock(&LOCK_log);
  signal_update();				// Safe as we don't call close
  DBUG_RETURN(error);
}


bool MYSQL_LOG::appendv(const char* buf, uint len,...)
{
  bool error= 0;
  DBUG_ENTER("MYSQL_LOG::appendv");
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
  DBUG_PRINT("info",("max_size: %lu",max_size));
  if ((uint) my_b_append_tell(&log_file) > max_size)
  {
    pthread_mutex_lock(&LOCK_index);
    new_file(0);
    pthread_mutex_unlock(&LOCK_index);
  }

err:
  pthread_mutex_unlock(&LOCK_log);
  if (!error)
    signal_update();
  DBUG_RETURN(error);
}


/*
  Write to normal (not rotable) log
  This is the format for the 'normal' log.
*/

bool MYSQL_LOG::write(THD *thd,enum enum_server_command command,
		      const char *format,...)
{
  if (is_open() && (what_to_log & (1L << (uint) command)))
  {
    uint length;
    int error= 0;
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
	if ((thd->options & OPTION_LOG_OFF)
#ifndef NO_EMBEDDED_ACCESS_CHECKS
	    && (thd->master_access & SUPER_ACL)
#endif
)
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
      length=my_sprintf(buff,
			(buff, "%7ld %-11.11s", id,
			 command_name[(uint) command]));
      if (my_b_write(&log_file, (byte*) buff,length))
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


inline bool sync_binlog(IO_CACHE *cache)
{
  if (sync_binlog_period == ++sync_binlog_counter && sync_binlog_period)
  {
    sync_binlog_counter= 0;
    return my_sync(cache->file, MYF(MY_WME));
  }
  return 0;
}


/*
  Write an event to the binary log
*/

bool MYSQL_LOG::write(Log_event* event_info)
{
  THD *thd=event_info->thd;
  bool called_handler_commit=0;
  bool error=0;
  bool should_rotate = 0;
  DBUG_ENTER("MYSQL_LOG::write(event)");
  
  pthread_mutex_lock(&LOCK_log);

  /* 
     In most cases this is only called if 'is_open()' is true; in fact this is
     mostly called if is_open() *was* true a few instructions before, but it
     could have changed since.
  */
  if (is_open())
  {
    const char *local_db= event_info->get_db();
    IO_CACHE *file= &log_file;
#ifdef USING_TRANSACTIONS    
    /*
      Should we write to the binlog cache or to the binlog on disk?
      Write to the binlog cache if:
      - it is already not empty (meaning we're in a transaction; note that the
     present event could be about a non-transactional table, but still we need
     to write to the binlog cache in that case to handle updates to mixed
     trans/non-trans table types the best possible in binlogging)
      - or if the event asks for it (cache_stmt == true).
    */
    if (opt_using_transactions &&
	(event_info->get_cache_stmt() ||
	 (thd && my_b_tell(&thd->transaction.trans_log))))
      file= &thd->transaction.trans_log;
#endif
    DBUG_PRINT("info",("event type=%d",event_info->get_type_code()));
#ifdef HAVE_REPLICATION
    /* 
       In the future we need to add to the following if tests like
       "do the involved tables match (to be implemented)
        binlog_[wild_]{do|ignore}_table?" (WL#1049)"
    */
    if ((thd && !(thd->options & OPTION_BIN_LOG)) ||
	(local_db && !db_ok(local_db, binlog_do_db, binlog_ignore_db)))
    {
      VOID(pthread_mutex_unlock(&LOCK_log));
      DBUG_PRINT("error",("!db_ok"));
      DBUG_RETURN(0);
    }
#endif /* HAVE_REPLICATION */

    error=1;
    /*
      No check for auto events flag here - this write method should
      never be called if auto-events are enabled
    */

    /*
    1. Write first log events which describe the 'run environment'
    of the SQL command
    */

    if (thd)
    {
      /* NOTE: CHARSET AND TZ REPL WILL BE REWRITTEN SHORTLY */
      /*
        To make replication of charsets working in 4.1 we are writing values
        of charset related variables before every statement in the binlog,
        if values of those variables differ from global server-wide defaults.
        We are using SET ONE_SHOT command so that the charset vars get reset
        to default after the first non-SET statement.
        In the next 5.0 this won't be needed as we will use the new binlog
        format to store charset info.
      */
      if ((thd->variables.character_set_client->number !=
           global_system_variables.collation_server->number) ||
          (thd->variables.character_set_client->number !=
           thd->variables.collation_connection->number) ||
          (thd->variables.collation_server->number !=
           thd->variables.collation_connection->number))
      {
	char buf[200];
        int written= my_snprintf(buf, sizeof(buf)-1,
                    "SET ONE_SHOT CHARACTER_SET_CLIENT=%u,\
COLLATION_CONNECTION=%u,COLLATION_DATABASE=%u,COLLATION_SERVER=%u",
                             (uint) thd->variables.character_set_client->number,
                             (uint) thd->variables.collation_connection->number,
                             (uint) thd->variables.collation_database->number,
                             (uint) thd->variables.collation_server->number);
	Query_log_event e(thd, buf, written, 0);
	if (e.write(file))
	  goto err;
      }
      /*
        We use the same ONE_SHOT trick for making replication of time zones 
        working in 4.1. Again in 5.0 we have better means for doing this.
      */
      if (thd->time_zone_used &&
          thd->variables.time_zone != global_system_variables.time_zone)
      {
        char buf[MAX_TIME_ZONE_NAME_LENGTH + 26];
        char *buf_end= strxmov(buf, "SET ONE_SHOT TIME_ZONE='", 
                               thd->variables.time_zone->get_name()->ptr(),
                               "'", NullS);
        Query_log_event e(thd, buf, buf_end - buf, 0);
        if (e.write(file))
          goto err;
      }

      if (thd->last_insert_id_used)
      {
	Intvar_log_event e(thd,(uchar) LAST_INSERT_ID_EVENT,
			   thd->current_insert_id);
	if (e.write(file))
	  goto err;
      }
      if (thd->insert_id_used)
      {
	Intvar_log_event e(thd,(uchar) INSERT_ID_EVENT,thd->last_insert_id);
	if (e.write(file))
	  goto err;
      }
      if (thd->rand_used)
      {
	Rand_log_event e(thd,thd->rand_saved_seed1,thd->rand_saved_seed2);
	if (e.write(file))
	  goto err;
      }
      if (thd->user_var_events.elements)
      {
	for (uint i= 0; i < thd->user_var_events.elements; i++)
	{
	  BINLOG_USER_VAR_EVENT *user_var_event;
	  get_dynamic(&thd->user_var_events,(gptr) &user_var_event, i);
          User_var_log_event e(thd, user_var_event->user_var_event->name.str,
                               user_var_event->user_var_event->name.length,
                               user_var_event->value,
                               user_var_event->length,
                               user_var_event->type,
			       user_var_event->charset_number);
	  if (e.write(file))
	    goto err;
	}
      }
#ifdef TO_BE_REMOVED
      if (thd->variables.convert_set)
      {
	char buf[256], *p;
	p= strmov(strmov(buf, "SET CHARACTER SET "),
		  thd->variables.convert_set->name);
	Query_log_event e(thd, buf, (ulong) (p - buf), 0);
	if (e.write(file))
	  goto err;
      }
#endif
    }

    /* Write the SQL command */

    if (event_info->write(file))
      goto err;

    /*
      Tell for transactional table handlers up to which position in the
      binlog file we wrote. The table handler can store this info, and
      after crash recovery print for the user the offset of the last
      transactions which were recovered. Actually, we must also call
      the table handler commit here, protected by the LOCK_log mutex,
      because otherwise the transactions may end up in a different order
      in the table handler log!

      Note that we will NOT call ha_report_binlog_offset_and_commit() if
      there are binlog events cached in the transaction cache. That is
      because then the log event which we write to the binlog here is
      not a transactional event. In versions < 4.0.13 before this fix this
      caused an InnoDB transaction to be committed if in the middle there
      was a MyISAM event!
    */

    if (file == &log_file) // we are writing to the real log (disk)
    {
      if (flush_io_cache(file) || sync_binlog(file))
	goto err;

      if (opt_using_transactions && !my_b_tell(&thd->transaction.trans_log))
      {
        /*
          LOAD DATA INFILE in AUTOCOMMIT=1 mode writes to the binlog
          chunks also before it is successfully completed. We only report
          the binlog write and do the commit inside the transactional table
          handler if the log event type is appropriate.
        */
        
        if (event_info->get_type_code() == QUERY_EVENT ||
            event_info->get_type_code() == EXEC_LOAD_EVENT)
        {
#ifndef DBUG_OFF
          if (unlikely(opt_crash_binlog_innodb))
          {
            /*
              This option is for use in rpl_crash_binlog_innodb.test.
              1st we want to verify that Binlog_dump thread cannot send the
              event now (because of LOCK_log): we here tell the Binlog_dump
              thread to wake up, sleep for the slave to have time to possibly
              receive data from the master (it should not), and then crash.
              2nd we want to verify that at crash recovery the rolled back
              event is cut from the binlog.
            */
            if (!(--opt_crash_binlog_innodb))
            {
              signal_update();
              sleep(2);
              fprintf(stderr,"This is a normal crash because of"
                      " --crash-binlog-innodb\n");
              assert(0);
            }
            DBUG_PRINT("info",("opt_crash_binlog_innodb: %d",
                               opt_crash_binlog_innodb));
          }
#endif
          error = ha_report_binlog_offset_and_commit(thd, log_file_name,
                                                     file->pos_in_file);
          called_handler_commit=1;
        }
      }
      /* We wrote to the real log, check automatic rotation; */
      DBUG_PRINT("info",("max_size: %lu",max_size));      
      should_rotate= (my_b_tell(file) >= (my_off_t) max_size); 
    }
    error=0;

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

  /*
    Flush the transactional handler log file now that we have released
    LOCK_log; the flush is placed here to eliminate the bottleneck on the
    group commit
  */

  if (called_handler_commit)
    ha_commit_complete(thd);

#ifdef HAVE_REPLICATION
  if (should_rotate && expire_logs_days)
  {
    long purge_time= time(0) - expire_logs_days*24*60*60;
    if (purge_time >= 0)
      error= purge_logs_before_date(purge_time);
  }
#endif
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

  SYNOPSIS
    write()
    thd 		
    cache		The cache to copy to the binlog
    commit_or_rollback  If true, will write "COMMIT" in the end, if false will
                        write "ROLLBACK".

  NOTE
    - We only come here if there is something in the cache.
    - The thing in the cache is always a complete transaction
    - 'cache' needs to be reinitialized after this functions returns.

  IMPLEMENTATION
    - To support transaction over replication, we wrap the transaction
      with BEGIN/COMMIT or BEGIN/ROLLBACK in the binary log.
      We want to write a BEGIN/ROLLBACK block when a non-transactional table was
      updated in a transaction which was rolled back. This is to ensure that the
      same updates are run on the slave.
*/

bool MYSQL_LOG::write(THD *thd, IO_CACHE *cache, bool commit_or_rollback)
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
	slave: it would prevent this one from knowing where he is in the
	master's binlog, which would result in wrong positions being shown to
	the user, MASTER_POS_WAIT undue waiting etc.
      */
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
      Query_log_event qinfo(thd, 
                            commit_or_rollback ? "COMMIT" : "ROLLBACK",
                            commit_or_rollback ? 6        : 8, 
                            TRUE);
      if (qinfo.write(&log_file) || flush_io_cache(&log_file) ||
          sync_binlog(&log_file))
	goto err;
    }
    if (cache->error)				// Error on read
    {
      sql_print_error(ER(ER_ERROR_ON_READ), cache->file_name, errno);
      write_error=1;				// Don't give more errors
      goto err;
    }
#ifndef DBUG_OFF
    if (unlikely(opt_crash_binlog_innodb))
    {
      /* see the previous MYSQL_LOG::write() method for a comment */
      if (!(--opt_crash_binlog_innodb))
      {
        signal_update();
        sleep(2);
        fprintf(stderr, "This is a normal crash because of"
                " --crash-binlog-innodb\n");
        assert(0);
      }
      DBUG_PRINT("info",("opt_crash_binlog_innodb: %d",
                         opt_crash_binlog_innodb));
    }
#endif
    if ((ha_report_binlog_offset_and_commit(thd, log_file_name,
					    log_file.pos_in_file)))
      goto err;
    signal_update();
    DBUG_PRINT("info",("max_size: %lu",max_size));
    if (my_b_tell(&log_file) >= (my_off_t) max_size)
    {
      pthread_mutex_lock(&LOCK_index);
      new_file(0); // inside mutex
      pthread_mutex_unlock(&LOCK_index);
    }

  }
  VOID(pthread_mutex_unlock(&LOCK_log));

  /* Flush the transactional handler log file now that we have released
  LOCK_log; the flush is placed here to eliminate the bottleneck on the
  group commit */  

  ha_commit_complete(thd);

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
  Write to the slow query log.
*/

bool MYSQL_LOG::write(THD *thd,const char *query, uint query_length,
		      time_t query_start_arg)
{
  bool error=0;
  time_t current_time;
  if (!is_open())
    return 0;
  VOID(pthread_mutex_lock(&LOCK_log));
  if (is_open())
  {						// Safety agains reopen
    int tmp_errno=0;
    char buff[80],*end;
    end=buff;
    if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT) || query_start_arg)
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
                      thd->priv_user ? thd->priv_user : "",
                      thd->user ? thd->user : "",
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
      if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT))
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
  return error;
}


/*
  Wait until we get a signal that the binary log has been updated

  SYNOPSIS
    wait_for_update()
    thd			Thread variable
    master_or_slave     If 0, the caller is the Binlog_dump thread from master;
                        if 1, the caller is the SQL thread from the slave. This
                        influences only thd->proc_info.

  NOTES
    One must have a lock on LOCK_log before calling this function.
    This lock will be freed before return! That's required by
    THD::enter_cond() (see NOTES in sql_class.h).
*/

void MYSQL_LOG:: wait_for_update(THD* thd, bool master_or_slave)
{
  const char* old_msg = thd->enter_cond(&update_cond, &LOCK_log,
                                        master_or_slave ?
                                        "Has read all relay log; waiting for \
the slave I/O thread to update it" : 
                                        "Has sent all binlog to slave; \
waiting for binlog to be updated"); 
  pthread_cond_wait(&update_cond, &LOCK_log);
  thd->exit_cond(old_msg);
}


/*
  Close the log file

  SYNOPSIS
    close()
    exiting	Bitmask for one or more of the following bits:
    		LOG_CLOSE_INDEX if we should close the index file
		LOG_CLOSE_TO_BE_OPENED if we intend to call open
		at once after close.
		LOG_CLOSE_STOP_EVENT write a 'stop' event to the log

  NOTES
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void MYSQL_LOG::close(uint exiting)
{					// One can't set log_type here!
  DBUG_ENTER("MYSQL_LOG::close");
  DBUG_PRINT("enter",("exiting: %d", (int) exiting));
  if (log_type != LOG_CLOSED && log_type != LOG_TO_BE_OPENED)
  {
#ifdef HAVE_REPLICATION
    if (log_type == LOG_BIN && !no_auto_events &&
	(exiting & LOG_CLOSE_STOP_EVENT))
    {
      Stop_log_event s;
      s.write(&log_file);
      bytes_written+= s.data_written;
      signal_update();
    }
#endif /* HAVE_REPLICATION */
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

  if ((exiting & LOG_CLOSE_INDEX) && my_b_inited(&index_file))
  {
    end_io_cache(&index_file);
    if (my_close(index_file.file, MYF(0)) < 0 && ! write_error)
    {
      write_error= 1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), index_file_name, errno);
    }
  }
  log_type= (exiting & LOG_CLOSE_TO_BE_OPENED) ? LOG_TO_BE_OPENED : LOG_CLOSED;
  safeFree(name);
  DBUG_VOID_RETURN;
}


void MYSQL_LOG::set_max_size(ulong max_size_arg)
{
  /*
    We need to take locks, otherwise this may happen:
    new_file() is called, calls open(old_max_size), then before open() starts,
    set_max_size() sets max_size to max_size_arg, then open() starts and
    uses the old_max_size argument, so max_size_arg has been overwritten and
    it's like if the SET command was never run.
  */
  DBUG_ENTER("MYSQL_LOG::set_max_size");
  pthread_mutex_lock(&LOCK_log);
  if (is_open())
    max_size= max_size_arg;
  pthread_mutex_unlock(&LOCK_log);
  DBUG_VOID_RETURN;
}


Disable_binlog::Disable_binlog(THD *thd_arg) : 
  thd(thd_arg), save_options(thd_arg->options)
{
  thd_arg->options&= ~OPTION_BIN_LOG;
}


Disable_binlog::~Disable_binlog()
{
  thd->options= save_options;
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
  while (my_isdigit(files_charset_info,*str) ||
	 (allow_wildcards && (*str == wild_many || *str == wild_one)))
  {
    flag=1;
    str++;
  }
  if (*str == '.')
  {
    for (str++ ;
	 my_isdigit(files_charset_info,*str) ||
	   (allow_wildcards && (*str == wild_many || *str == wild_one)) ;
	 str++, flag=1) ;
  }
  if (*str != 0 || flag == 0)
    DBUG_RETURN(0);
  if (res)
    *res=atol(start);
  DBUG_RETURN(1);			/* Number ok */
} /* test_if_number */


void print_buffer_to_file(enum loglevel level, const char *buffer)
{
  time_t skr;
  struct tm tm_tmp;
  struct tm *start;
  DBUG_ENTER("print_buffer_to_file");
  DBUG_PRINT("enter",("buffer: %s", buffer));

  VOID(pthread_mutex_lock(&LOCK_error_log));

  skr=time(NULL);
  localtime_r(&skr, &tm_tmp);
  start=&tm_tmp;
  fprintf(stderr, "%02d%02d%02d %2d:%02d:%02d  [%s] %s\n",
    	  start->tm_year % 100,
  	  start->tm_mon+1,
	  start->tm_mday,
	  start->tm_hour,
	  start->tm_min,
	  start->tm_sec,
          (level == ERROR_LEVEL ? "ERROR" : level == WARNING_LEVEL ?
           "WARNING" : "INFORMATION"),
          buffer);

  fflush(stderr);

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


/*
  If the server has InnoDB on, and InnoDB has published the position of the
  last committed transaction (which happens only if a crash recovery occured at
  this startup) then truncate the previous binary log at the position given by
  InnoDB. If binlog is shorter than the position, print a message to the error
  log.

  SYNOPSIS
    cut_spurious_tail()

  RETURN VALUES
    1	Error
    0	Ok
*/

bool MYSQL_LOG::cut_spurious_tail()
{
  int error= 0;
  DBUG_ENTER("cut_spurious_tail");

#ifdef HAVE_INNOBASE_DB
  if (have_innodb != SHOW_OPTION_YES)
    DBUG_RETURN(0);
  /*
    This is the place where we use information from InnoDB to cut the
    binlog.
  */
  char *name= ha_innobase::get_mysql_bin_log_name();
  ulonglong pos= ha_innobase::get_mysql_bin_log_pos();
  ulonglong actual_size;
  char llbuf1[22], llbuf2[22];

  if (name[0] == 0 || pos == ULONGLONG_MAX)
  {
    DBUG_PRINT("info", ("InnoDB has not set binlog info"));
    DBUG_RETURN(0);
  }
  /* The binlog given by InnoDB normally is never an active binlog */
  if (is_open() && is_active(name))
  {
    sql_print_error("Warning: after InnoDB crash recovery, InnoDB says that "
                    "the binary log of the previous run has the same name "
                    "'%s' as the current one; this is likely to be abnormal.",
                    name);
    DBUG_RETURN(1);
  }
  sql_print_error("After InnoDB crash recovery, checking if the binary log "
                  "'%s' contains rolled back transactions which must be "
                  "removed from it...", name);
  /* If we have a too long binlog, cut. If too short, print error */
  int fd= my_open(name, O_EXCL | O_APPEND | O_BINARY | O_WRONLY, MYF(MY_WME));
  if (fd < 0)
  {
    int save_errno= my_errno;
    sql_print_error("Could not open the binary log '%s' for truncation.",
                    name);
    if (save_errno != ENOENT)
      sql_print_error("The binary log '%s' should not be used for "
                      "replication.", name);    
    DBUG_RETURN(1);
  }

  if (pos > (actual_size= my_seek(fd, 0L, MY_SEEK_END, MYF(MY_WME))))
  {
    /*
      Note that when we have MyISAM rollback this error message should be
      reconsidered.
    */
    sql_print_error("The binary log '%s' is shorter than its expected size "
                    "(actual: %s, expected: %s) so it misses at least one "
                    "committed transaction; so it should not be used for "
                    "replication or point-in-time recovery. You would need "
                    "to restart slaves from a fresh master's data "
                    "snapshot ",
                    name, llstr(actual_size, llbuf1),
                    llstr(pos, llbuf2));
    error= 1;
    goto err;
  }
  if (pos < actual_size)
  {
    sql_print_error("The binary log '%s' is bigger than its expected size "
                    "(actual: %s, expected: %s) so it contains a rolled back "
                    "transaction; now truncating that.", name,
                    llstr(actual_size, llbuf1), llstr(pos, llbuf2));
    /*
      As on some OS, my_chsize() can only pad with 0s instead of really
      truncating. Then mysqlbinlog (and Binlog_dump thread) will error on
      these zeroes. This is annoying, but not more (you just need to manually
      switch replication to the next binlog). Fortunately, in my_chsize.c, it
      says that all modern machines support real ftruncate().
      
    */
    if ((error= my_chsize(fd, pos, 0, MYF(MY_WME))))
      goto err;
  }
err:
  if (my_close(fd, MYF(MY_WME)))
    error= 1;
#endif
  DBUG_RETURN(error);
}


/*
  If the server has InnoDB on, store the binlog name and position into
  InnoDB. This function is used every time we create a new binlog.

  SYNOPSIS
    report_pos_in_innodb()

  NOTES
    This cannot simply be done in MYSQL_LOG::open(), because when we create
    the first binlog at startup, we have not called ha_init() yet so we cannot
    write into InnoDB yet.

  RETURN VALUES
    1	Error
    0	Ok
*/

void MYSQL_LOG::report_pos_in_innodb()
{
  DBUG_ENTER("report_pos_in_innodb");
#ifdef HAVE_INNOBASE_DB
  if (is_open() && have_innodb == SHOW_OPTION_YES)
  {
    DBUG_PRINT("info", ("Reporting binlog info into InnoDB - "
                        "name: '%s' position: %d",
                        log_file_name, my_b_tell(&log_file)));
    innobase_store_binlog_offset_and_flush_log(log_file_name,
                                               my_b_tell(&log_file));
  }
#endif
  DBUG_VOID_RETURN;
}

#ifdef __NT__
void print_buffer_to_nt_eventlog(enum loglevel level, char *buff,
                                 uint length, int buffLen)
{
  HANDLE event;
  char   *buffptr;
  LPCSTR *buffmsgptr;
  DBUG_ENTER("print_buffer_to_nt_eventlog");

  buffptr= buff;
  if (length > (uint)(buffLen-4))
  {
    char *newBuff= new char[length + 4];
    strcpy(newBuff, buff);
    buffptr= newBuff;
  }
  strmov(buffptr+length, "\r\n\r\n");
  buffmsgptr= (LPCSTR*) &buffptr;               // Keep windows happy

  setup_windows_event_source();
  if ((event= RegisterEventSource(NULL,"MySQL")))
  {
    switch (level) {
      case ERROR_LEVEL:
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, MSG_DEFAULT, NULL, 1, 0,
                    buffmsgptr, NULL);
        break;
      case WARNING_LEVEL:
        ReportEvent(event, EVENTLOG_WARNING_TYPE, 0, MSG_DEFAULT, NULL, 1, 0,
                    buffmsgptr, NULL);
        break;
      case INFORMATION_LEVEL:
        ReportEvent(event, EVENTLOG_INFORMATION_TYPE, 0, MSG_DEFAULT, NULL, 1,
                    0, buffmsgptr, NULL);
        break;
    }
    DeregisterEventSource(event);
  }

  /* if we created a string buffer, then delete it */
  if (buffptr != buff)
    delete[] buffptr;

  DBUG_VOID_RETURN;
}
#endif /* __NT__ */


/*
  Prints a printf style message to the error log and, under NT, to the
  Windows event log.

  SYNOPSIS
    vprint_msg_to_log()
    event_type             Type of event to write (Error, Warning, or Info)
    format                 Printf style format of message
    args                   va_list list of arguments for the message    

  NOTE

  IMPLEMENTATION
    This function prints the message into a buffer and then sends that buffer
    to other functions to write that message to other logging sources.

  RETURN VALUES
    void
*/

void vprint_msg_to_log(enum loglevel level, const char *format, va_list args)
{
  char   buff[1024];
  uint length;
  DBUG_ENTER("vprint_msg_to_log");

  length= my_vsnprintf(buff, sizeof(buff)-5, format, args);
  print_buffer_to_file(level, buff);

#ifdef __NT__
  print_buffer_to_nt_eventlog(level, buff, length, sizeof(buff));
#endif

  DBUG_VOID_RETURN;
}


void sql_print_error(const char *format, ...) 
{
  va_list args;
  DBUG_ENTER("sql_print_error");

  va_start(args, format);
  vprint_msg_to_log(ERROR_LEVEL, format, args);
  va_end(args);

  DBUG_VOID_RETURN;
}


void sql_print_warning(const char *format, ...) 
{
  va_list args;
  DBUG_ENTER("sql_print_warning");

  va_start(args, format);
  vprint_msg_to_log(WARNING_LEVEL, format, args);
  va_end(args);

  DBUG_VOID_RETURN;
}


void sql_print_information(const char *format, ...) 
{
  va_list args;
  DBUG_ENTER("sql_print_information");

  va_start(args, format);
  vprint_msg_to_log(INFORMATION_LEVEL, format, args);
  va_end(args);

  DBUG_VOID_RETURN;
}
