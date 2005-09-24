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
#include "sql_repl.h"

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
static int binlog_close_connection(THD *thd);
static int binlog_savepoint_set(THD *thd, void *sv);
static int binlog_savepoint_rollback(THD *thd, void *sv);
static int binlog_commit(THD *thd, bool all);
static int binlog_rollback(THD *thd, bool all);
static int binlog_prepare(THD *thd, bool all);

static handlerton binlog_hton = {
  "binlog",
  0,
  sizeof(my_off_t),             /* savepoint size = binlog offset */
  binlog_close_connection,
  binlog_savepoint_set,
  binlog_savepoint_rollback,
  NULL,                         /* savepoint_release */
  binlog_commit,
  binlog_rollback,
  binlog_prepare,
  NULL,                         /* recover */
  NULL,                         /* commit_by_xid */
  NULL,                         /* rollback_by_xid */
  NULL,                         /* create_cursor_read_view */
  NULL,                         /* set_cursor_read_view */
  NULL,    			/* close_cursor_read_view */
  HTON_NO_FLAGS
};

/*
  this function is mostly a placeholder.
  conceptually, binlog initialization (now mostly done in MYSQL_LOG::open)
  should be moved here.
*/

handlerton *binlog_init()
{
  return &binlog_hton;
}

static int binlog_close_connection(THD *thd)
{
  IO_CACHE *trans_log= (IO_CACHE*)thd->ha_data[binlog_hton.slot];
  DBUG_ASSERT(mysql_bin_log.is_open() && !my_b_tell(trans_log));
  close_cached_file(trans_log);
  my_free((gptr)trans_log, MYF(0));
  return 0;
}

static int binlog_end_trans(THD *thd, IO_CACHE *trans_log, Log_event *end_ev)
{
  int error=0;
  DBUG_ENTER("binlog_end_trans");
  if (end_ev)
    error= mysql_bin_log.write(thd, trans_log, end_ev);

  statistic_increment(binlog_cache_use, &LOCK_status);
  if (trans_log->disk_writes != 0)
  {
    statistic_increment(binlog_cache_disk_use, &LOCK_status);
    trans_log->disk_writes= 0;
  }
  reinit_io_cache(trans_log, WRITE_CACHE, (my_off_t) 0, 0, 1); // cannot fail
  trans_log->end_of_file= max_binlog_cache_size;
  DBUG_RETURN(error);
}

static int binlog_prepare(THD *thd, bool all)
{
  /*
    do nothing.
    just pretend we can do 2pc, so that MySQL won't
    switch to 1pc.
    real work will be done in MYSQL_LOG::log()
  */
  return 0;
}

static int binlog_commit(THD *thd, bool all)
{
  IO_CACHE *trans_log= (IO_CACHE*)thd->ha_data[binlog_hton.slot];
  DBUG_ENTER("binlog_commit");
  DBUG_ASSERT(mysql_bin_log.is_open() &&
     (all || !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))));

  if (!my_b_tell(trans_log))
  {
    // we're here because trans_log was flushed in MYSQL_LOG::log()
    DBUG_RETURN(0);
  }
  Query_log_event qev(thd, "COMMIT", 6, TRUE, FALSE);
  DBUG_RETURN(binlog_end_trans(thd, trans_log, &qev));
}

static int binlog_rollback(THD *thd, bool all)
{
  int error=0;
  IO_CACHE *trans_log= (IO_CACHE*)thd->ha_data[binlog_hton.slot];
  DBUG_ENTER("binlog_rollback");
  /*
    First assert is guaranteed - see trans_register_ha() call below.
    The second must be true. If it is not, we're registering
    unnecessary, doing extra work. The cause should be found and eliminated
  */
  DBUG_ASSERT(all || !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)));
  DBUG_ASSERT(mysql_bin_log.is_open() && my_b_tell(trans_log));
  /*
    Update the binary log with a BEGIN/ROLLBACK block if we have
    cached some queries and we updated some non-transactional
    table. Such cases should be rare (updating a
    non-transactional table inside a transaction...)
  */
  if (unlikely(thd->options & OPTION_STATUS_NO_TRANS_UPDATE))
  {
    Query_log_event qev(thd, "ROLLBACK", 8, TRUE, FALSE);
    error= binlog_end_trans(thd, trans_log, &qev);
  }
  else
    error= binlog_end_trans(thd, trans_log, 0);
  DBUG_RETURN(error);
}

/*
  NOTE: how do we handle this (unlikely but legal) case:
  [transaction] + [update to non-trans table] + [rollback to savepoint] ?
  The problem occurs when a savepoint is before the update to the
  non-transactional table. Then when there's a rollback to the savepoint, if we
  simply truncate the binlog cache, we lose the part of the binlog cache where
  the update is. If we want to not lose it, we need to write the SAVEPOINT
  command and the ROLLBACK TO SAVEPOINT command to the binlog cache. The latter
  is easy: it's just write at the end of the binlog cache, but the former
  should be *inserted* to the place where the user called SAVEPOINT. The
  solution is that when the user calls SAVEPOINT, we write it to the binlog
  cache (so no need to later insert it). As transactions are never intermixed
  in the binary log (i.e. they are serialized), we won't have conflicts with
  savepoint names when using mysqlbinlog or in the slave SQL thread.
  Then when ROLLBACK TO SAVEPOINT is called, if we updated some
  non-transactional table, we don't truncate the binlog cache but instead write
  ROLLBACK TO SAVEPOINT to it; otherwise we truncate the binlog cache (which
  will chop the SAVEPOINT command from the binlog cache, which is good as in
  that case there is no need to have it in the binlog).
*/

static int binlog_savepoint_set(THD *thd, void *sv)
{
  IO_CACHE *trans_log= (IO_CACHE*)thd->ha_data[binlog_hton.slot];
  DBUG_ENTER("binlog_savepoint_set");
  DBUG_ASSERT(mysql_bin_log.is_open() && my_b_tell(trans_log));

  *(my_off_t *)sv= my_b_tell(trans_log);
  /* Write it to the binary log */
  Query_log_event qinfo(thd, thd->query, thd->query_length, TRUE, FALSE);
  DBUG_RETURN(mysql_bin_log.write(&qinfo));
}

static int binlog_savepoint_rollback(THD *thd, void *sv)
{
  IO_CACHE *trans_log= (IO_CACHE*)thd->ha_data[binlog_hton.slot];
  DBUG_ENTER("binlog_savepoint_rollback");
  DBUG_ASSERT(mysql_bin_log.is_open() && my_b_tell(trans_log));

  /*
    Write ROLLBACK TO SAVEPOINT to the binlog cache if we have updated some
    non-transactional table. Otherwise, truncate the binlog cache starting
    from the SAVEPOINT command.
  */
  if (unlikely(thd->options & OPTION_STATUS_NO_TRANS_UPDATE))
  {
    Query_log_event qinfo(thd, thd->query, thd->query_length, TRUE, FALSE);
    DBUG_RETURN(mysql_bin_log.write(&qinfo));
  }
  reinit_io_cache(trans_log, WRITE_CACHE, *(my_off_t *)sv, 0, 0);
  DBUG_RETURN(0);
}

int check_binlog_magic(IO_CACHE* log, const char** errmsg)
{
  char magic[4];
  DBUG_ASSERT(my_b_tell(log) == 0);

  if (my_b_read(log, (byte*) magic, sizeof(magic)))
  {
    *errmsg = "I/O error reading the header from the binary log";
    sql_print_error("%s, errno=%d, io cache code=%d", *errmsg, my_errno,
		    log->error);
    return 1;
  }
  if (memcmp(magic, BINLOG_MAGIC, sizeof(magic)))
  {
    *errmsg = "Binlog has bad magic number;  It's not a binary log file that can be used by this version of MySQL";
    return 1;
  }
  return 0;
}

File open_binlog(IO_CACHE *log, const char *log_file_name, const char **errmsg)
{
  File file;
  DBUG_ENTER("open_binlog");

  if ((file = my_open(log_file_name, O_RDONLY | O_BINARY | O_SHARE, 
                      MYF(MY_WME))) < 0)
  {
    sql_print_error("Failed to open log (file '%s', errno %d)",
                    log_file_name, my_errno);
    *errmsg = "Could not open log file";
    goto err;
  }
  if (init_io_cache(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
                    MYF(MY_WME|MY_DONT_CHECK_FILESIZE)))
  {
    sql_print_error("Failed to create a cache on log (file '%s')",
                    log_file_name);
    *errmsg = "Could not open log file";
    goto err;
  }
  if (check_binlog_magic(log,errmsg))
    goto err;
  DBUG_RETURN(file);

err:
  if (file >= 0)
  {
    my_close(file,MYF(0));
    end_io_cache(log);
  }
  DBUG_RETURN(-1);
}

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
   need_start_event(1), prepared_xids(0), description_event_for_exec(0),
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

/* this is called only once */

void MYSQL_LOG::cleanup()
{
  DBUG_ENTER("cleanup");
  if (inited)
  {
    inited= 0;
    close(LOG_CLOSE_INDEX|LOG_CLOSE_STOP_EVENT);
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

const char *MYSQL_LOG::generate_name(const char *log_name,
                                     const char *suffix,
                                     bool strip_ext, char *buff)
{
  if (!log_name || !log_name[0])
  {
    /*
      TODO: The following should be using fn_format();  We just need to
      first change fn_format() to cut the file name if it's too long.
    */
    strmake(buff,glob_hostname,FN_REFLEN-5);
    strmov(fn_ext(buff),suffix);
    return (const char *)buff;
  }
  // get rid of extension if the log is binary to avoid problems
  if (strip_ext)
  {
    char *p = fn_ext(log_name);
    uint length=(uint) (p-log_name);
    strmake(buff,log_name,min(length,FN_REFLEN));
    return (const char*)buff;
  }
  return log_name;
}

bool MYSQL_LOG::open_index_file(const char *index_file_name_arg,
                                const char *log_name)
{
  File index_file_nr= -1;
  DBUG_ASSERT(!my_b_inited(&index_file));

  /*
    First open of this class instance
    Create an index file that will hold all file names uses for logging.
    Add new entries to the end of it.
  */
  myf opt= MY_UNPACK_FILENAME;
  if (!index_file_name_arg)
  {
    index_file_name_arg= log_name;    // Use same basename for index file
    opt= MY_UNPACK_FILENAME | MY_REPLACE_EXT;
  }
  fn_format(index_file_name, index_file_name_arg, mysql_data_home,
            ".index", opt);
  if ((index_file_nr= my_open(index_file_name,
                              O_RDWR | O_CREAT | O_BINARY ,
                              MYF(MY_WME))) < 0 ||
       my_sync(index_file_nr, MYF(MY_WME)) ||
       init_io_cache(&index_file, index_file_nr,
                     IO_SIZE, WRITE_CACHE,
                     my_seek(index_file_nr,0L,MY_SEEK_END,MYF(0)),
			0, MYF(MY_WME | MY_WAIT_IF_FULL)))
  {
    if (index_file_nr >= 0)
      my_close(index_file_nr,MYF(0));
    return TRUE;
  }
  return FALSE;
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

bool MYSQL_LOG::open(const char *log_name,
                     enum_log_type log_type_arg,
                     const char *new_name,
                     enum cache_type io_cache_type_arg,
                     bool no_auto_events_arg,
                     ulong max_size_arg,
                     bool null_created_arg)
{
  char buff[FN_REFLEN];
  File file= -1;
  int open_flags = O_CREAT | O_BINARY;
  DBUG_ENTER("MYSQL_LOG::open");
  DBUG_PRINT("enter",("log_type: %d",(int) log_type_arg));

  last_time=query_start=0;
  write_error=0;

  init(log_type_arg,io_cache_type_arg,no_auto_events_arg,max_size_arg);

  if (!(name=my_strdup(log_name,MYF(MY_WME))))
  {
    name= (char *)log_name; // for the error message
    goto err;
  }
  if (new_name)
    strmov(log_file_name,new_name);
  else if (generate_new_name(log_file_name, name))
    goto err;

  if (io_cache_type == SEQ_READ_APPEND)
    open_flags |= O_RDWR | O_APPEND;
  else
    open_flags |= O_WRONLY | (log_type == LOG_BIN ? 0 : O_APPEND);

  db[0]=0;
  open_count++;
  if ((file=my_open(log_file_name,open_flags,
		    MYF(MY_WME | ME_WAITTANG))) < 0 ||
      init_io_cache(&log_file, file, IO_SIZE, io_cache_type,
		    my_tell(file,MYF(MY_WME)), 0, 
                    MYF(MY_WME | MY_NABP |
                        ((log_type == LOG_BIN) ? MY_WAIT_IF_FULL : 0))))
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

    DBUG_ASSERT(my_b_inited(&index_file) != 0);
    reinit_io_cache(&index_file, WRITE_CACHE,
                    my_b_filelength(&index_file), 0, 0);
    if (need_start_event && !no_auto_events)
    {
      /*
        In 4.x we set need_start_event=0 here, but in 5.0 we want a Start event
        even if this is not the very first binlog.
      */
      Format_description_log_event s(BINLOG_VERSION);
      /*
        don't set LOG_EVENT_BINLOG_IN_USE_F for SEQ_READ_APPEND io_cache
        as we won't be able to reset it later
      */
      if (io_cache_type == WRITE_CACHE)
        s.flags|= LOG_EVENT_BINLOG_IN_USE_F;
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
        Format_description_log_event::Format_description_log_event(char* buf,).
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
shutdown the MySQL server and restart it.", name, errno);
  if (file >= 0)
    my_close(file,MYF(0));
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
    Mutex needed because we need to make sure the file pointer does not
    move from under our feet
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
  fname[length-1]=0;				// kill \n
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
  /*
    The following mutex is needed to ensure that no threads call
    'delete thd' as we would then risk missing a 'rollback' from this
    thread. If the transaction involved MyISAM tables, it should go
    into binlog even on rollback.
  */
  (void) pthread_mutex_lock(&LOCK_thread_count);

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
  if (!open_index_file(index_file_name, 0))
    open(save_name, save_log_type, 0,
         io_cache_type, no_auto_events, max_size, 0);
  my_free((gptr) save_name, MYF(0));

err:
  (void) pthread_mutex_unlock(&LOCK_thread_count);
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
      else
	sql_print_information("Failed to execute my_stat on file '%s'",
			      log_info.log_file_name);
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
    need_lock		Set to 1 if caller has not locked LOCK_log

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
    pthread_mutex_lock(&LOCK_log);
  pthread_mutex_lock(&LOCK_index);

  safe_mutex_assert_owner(&LOCK_log);
  safe_mutex_assert_owner(&LOCK_index);

  /*
    if binlog is used as tc log, be sure all xids are "unlogged",
    so that on recover we only need to scan one - latest - binlog file
    for prepared xids. As this is expected to be a rare event,
    simple wait strategy is enough. We're locking LOCK_log to be sure no
    new Xid_log_event's are added to the log (and prepared_xids is not
    increased), and waiting on COND_prep_xids for late threads to
    catch up.
  */
  if (prepared_xids)
  {
    tc_log_page_waits++;
    pthread_mutex_lock(&LOCK_prep_xids);
    while (prepared_xids)
      pthread_cond_wait(&COND_prep_xids, &LOCK_prep_xids);
    pthread_mutex_unlock(&LOCK_prep_xids);
  }

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

  open(old_name, save_log_type, new_name_ptr,
       io_cache_type, no_auto_events, max_size, 1);
  my_free(old_name,MYF(0));

end:
  if (need_lock)
    pthread_mutex_unlock(&LOCK_log);
  pthread_mutex_unlock(&LOCK_index);

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
    new_file(0);

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
    new_file(0);

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
	    && (thd->security_ctx->master_access & SUPER_ACL)
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

bool MYSQL_LOG::flush_and_sync()
{
  int err=0, fd=log_file.file;
  safe_mutex_assert_owner(&LOCK_log);
  if (flush_io_cache(&log_file))
    return 1;
  if (++sync_binlog_counter >= sync_binlog_period && sync_binlog_period)
  {
    sync_binlog_counter= 0;
    err=my_sync(fd, MYF(MY_WME));
  }
  return err;
}

void MYSQL_LOG::start_union_events(THD *thd)
{
  DBUG_ASSERT(!thd->binlog_evt_union.do_union);
  thd->binlog_evt_union.do_union= TRUE;
  thd->binlog_evt_union.unioned_events= FALSE;
  thd->binlog_evt_union.unioned_events_trans= FALSE;
  thd->binlog_evt_union.first_query_id= thd->query_id;
}

void MYSQL_LOG::stop_union_events(THD *thd)
{
  DBUG_ASSERT(thd->binlog_evt_union.do_union);
  thd->binlog_evt_union.do_union= FALSE;
}

bool MYSQL_LOG::is_query_in_union(THD *thd, query_id_t query_id_param)
{
  return (thd->binlog_evt_union.do_union && 
          query_id_param >= thd->binlog_evt_union.first_query_id);
}

/*
  Write an event to the binary log
*/

bool MYSQL_LOG::write(Log_event *event_info)
{
  THD *thd= event_info->thd;
  bool error= 1;
  DBUG_ENTER("MYSQL_LOG::write(Log_event *)");

  if (thd->binlog_evt_union.do_union)
  {
    /*
      In Stored function; Remember that function call caused an update.
      We will log the function call to the binary log on function exit
    */
    thd->binlog_evt_union.unioned_events= TRUE;
    thd->binlog_evt_union.unioned_events_trans |= event_info->cache_stmt;
    DBUG_RETURN(0);
  }
  
  pthread_mutex_lock(&LOCK_log);

  /*
     In most cases this is only called if 'is_open()' is true; in fact this is
     mostly called if is_open() *was* true a few instructions before, but it
     could have changed since.
  */
  if (likely(is_open()))
  {
    const char *local_db= event_info->get_db();
    IO_CACHE *file= &log_file;
#ifdef HAVE_REPLICATION
    /*
       In the future we need to add to the following if tests like
       "do the involved tables match (to be implemented)
        binlog_[wild_]{do|ignore}_table?" (WL#1049)"
    */
    if ((thd && !(thd->options & OPTION_BIN_LOG)) ||
	(!db_ok(local_db, binlog_do_db, binlog_ignore_db)))
    {
      VOID(pthread_mutex_unlock(&LOCK_log));
      DBUG_PRINT("error",("!db_ok('%s')", local_db));
      DBUG_RETURN(0);
    }
#endif /* HAVE_REPLICATION */

#ifdef USING_TRANSACTIONS
    /*
      Should we write to the binlog cache or to the binlog on disk?
      Write to the binlog cache if:
      - it is already not empty (meaning we're in a transaction; note that the
     present event could be about a non-transactional table, but still we need
     to write to the binlog cache in that case to handle updates to mixed
     trans/non-trans table types the best possible in binlogging)
      - or if the event asks for it (cache_stmt == TRUE).
    */
    if (opt_using_transactions && thd)
    {
      IO_CACHE *trans_log= (IO_CACHE*)thd->ha_data[binlog_hton.slot];

      if (event_info->get_cache_stmt())
      {
        if (!trans_log)
        {
          thd->ha_data[binlog_hton.slot]= trans_log= (IO_CACHE *)
            my_malloc(sizeof(IO_CACHE), MYF(MY_ZEROFILL));
          if (!trans_log || open_cached_file(trans_log, mysql_tmpdir,
                                             LOG_PREFIX,
                                             binlog_cache_size, MYF(MY_WME)))
          {
            my_free((gptr)trans_log, MYF(MY_ALLOW_ZERO_PTR));
            thd->ha_data[binlog_hton.slot]= trans_log= 0;
            goto err;
          }
          trans_log->end_of_file= max_binlog_cache_size;
          trans_register_ha(thd,
                            thd->options & (OPTION_NOT_AUTOCOMMIT |
                                            OPTION_BEGIN),
                            &binlog_hton);
        }
        else if (!my_b_tell(trans_log))
          trans_register_ha(thd,
                            thd->options & (OPTION_NOT_AUTOCOMMIT |
                                            OPTION_BEGIN),
                            &binlog_hton);
        file= trans_log;
      }
      else if (trans_log && my_b_tell(trans_log))
        file= trans_log;
    }
#endif
    DBUG_PRINT("info",("event type=%d",event_info->get_type_code()));

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
    }

    /*
       Write the SQL command
     */

    if (event_info->write(file))
      goto err;

    if (file == &log_file) // we are writing to the real log (disk)
    {
      if (flush_and_sync())
	goto err;
      signal_update();
      rotate_and_purge(RP_LOCK_LOG_IS_ALREADY_LOCKED);
    }
    error=0;

err:
    if (error)
    {
      if (my_errno == EFBIG)
	my_message(ER_TRANS_CACHE_FULL, ER(ER_TRANS_CACHE_FULL), MYF(0));
      else
	my_error(ER_ERROR_ON_WRITE, MYF(0), name, errno);
      write_error=1;
    }
  }

  pthread_mutex_unlock(&LOCK_log);
  DBUG_RETURN(error);
}

void MYSQL_LOG::rotate_and_purge(uint flags)
{
  if (!(flags & RP_LOCK_LOG_IS_ALREADY_LOCKED))
    pthread_mutex_lock(&LOCK_log);
  if ((flags & RP_FORCE_ROTATE) ||
      (my_b_tell(&log_file) >= (my_off_t) max_size))
  {
    new_file(0);
#ifdef HAVE_REPLICATION
    if (expire_logs_days)
    {
      long purge_time= time(0) - expire_logs_days*24*60*60;
      if (purge_time >= 0)
        purge_logs_before_date(purge_time);
    }
#endif
  }
  if (!(flags & RP_LOCK_LOG_IS_ALREADY_LOCKED))
    pthread_mutex_unlock(&LOCK_log);
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

  NOTE
    - We only come here if there is something in the cache.
    - The thing in the cache is always a complete transaction
    - 'cache' needs to be reinitialized after this functions returns.

  IMPLEMENTATION
    - To support transaction over replication, we wrap the transaction
      with BEGIN/COMMIT or BEGIN/ROLLBACK in the binary log.
      We want to write a BEGIN/ROLLBACK block when a non-transactional table
      was updated in a transaction which was rolled back. This is to ensure
      that the same updates are run on the slave.
*/

bool MYSQL_LOG::write(THD *thd, IO_CACHE *cache, Log_event *commit_event)
{
  DBUG_ENTER("MYSQL_LOG::write(THD *, IO_CACHE *, Log_event *)");
  VOID(pthread_mutex_lock(&LOCK_log));

  if (likely(is_open()))                       // Should always be true
  {
    uint length;

    /*
      Log "BEGIN" at the beginning of the transaction.
      which may contain more than 1 SQL statement.
    */
    if (thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
    {
      Query_log_event qinfo(thd, "BEGIN", 5, TRUE, FALSE);
      /*
        Imagine this is rollback due to net timeout, after all statements of
        the transaction succeeded. Then we want a zero-error code in BEGIN.
        In other words, if there was a really serious error code it's already
        in the statement's events.
        This is safer than thd->clear_error() against kills at shutdown.
      */
      qinfo.error_code= 0;
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
    DBUG_EXECUTE_IF("half_binlogged_transaction", length-=100;);
    do
    {
      /* Write data to the binary log file */
      if (my_b_write(&log_file, cache->read_pos, length))
	goto err;
      cache->read_pos=cache->read_end;		// Mark buffer used up
      DBUG_EXECUTE_IF("half_binlogged_transaction", goto DBUG_skip_commit;);
    } while ((length=my_b_fill(cache)));

    if (commit_event->write(&log_file))
      goto err;
DBUG_skip_commit:
    if (flush_and_sync())
      goto err;
    DBUG_EXECUTE_IF("half_binlogged_transaction", abort(););
    if (cache->error)				// Error on read
    {
      sql_print_error(ER(ER_ERROR_ON_READ), cache->file_name, errno);
      write_error=1;				// Don't give more errors
      goto err;
    }
    signal_update();
    /*
      if commit_event is Xid_log_event, increase the number of
      prepared_xids (it's decreasd in ::unlog()). Binlog cannot be rotated
      if there're prepared xids in it - see the comment in new_file() for
      an explanation.
      If the commit_event is not Xid_log_event (then it's a Query_log_event)
      rotate binlog, if necessary.
    */
    if (commit_event->get_type_code() == XID_EVENT)
      thread_safe_increment(prepared_xids, &LOCK_prep_xids);
    else
      rotate_and_purge(RP_LOCK_LOG_IS_ALREADY_LOCKED);
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
  Write to the slow query log.
*/

bool MYSQL_LOG::write(THD *thd,const char *query, uint query_length,
		      time_t query_start_arg)
{
  bool error=0;
  time_t current_time;
  if (!is_open())
    return 0;
  DBUG_ENTER("MYSQL_LOG::write");

  VOID(pthread_mutex_lock(&LOCK_log));
  if (is_open())
  {						// Safety agains reopen
    int tmp_errno=0;
    char buff[80],*end;
    end=buff;
    if (!(thd->options & OPTION_UPDATE_LOG))
    {
      VOID(pthread_mutex_unlock(&LOCK_log));
      DBUG_RETURN(0);
    }
    if (!(specialflag & SPECIAL_SHORT_LOG_FORMAT) || query_start_arg)
    {
      Security_context *sctx= thd->security_ctx;
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
                      sctx->priv_user ?
                      sctx->priv_user : "",
                      sctx->user ? sctx->user : "",
                      sctx->host ? sctx->host : "",
                      sctx->ip ? sctx->ip : "") ==
          (uint) -1)
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
  DBUG_RETURN(error);
}


/*
  Wait until we get a signal that the binary log has been updated

  SYNOPSIS
    wait_for_update()
    thd			Thread variable
    is_slave            If 0, the caller is the Binlog_dump thread from master;
                        if 1, the caller is the SQL thread from the slave. This
                        influences only thd->proc_info.

  NOTES
    One must have a lock on LOCK_log before calling this function.
    This lock will be released before return! That's required by
    THD::enter_cond() (see NOTES in sql_class.h).
*/

void MYSQL_LOG::wait_for_update(THD* thd, bool is_slave)
{
  const char *old_msg;
  DBUG_ENTER("wait_for_update");
  old_msg= thd->enter_cond(&update_cond, &LOCK_log,
                           is_slave ?
                           "Has read all relay log; waiting for the slave I/O "
                           "thread to update it" :
                           "Has sent all binlog to slave; waiting for binlog "
                           "to be updated");
  pthread_cond_wait(&update_cond, &LOCK_log);
  thd->exit_cond(old_msg);
  DBUG_VOID_RETURN;
}


/*
  Close the log file

  SYNOPSIS
    close()
    exiting     Bitmask for one or more of the following bits:
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

    /* don't pwrite in a file opened with O_APPEND - it doesn't work */
    if (log_file.type == WRITE_CACHE && log_type == LOG_BIN)
    {
      my_off_t offset= BIN_LOG_HEADER_SIZE + FLAGS_OFFSET;
      byte flags=0; // clearing LOG_EVENT_BINLOG_IN_USE_F
      my_pwrite(log_file.file, &flags, 1, offset, MYF(0));
    }

    if (my_sync(log_file.file,MYF(MY_WME)) && ! write_error)
    {
      write_error=1;
      sql_print_error(ER(ER_ERROR_ON_WRITE), name, errno);
    }
    if (my_close(log_file.file,MYF(MY_WME)) && ! write_error)
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
  fprintf(stderr, "%02d%02d%02d %2d:%02d:%02d [%s] %s\n",
          start->tm_year % 100,
          start->tm_mon+1,
          start->tm_mday,
          start->tm_hour,
          start->tm_min,
          start->tm_sec,
          (level == ERROR_LEVEL ? "ERROR" : level == WARNING_LEVEL ?
           "Warning" : "Note"),
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
    VOID(pthread_mutex_lock(&LOCK_error_log));
#ifdef __WIN__
    char err_temp[FN_REFLEN+4];
    /*
     On Windows is necessary a temporary file for to rename
     the current error file.
    */
    strxmov(err_temp, err_renamed,"-tmp",NullS);
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
             my_fwrite(stderr, (byte*) buf, bytes, MYF(0));
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
    VOID(pthread_mutex_unlock(&LOCK_error_log));
  }
   return result;
}

void MYSQL_LOG::signal_update()
{
  DBUG_ENTER("MYSQL_LOG::signal_update");
  pthread_cond_broadcast(&update_cond);
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
  if (length > (uint)(buffLen-5))
  {
    char *newBuff= new char[length + 5];
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


/********* transaction coordinator log for 2pc - mmap() based solution *******/

/*
  the log consists of a file, mmapped to a memory.
  file is divided on pages of tc_log_page_size size.
  (usable size of the first page is smaller because of log header)
  there's PAGE control structure for each page
  each page (or rather PAGE control structure) can be in one of three
  states - active, syncing, pool.
  there could be only one page in active or syncing states,
  but many in pool - pool is fifo queue.
  usual lifecycle of a page is pool->active->syncing->pool
  "active" page - is a page where new xid's are logged.
  the page stays active as long as syncing slot is taken.
  "syncing" page is being synced to disk. no new xid can be added to it.
  when the sync is done the page is moved to a pool and an active page
  becomes "syncing".

  the result of such an architecture is a natural "commit grouping" -
  If commits are coming faster than the system can sync, they do not
  stall. Instead, all commit that came since the last sync are
  logged to the same page, and they all are synced with the next -
  one - sync. Thus, thought individual commits are delayed, throughput
  is not decreasing.

  when a xid is added to an active page, the thread of this xid waits
  for a page's condition until the page is synced. when syncing slot
  becomes vacant one of these waiters is awaken to take care of syncing.
  it syncs the page and signals all waiters that the page is synced.
  PAGE::waiters is used to count these waiters, and a page may never
  become active again until waiters==0 (that is all waiters from the
  previous sync have noticed the sync was completed)

  note, that the page becomes "dirty" and has to be synced only when a
  new xid is added into it. Removing a xid from a page does not make it
  dirty - we don't sync removals to disk.
*/

ulong tc_log_page_waits= 0;

#ifdef HAVE_MMAP

#define TC_LOG_HEADER_SIZE (sizeof(tc_log_magic)+1)

static const char tc_log_magic[]={(char) 254, 0x23, 0x05, 0x74};

ulong opt_tc_log_size= TC_LOG_MIN_SIZE;
ulong tc_log_max_pages_used=0, tc_log_page_size=0, tc_log_cur_pages_used=0;

int TC_LOG_MMAP::open(const char *opt_name)
{
  uint i;
  bool crashed=FALSE;
  PAGE *pg;

  DBUG_ASSERT(total_ha_2pc > 1);
  DBUG_ASSERT(opt_name && opt_name[0]);

  tc_log_page_size= my_getpagesize();
  DBUG_ASSERT(TC_LOG_PAGE_SIZE % tc_log_page_size == 0);

  fn_format(logname,opt_name,mysql_data_home,"",MY_UNPACK_FILENAME);
  if ((fd= my_open(logname, O_RDWR, MYF(0))) < 0)
  {
    if (my_errno != ENOENT)
      goto err;
    if (using_heuristic_recover())
      return 1;
    if ((fd= my_create(logname, O_RDWR, 0, MYF(MY_WME))) < 0)
      goto err;
    inited=1;
    file_length= opt_tc_log_size;
    if (my_chsize(fd, file_length, 0, MYF(MY_WME)))
      goto err;
  }
  else
  {
    inited= 1;
    crashed= TRUE;
    sql_print_information("Recovering after a crash using %s", opt_name);
    if (tc_heuristic_recover)
    {
      sql_print_error("Cannot perform automatic crash recovery when "
                      "--tc-heuristic-recover is used");
      goto err;
    }
    file_length= my_seek(fd, 0L, MY_SEEK_END, MYF(MY_WME+MY_FAE));
    if (file_length == MY_FILEPOS_ERROR || file_length % tc_log_page_size)
      goto err;
  }

  data= (uchar *)my_mmap(0, (size_t)file_length, PROT_READ|PROT_WRITE,
                        MAP_NOSYNC|MAP_SHARED, fd, 0);
  if (data == MAP_FAILED)
  {
    my_errno=errno;
    goto err;
  }
  inited=2;

  npages=(uint)file_length/tc_log_page_size;
  DBUG_ASSERT(npages >= 3);             // to guarantee non-empty pool
  if (!(pages=(PAGE *)my_malloc(npages*sizeof(PAGE), MYF(MY_WME|MY_ZEROFILL))))
    goto err;
  inited=3;
  for (pg=pages, i=0; i < npages; i++, pg++)
  {
    pg->next=pg+1;
    pg->waiters=0;
    pg->state=POOL;
    pthread_mutex_init(&pg->lock, MY_MUTEX_INIT_FAST);
    pthread_cond_init (&pg->cond, 0);
    pg->start=(my_xid *)(data + i*tc_log_page_size);
    pg->end=(my_xid *)(pg->start + tc_log_page_size);
    pg->size=pg->free=tc_log_page_size/sizeof(my_xid);
  }
  pages[0].size=pages[0].free=
                (tc_log_page_size-TC_LOG_HEADER_SIZE)/sizeof(my_xid);
  pages[0].start=pages[0].end-pages[0].size;
  pages[npages-1].next=0;
  inited=4;

  if (crashed && recover())
      goto err;

  memcpy(data, tc_log_magic, sizeof(tc_log_magic));
  data[sizeof(tc_log_magic)]= (uchar)total_ha_2pc;
  my_msync(fd, data, tc_log_page_size, MS_SYNC);
  inited=5;

  pthread_mutex_init(&LOCK_sync,    MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_active,  MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_pool,    MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_active, 0);
  pthread_cond_init(&COND_pool, 0);

  inited=6;

  syncing= 0;
  active=pages;
  pool=pages+1;
  pool_last=pages+npages-1;

  return 0;

err:
  close();
  return 1;
}

/*
  there is no active page, let's got one from the pool

  two strategies here:
  1. take the first from the pool
  2. if there're waiters - take the one with the most free space

  TODO page merging. try to allocate adjacent page first,
  so that they can be flushed both in one sync
*/
void TC_LOG_MMAP::get_active_from_pool()
{
  PAGE **p, **best_p=0;
  int best_free;

  if (syncing)
    pthread_mutex_lock(&LOCK_pool);

  do
  {
    best_p= p= &pool;
    if ((*p)->waiters == 0) // can the first page be used ?
      break;                // yes - take it.

    best_free=0;            // no - trying second strategy
    for (p=&(*p)->next; *p; p=&(*p)->next)
    {
      if ((*p)->waiters == 0 && (*p)->free > best_free)
      {
        best_free=(*p)->free;
        best_p=p;
      }
    }
  }
  while ((*best_p == 0 || best_free == 0) && overflow());

  active=*best_p;
  if (active->free == active->size) // we've chosen an empty page
  {
    tc_log_cur_pages_used++;
    set_if_bigger(tc_log_max_pages_used, tc_log_cur_pages_used);
  }

  if ((*best_p)->next)              // unlink the page from the pool
    *best_p=(*best_p)->next;
  else
    pool_last=*best_p;

  if (syncing)
    pthread_mutex_unlock(&LOCK_pool);
}

int TC_LOG_MMAP::overflow()
{
  /*
    simple overflow handling - just wait
    TODO perhaps, increase log size ?
    let's check the behaviour of tc_log_page_waits first
  */
  tc_log_page_waits++;
  pthread_cond_wait(&COND_pool, &LOCK_pool);
  return 1; // always return 1
}

/*
  all access to active page is serialized but it's not a problem, as
  we're assuming that fsync() will be a main bottleneck.
  That is, parallelizing writes to log pages we'll decrease number of
  threads waiting for a page, but then all these threads will be waiting
  for a fsync() anyway

  RETURN
         0  - error
  otherwise - "cookie", a number that will be passed as an argument
              to unlog() call. tc_log can define it any way it wants,
              and use for whatever purposes. TC_LOG_MMAP sets it
              to the position in memory where xid was logged to.
*/

int TC_LOG_MMAP::log(THD *thd, my_xid xid)
{
  int err;
  PAGE *p;
  ulong cookie;

  pthread_mutex_lock(&LOCK_active);

  /*
    if active page is full - just wait...
    frankly speaking, active->free here accessed outside of mutex
    protection, but it's safe, because it only means we may miss an
    unlog() for the active page, and we're not waiting for it here -
    unlog() does not signal COND_active.
  */
  while (unlikely(active && active->free == 0))
    pthread_cond_wait(&COND_active, &LOCK_active);

  /* no active page ? take one from the pool */
  if (active == 0)
    get_active_from_pool();

  p=active;
  pthread_mutex_lock(&p->lock);

  /* searching for an empty slot */
  while (*p->ptr)
  {
    p->ptr++;
    DBUG_ASSERT(p->ptr < p->end);               // because p->free > 0
  }

  /* found! store xid there and mark the page dirty */
  cookie= (ulong)((uchar *)p->ptr - data);      // can never be zero
  *p->ptr++= xid;
  p->free--;
  p->state= DIRTY;

  /* to sync or not to sync - this is the question */
  pthread_mutex_unlock(&LOCK_active);
  pthread_mutex_lock(&LOCK_sync);
  pthread_mutex_unlock(&p->lock);

  if (syncing)
  {                                          // somebody's syncing. let's wait
    p->waiters++;
    /*
      note - it must be while (), not do ... while () here
      as p->state may be not DIRTY when we come here
    */
    while (p->state == DIRTY && syncing)
      pthread_cond_wait(&p->cond, &LOCK_sync);
    p->waiters--;
    err= p->state == ERROR;
    if (p->state != DIRTY)                   // page was synced
    {
      if (p->waiters == 0)
        pthread_cond_signal(&COND_pool);     // in case somebody's waiting
      pthread_mutex_unlock(&LOCK_sync);
      goto done;                             // we're done
    }
  }                                          // page was not synced! do it now
  DBUG_ASSERT(active == p && syncing == 0);
  pthread_mutex_lock(&LOCK_active);
  syncing=p;                                 // place is vacant - take it
  active=0;                                  // page is not active anymore
  pthread_cond_broadcast(&COND_active);      // in case somebody's waiting
  pthread_mutex_unlock(&LOCK_active);
  pthread_mutex_unlock(&LOCK_sync);
  err= sync();

done:
  return err ? 0 : cookie;
}

int TC_LOG_MMAP::sync()
{
  int err;

  DBUG_ASSERT(syncing != active);

  /*
    sit down and relax - this can take a while...
    note - no locks are held at this point
  */
  err= my_msync(fd, syncing->start, 1, MS_SYNC);

  /* page is synced. let's move it to the pool */
  pthread_mutex_lock(&LOCK_pool);
  pool_last->next=syncing;
  pool_last=syncing;
  syncing->next=0;
  syncing->state= err ? ERROR : POOL;
  pthread_cond_broadcast(&syncing->cond);    // signal "sync done"
  pthread_cond_signal(&COND_pool);           // in case somebody's waiting
  pthread_mutex_unlock(&LOCK_pool);

  /* marking 'syncing' slot free */
  pthread_mutex_lock(&LOCK_sync);
  syncing=0;
  pthread_cond_signal(&active->cond);        // wake up a new syncer
  pthread_mutex_unlock(&LOCK_sync);
  return err;
}

/*
  erase xid from the page, update page free space counters/pointers.
  cookie points directly to the memory where xid was logged
*/
void TC_LOG_MMAP::unlog(ulong cookie, my_xid xid)
{
  PAGE *p=pages+(cookie/tc_log_page_size);
  my_xid *x=(my_xid *)(data+cookie);

  DBUG_ASSERT(*x == xid);
  DBUG_ASSERT(x >= p->start && x < p->end);
  *x=0;

  pthread_mutex_lock(&p->lock);
  p->free++;
  DBUG_ASSERT(p->free <= p->size);
  set_if_smaller(p->ptr, x);
  if (p->free == p->size)               // the page is completely empty
    statistic_decrement(tc_log_cur_pages_used, &LOCK_status);
  if (p->waiters == 0)                 // the page is in pool and ready to rock
    pthread_cond_signal(&COND_pool);   // ping ... for overflow()
  pthread_mutex_unlock(&p->lock);
}

void TC_LOG_MMAP::close()
{
  uint i;
  switch (inited) {
  case 6:
    pthread_mutex_destroy(&LOCK_sync);
    pthread_mutex_destroy(&LOCK_active);
    pthread_mutex_destroy(&LOCK_pool);
    pthread_cond_destroy(&COND_pool);
  case 5:
    data[0]='A'; // garble the first (signature) byte, in case my_delete fails
  case 4:
    for (i=0; i < npages; i++)
    {
      if (pages[i].ptr == 0)
        break;
      pthread_mutex_destroy(&pages[i].lock);
      pthread_cond_destroy(&pages[i].cond);
    }
  case 3:
    my_free((gptr)pages, MYF(0));
  case 2:
    my_munmap((byte*)data, (size_t)file_length);
  case 1:
    my_close(fd, MYF(0));
  }
  if (inited>=5) // cannot do in the switch because of Windows
    my_delete(logname, MYF(MY_WME));
  inited=0;
}

int TC_LOG_MMAP::recover()
{
  HASH xids;
  PAGE *p=pages, *end_p=pages+npages;

  if (memcmp(data, tc_log_magic, sizeof(tc_log_magic)))
  {
    sql_print_error("Bad magic header in tc log");
    goto err1;
  }

  /*
    the first byte after magic signature is set to current
    number of storage engines on startup
  */
  if (data[sizeof(tc_log_magic)] != total_ha_2pc)
  {
    sql_print_error("Recovery failed! You must enable "
                    "exactly %d storage engines that support "
                    "two-phase commit protocol",
                    data[sizeof(tc_log_magic)]);
    goto err1;
  }

  if (hash_init(&xids, &my_charset_bin, tc_log_page_size/3, 0,
            sizeof(my_xid), 0, 0, MYF(0)))
    goto err1;

  for ( ; p < end_p ; p++)
  {
    for (my_xid *x=p->start; x < p->end; x++)
      if (*x && my_hash_insert(&xids, (byte *)x))
        goto err2; // OOM
  }

  if (ha_recover(&xids))
    goto err2;

  hash_free(&xids);
  bzero(data, (size_t)file_length);
  return 0;

err2:
  hash_free(&xids);
err1:
  sql_print_error("Crash recovery failed. Either correct the problem "
                  "(if it's, for example, out of memory error) and restart, "
                  "or delete tc log and start mysqld with "
                  "--tc-heuristic-recover={commit|rollback}");
  return 1;
}
#endif

TC_LOG *tc_log;
TC_LOG_DUMMY tc_log_dummy;
TC_LOG_MMAP  tc_log_mmap;

/*
  Perform heuristic recovery, if --tc-heuristic-recover was used

  RETURN VALUE
    0	no heuristic recovery was requested
    1   heuristic recovery was performed

  NOTE
    no matter whether heuristic recovery was successful or not
    mysqld must exit. So, return value is the same in both cases.
*/

int TC_LOG::using_heuristic_recover()
{
  if (!tc_heuristic_recover)
    return 0;

  sql_print_information("Heuristic crash recovery mode");
  if (ha_recover(0))
    sql_print_error("Heuristic crash recovery failed");
  sql_print_information("Please restart mysqld without --tc-heuristic-recover");
  return 1;
}

/****** transaction coordinator log for 2pc - binlog() based solution ******/
#define TC_LOG_BINLOG MYSQL_LOG

/*
  TODO keep in-memory list of prepared transactions
  (add to list in log(), remove on unlog())
  and copy it to the new binlog if rotated
  but let's check the behaviour of tc_log_page_waits first!
*/

int TC_LOG_BINLOG::open(const char *opt_name)
{
  LOG_INFO log_info;
  int      error= 1;

  DBUG_ASSERT(total_ha_2pc > 1);
  DBUG_ASSERT(opt_name && opt_name[0]);

  pthread_mutex_init(&LOCK_prep_xids, MY_MUTEX_INIT_FAST);
  pthread_cond_init (&COND_prep_xids, 0);

  if (!my_b_inited(&index_file))
  {
    /* There was a failure to open the index file, can't open the binlog */
    cleanup();
    return 1;
  }

  if (using_heuristic_recover())
  {
    /* generate a new binlog to mask a corrupted one */
    open(opt_name, LOG_BIN, 0, WRITE_CACHE, 0, max_binlog_size, 0);
    cleanup();
    return 1;
  }

  if ((error= find_log_pos(&log_info, NullS, 1)))
  {
    if (error != LOG_INFO_EOF)
      sql_print_error("find_log_pos() failed (error: %d)", error);
    else
      error= 0;
    goto err;
  }

  {
    const char *errmsg;
    IO_CACHE    log;
    File        file;
    Log_event  *ev=0;
    Format_description_log_event fdle(BINLOG_VERSION);
    char        log_name[FN_REFLEN];

    if (! fdle.is_valid())
      goto err;

    do
    {
      strmake(log_name, log_info.log_file_name, sizeof(log_name)-1);
    } while (!(error= find_next_log(&log_info, 1)));

    if (error !=  LOG_INFO_EOF)
    {
      sql_print_error("find_log_pos() failed (error: %d)", error);
      goto err;
    }

    if ((file= open_binlog(&log, log_name, &errmsg)) < 0)
    {
      sql_print_error("%s", errmsg);
      goto err;
    }

    if ((ev= Log_event::read_log_event(&log, 0, &fdle)) &&
        ev->get_type_code() == FORMAT_DESCRIPTION_EVENT &&
        ev->flags & LOG_EVENT_BINLOG_IN_USE_F)
    {
      sql_print_information("Recovering after a crash using %s", opt_name);
      error= recover(&log, (Format_description_log_event *)ev);
    }
    else
      error=0;

    delete ev;
    end_io_cache(&log);
    my_close(file, MYF(MY_WME));

    if (error)
      goto err;
  }

err:
  return error;
}

/* this is called on shutdown, after ha_panic */
void TC_LOG_BINLOG::close()
{
  DBUG_ASSERT(prepared_xids==0);
  pthread_mutex_destroy(&LOCK_prep_xids);
  pthread_cond_destroy (&COND_prep_xids);
}

/*
  TODO group commit

  RETURN
         0  - error
         1  - success
*/
int TC_LOG_BINLOG::log(THD *thd, my_xid xid)
{
  Xid_log_event xle(thd, xid);
  IO_CACHE *trans_log= (IO_CACHE*)thd->ha_data[binlog_hton.slot];
  return !binlog_end_trans(thd, trans_log, &xle);  // invert return value
}

void TC_LOG_BINLOG::unlog(ulong cookie, my_xid xid)
{
  pthread_mutex_lock(&LOCK_prep_xids);
  if (--prepared_xids == 0)
    pthread_cond_signal(&COND_prep_xids);
  pthread_mutex_unlock(&LOCK_prep_xids);
  rotate_and_purge(0);     // as ::write() did not rotate
}

int TC_LOG_BINLOG::recover(IO_CACHE *log, Format_description_log_event *fdle)
{
  Log_event  *ev;
  HASH xids;
  MEM_ROOT mem_root;

  if (! fdle->is_valid() ||
      hash_init(&xids, &my_charset_bin, TC_LOG_PAGE_SIZE/3, 0,
            sizeof(my_xid), 0, 0, MYF(0)))
    goto err1;

  init_alloc_root(&mem_root, TC_LOG_PAGE_SIZE, TC_LOG_PAGE_SIZE);

  fdle->flags&= ~LOG_EVENT_BINLOG_IN_USE_F; // abort on the first error

  while ((ev= Log_event::read_log_event(log,0,fdle)) && ev->is_valid())
  {
    if (ev->get_type_code() == XID_EVENT)
    {
      Xid_log_event *xev=(Xid_log_event *)ev;
      byte *x=(byte *)memdup_root(&mem_root, (char *)& xev->xid,
                                  sizeof(xev->xid));
      if (! x)
        goto err2;
      my_hash_insert(&xids, x);
    }
    delete ev;
  }

  if (ha_recover(&xids))
    goto err2;

  free_root(&mem_root, MYF(0));
  hash_free(&xids);
  return 0;

err2:
  free_root(&mem_root, MYF(0));
  hash_free(&xids);
err1:
  sql_print_error("Crash recovery failed. Either correct the problem "
                  "(if it's, for example, out of memory error) and restart, "
                  "or delete (or rename) binary log and start mysqld with "
                  "--tc-heuristic-recover={commit|rollback}");
  return 1;
}

