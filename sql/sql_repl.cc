/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB & Sasha

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

// Sasha Pachev <sasha@mysql.com> is currently in charge of this file

#include "mysql_priv.h"
#include "sql_repl.h"
#include "sql_acl.h"
#include "log_event.h"
#include "mini_client.h"
#include <thr_alarm.h>
#include <my_dir.h>
#include <assert.h>

extern const char* any_db;

#ifndef DBUG_OFF
int max_binlog_dump_events = 0; // unlimited
bool opt_sporadic_binlog_dump_fail = 0;
static int binlog_dump_count = 0;
#endif

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

static int fake_rotate_event(NET* net, String* packet, char* log_file_name,
			     const char**errmsg)
{
  char header[LOG_EVENT_HEADER_LEN], buf[ROTATE_HEADER_LEN];
  memset(header, 0, 4); // when does not matter
  header[EVENT_TYPE_OFFSET] = ROTATE_EVENT;

  char* p = log_file_name+dirname_length(log_file_name);
  uint ident_len = (uint) strlen(p);
  ulong event_len = ident_len + ROTATE_EVENT_OVERHEAD;
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int2store(header + FLAGS_OFFSET, 0);
  
  // TODO: check what problems this may cause and fix them
  int4store(header + LOG_POS_OFFSET, 0);
  
  packet->append(header, sizeof(header));
  /* We need to split the next statement because of problem with cxx */
  int4store(buf,4); // tell slave to skip magic number
  int4store(buf+4,0);
  packet->append(buf, ROTATE_HEADER_LEN);
  packet->append(p,ident_len);
  if (my_net_write(net, (char*)packet->ptr(), packet->length()))
  {
    *errmsg = "failed on my_net_write()";
    return -1;
  }
  return 0;
}

static int send_file(THD *thd)
{
  NET* net = &thd->net;
  int fd = -1,bytes, error = 1;
  char fname[FN_REFLEN+1];
  const char *errmsg = 0;
  int old_timeout;
  uint packet_len;
  char buf[IO_SIZE];				// It's safe to alloc this
  DBUG_ENTER("send_file");

  // the client might be slow loading the data, give him wait_timeout to do
  // the job
  old_timeout = thd->net.timeout;
  thd->net.timeout = thd->inactive_timeout;

  // we need net_flush here because the client will not know it needs to send
  // us the file name until it has processed the load event entry
  if (net_flush(net) || (packet_len = my_net_read(net)) == packet_error)
  {
    errmsg = "while reading file name";
    goto err;
  }

  // terminate with \0 for fn_format
  *((char*)net->read_pos +  packet_len) = 0;
  fn_format(fname, (char*) net->read_pos + 1, "", "", 4);
  // this is needed to make replicate-ignore-db
  if (!strcmp(fname,"/dev/null"))
    goto end;

  if ((fd = my_open(fname, O_RDONLY, MYF(0))) < 0)
  {
    errmsg = "on open of file";
    goto err;
  }

  while ((bytes = (int) my_read(fd, (byte*) buf, IO_SIZE, MYF(0))) > 0)
  {
    if (my_net_write(net, buf, bytes))
    {
      errmsg = "while writing data to client";
      goto err;
    }
  }

 end:
  if (my_net_write(net, "", 0) || net_flush(net) ||
      (my_net_read(net) == packet_error))
  {
    errmsg = "while negotiating file transfer close";
    goto err;
  }
  error = 0;

 err:
  thd->net.timeout = old_timeout;
  if (fd >= 0)
    (void) my_close(fd, MYF(0));
  if (errmsg)
  {
    sql_print_error("Failed in send_file() %s", errmsg);
    DBUG_PRINT("error", (errmsg));
  }
  DBUG_RETURN(error);
}


File open_binlog(IO_CACHE *log, const char *log_file_name,
		 const char **errmsg)
{
  File file;

  if ((file = my_open(log_file_name, O_RDONLY | O_BINARY, MYF(MY_WME))) < 0 ||
      init_io_cache(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
		    MYF(MY_WME | MY_DONT_CHECK_FILESIZE)))
  {
    *errmsg = "Could not open log file";	// This will not be sent
    goto err;
  }
  if (check_binlog_magic(log,errmsg))
    goto err;
  return file;

err:
  if (file >= 0)
  {
    my_close(file,MYF(0));
    end_io_cache(log);
  }
  return -1;
}


void adjust_linfo_offsets(my_off_t purge_offset)
{
  THD *tmp;

  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);

  while ((tmp=it++))
  {
    LOG_INFO* linfo;
    if ((linfo = tmp->current_linfo))
    {
      pthread_mutex_lock(&linfo->lock);
      /* index file offset can be less that purge offset
	 only if we just started reading the index file. In that case
	 we have nothing to adjust
      */
      if (linfo->index_file_offset < purge_offset)
	linfo->fatal = (linfo->index_file_offset != 0);
      else
	linfo->index_file_offset -= purge_offset;
      pthread_mutex_unlock(&linfo->lock);
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
}


bool log_in_use(const char* log_name)
{
  int log_name_len = strlen(log_name) + 1;
  THD *tmp;
  bool result = 0;

  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);

  while ((tmp=it++))
  {
    LOG_INFO* linfo;
    if ((linfo = tmp->current_linfo))
    {
      pthread_mutex_lock(&linfo->lock);
      result = !memcmp(log_name, linfo->log_file_name, log_name_len);
      pthread_mutex_unlock(&linfo->lock);
      if (result) break;
    }
  }

  pthread_mutex_unlock(&LOCK_thread_count);
  return result;
}


int purge_master_logs(THD* thd, const char* to_log)
{
  char search_file_name[FN_REFLEN];
  const char* errmsg = 0;

  mysql_bin_log.make_log_name(search_file_name, to_log);
  int res = mysql_bin_log.purge_logs(thd, search_file_name);

  switch(res)  {
  case 0: break;
  case LOG_INFO_EOF:	 errmsg = "Target log not found in binlog index"; break;
  case LOG_INFO_IO:	 errmsg = "I/O error reading log index file"; break;
  case LOG_INFO_INVALID: errmsg = "Server configuration does not permit \
binlog purge"; break;
  case LOG_INFO_SEEK:	errmsg = "Failed on fseek()"; break;
  case LOG_INFO_PURGE_NO_ROTATE: errmsg = "Cannot purge unrotatable log";
    break;
  case LOG_INFO_MEM:	errmsg = "Out of memory"; break;
  case LOG_INFO_FATAL:	errmsg = "Fatal error during purge"; break;
  case LOG_INFO_IN_USE: errmsg = "A purgeable log is in use, will not purge";
    break;
  default:		errmsg = "Unknown error during purge"; break;
  }

  if (errmsg)
  {
    send_error(&thd->net, 0, errmsg);
    return 1;
  }
  else
    send_ok(&thd->net);

  return 0;
}


void mysql_binlog_send(THD* thd, char* log_ident, ulong pos, ushort flags)
{
  LOG_INFO linfo;
  char *log_file_name = linfo.log_file_name;
  char search_file_name[FN_REFLEN];
  IO_CACHE log;
  File file = -1;
  String* packet = &thd->packet;
  int error;
  const char *errmsg = "Unknown error";
  NET* net = &thd->net;
#ifndef DBUG_OFF
  int left_events = max_binlog_dump_events;
#endif
  DBUG_ENTER("mysql_binlog_send");
  bzero((char*) &log,sizeof(log));

#ifndef DBUG_OFF
  if (opt_sporadic_binlog_dump_fail && (binlog_dump_count++ % 2))
  {
    errmsg = "Master failed COM_BINLOG_DUMP to test if slave can recover";
    goto err;
  }
#endif


  if (!mysql_bin_log.is_open())
  {
    errmsg = "Binary log is not open";
    goto err;
  }
  if (!server_id_supplied)
  {
    errmsg = "Misconfigured master - server id was not set";
    goto err;
  }

  if (log_ident[0])
    mysql_bin_log.make_log_name(search_file_name, log_ident);
  else
    search_file_name[0] = 0;

  linfo.index_file_offset = 0;
  thd->current_linfo = &linfo;

  if (mysql_bin_log.find_first_log(&linfo, search_file_name))
  {
    errmsg = "Could not find first log";
    goto err;
  }

  if ((file=open_binlog(&log, log_file_name, &errmsg)) < 0)
    goto err;

  if (pos < 4)
  {
    errmsg = "Client requested master to start repliction from \
impossible position";
    goto err;
  }

  my_b_seek(&log, pos);				// Seek will done on next read
  packet->length(0);
  // we need to start a packet with something other than 255
  // to distiquish it from error
  packet->append("\0", 1);

  // if we are at the start of the log
  if (pos == 4)
  {
    // tell the client log name with a fake rotate_event
    if (fake_rotate_event(net, packet, log_file_name, &errmsg))
      goto err;
    packet->length(0);
    packet->append("\0", 1);
  }

  while (!net->error && net->vio != 0 && !thd->killed)
  {
    pthread_mutex_t *log_lock = mysql_bin_log.get_log_lock();

    while (!(error = Log_event::read_log_event(&log, packet, log_lock)))
    {
#ifndef DBUG_OFF
      if (max_binlog_dump_events && !left_events--)
      {
	net_flush(net);
	errmsg = "Debugging binlog dump abort";
	goto err;
      }
#endif
      if (my_net_write(net, (char*)packet->ptr(), packet->length()) )
      {
	errmsg = "Failed on my_net_write()";
	goto err;
      }
      DBUG_PRINT("info", ("log event code %d",
			  (*packet)[LOG_EVENT_OFFSET+1] ));
      if ((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
      {
	if (send_file(thd))
	{
	  errmsg = "failed in send_file()";
	  goto err;
	}
      }
      packet->length(0);
      packet->append("\0",1);
    }
    // TODO: now that we are logging the offset, check to make sure
    // the recorded offset and the actual match
    if (error != LOG_READ_EOF)
    {
      switch(error) {
      case LOG_READ_BOGUS:
	errmsg = "bogus data in log event";
	break;
      case LOG_READ_TOO_LARGE:
	errmsg = "log event entry exceeded max_allowed_packet -\
 increase max_allowed_packet on master";
	break;
      case LOG_READ_IO:
	errmsg = "I/O error reading log event";
	break;
      case LOG_READ_MEM:
	errmsg = "memory allocation failed reading log event";
	break;
      case LOG_READ_TRUNC:
	errmsg = "binlog truncated in the middle of event";
	break;
      default:
	errmsg = "unknown error reading log event on the master";
	break;
      }
      goto err;
    }

    if (!(flags & BINLOG_DUMP_NON_BLOCK) &&
       mysql_bin_log.is_active(log_file_name))
    {
      // block until there is more data in the log
      // unless non-blocking mode requested
      if (net_flush(net))
      {
	errmsg = "failed on net_flush()";
	goto err;
      }

      // we may have missed the update broadcast from the log
      // that has just happened, let's try to catch it if it did
      // if we did not miss anything, we just wait for other threads
      // to signal us
      {
	log.error=0;
	bool read_packet = 0, fatal_error = 0;

#ifndef DBUG_OFF
	if (max_binlog_dump_events && !left_events--)
	{
	  net_flush(net);
	  errmsg = "Debugging binlog dump abort";
	  goto err;
	}
#endif

	// no one will update the log while we are reading
	// now, but we'll be quick and just read one record
	pthread_mutex_lock(log_lock);
	switch (Log_event::read_log_event(&log, packet, (pthread_mutex_t*)0))
	{
	case 0:
          pthread_mutex_unlock(log_lock);
	  read_packet = 1;
	  // we read successfully, so we'll need to send it to the
	  // slave
	  break;
	case LOG_READ_EOF:
	  DBUG_PRINT("wait",("waiting for data in binary log"));
	  // wait_for_update unlocks the log lock - needed to avoid race
	  if (!thd->killed)
	    mysql_bin_log.wait_for_update(thd);
	  else
	    pthread_mutex_unlock(log_lock);
	  DBUG_PRINT("wait",("binary log received update"));
	  break;

	default:
          pthread_mutex_unlock(log_lock);
	  fatal_error = 1;
	  break;
	}
	
	if (read_packet)
	{
	  thd->proc_info = "sending update to slave";
	  if (my_net_write(net, (char*)packet->ptr(), packet->length()) )
	  {
	    errmsg = "Failed on my_net_write()";
	    goto err;
	  }

	  if ((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
	  {
	    if (send_file(thd))
	    {
	      errmsg = "failed in send_file()";
	      goto err;
	    }
	  }
	  packet->length(0);
	  packet->append("\0",1);
	  // no need to net_flush because we will get to flush later when
	  // we hit EOF pretty quick
	}

	if (fatal_error)
	{
	  errmsg = "error reading log entry";
	  goto err;
	}
	log.error=0;
      }
    }
    else
    {
      bool loop_breaker = 0;
      // need this to break out of the for loop from switch
      thd->proc_info = "switching to next log";
      switch (mysql_bin_log.find_next_log(&linfo)) {
      case LOG_INFO_EOF:
	loop_breaker = (flags & BINLOG_DUMP_NON_BLOCK);
	break;
      case 0:
	break;
      default:
	errmsg = "could not find next log";
	goto err;
      }

      if (loop_breaker)
	break;

      end_io_cache(&log);
      (void) my_close(file, MYF(MY_WME));

      // fake Rotate_log event just in case it did not make it to the log
      // otherwise the slave make get confused about the offset
      if ((file=open_binlog(&log, log_file_name, &errmsg)) < 0 ||
	  fake_rotate_event(net, packet, log_file_name, &errmsg))
	goto err;

      packet->length(0);
      packet->append("\0",1);
    }
  }

  end_io_cache(&log);
  (void)my_close(file, MYF(MY_WME));

  send_eof(&thd->net);
  thd->proc_info = "waiting to finalize termination";
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;

 err:
  thd->proc_info = "waiting to finalize termination";
  end_io_cache(&log);
  pthread_mutex_lock(&LOCK_thread_count);
  // exclude  iteration through thread list
  // this is needed for purge_logs() - it will iterate through
  // thread list and update thd->current_linfo->index_file_offset
  // this mutex will make sure that it never tried to update our linfo
  // after we return from this stack frame
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  if (file >= 0)
    (void) my_close(file, MYF(MY_WME));
  send_error(&thd->net, my_errno, errmsg);
  DBUG_VOID_RETURN;
}

int start_slave(THD* thd , MASTER_INFO* mi,  bool net_report)
{
  int slave_errno = 0;
  if (!thd) thd = current_thd;
  NET* net = &thd->net;
  int thread_mask;
  
  if (check_access(thd, PROCESS_ACL, any_db))
    return 1;
  lock_slave_threads(mi);  // this allows us to cleanly read slave_running
  init_thread_mask(&thread_mask,mi,1 /* inverse */);
  if (thread_mask)
  {
    if (server_id_supplied && (!mi->inited || (mi->inited && *mi->host)))
      slave_errno = start_slave_threads(0 /*no mutex */,
					1 /* wait for start */,
					mi,
					master_info_file,relay_log_info_file,
					thread_mask);
    else
      slave_errno = ER_BAD_SLAVE;
  }
  else
    slave_errno = ER_SLAVE_MUST_STOP;
  
  unlock_slave_threads(mi);
  
  if (slave_errno)
  {
    if (net_report)
      send_error(net, slave_errno);
    return 1;
  }
  else if (net_report)
    send_ok(net);

  return 0;
}

int stop_slave(THD* thd, MASTER_INFO* mi, bool net_report )
{
  int slave_errno = 0;
  if (!thd) thd = current_thd;
  NET* net = &thd->net;

  if (check_access(thd, PROCESS_ACL, any_db))
    return 1;
  thd->proc_info = "Killing slave";
  int thread_mask;
  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /* not inverse*/);
  slave_errno = (thread_mask) ?
    terminate_slave_threads(mi,thread_mask,
			    1 /*skip lock */) :    ER_SLAVE_NOT_RUNNING;
  unlock_slave_threads(mi);
  thd->proc_info = 0;

  if (slave_errno)
  {
    if (net_report)
      send_error(net, slave_errno);
    return 1;
  }
  else if (net_report)
    send_ok(net);

  return 0;
}

int reset_slave(MASTER_INFO* mi)
{
  MY_STAT stat_area;
  char fname[FN_REFLEN];
  int restart_thread_mask = 0,error=0;
  const char* errmsg=0;
  
  lock_slave_threads(mi);
  init_thread_mask(&restart_thread_mask,mi,0 /* not inverse */);
  if ((error=terminate_slave_threads(mi,restart_thread_mask,1 /*skip lock*/))
      || (error=purge_relay_logs(&mi->rli,1 /*just reset*/,&errmsg)))
    goto err;
  
  end_master_info(mi);
  fn_format(fname, master_info_file, mysql_data_home, "", 4+32);
  if (my_stat(fname, &stat_area, MYF(0)) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }
  fn_format(fname, relay_log_info_file, mysql_data_home, "", 4+32);
  if (my_stat(fname, &stat_area, MYF(0)) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }
  if (restart_thread_mask)
      error=start_slave_threads(0 /* mutex not needed*/,
			  1 /* wait for start*/,
			  mi,master_info_file,relay_log_info_file,
				restart_thread_mask);
  // TODO: fix error messages so they get to the client
err:
  unlock_slave_threads(mi);
  return error;
}

void kill_zombie_dump_threads(uint32 slave_server_id)
{
  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);
  THD *tmp;

  while ((tmp=it++))
  {
    if (tmp->command == COM_BINLOG_DUMP &&
       tmp->server_id == slave_server_id)
    {
      /*
	Here we do not call kill_one_thread() as
	it will be slow because it will iterate through the list
	again. Plus it double-locks LOCK_tread_count, which
	make safe_mutex complain and abort.
	We just to do kill the thread ourselves.
      */
      tmp->awake(1/*prepare to die*/);
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
}


int change_master(THD* thd, MASTER_INFO* mi)
{
  int error=0,restart_thread_mask;
  const char* errmsg=0;
  
  // kill slave thread
  lock_slave_threads(mi);
  init_thread_mask(&restart_thread_mask,mi,0 /*not inverse*/);
  if (restart_thread_mask &&
      (error=terminate_slave_threads(mi,
				     restart_thread_mask,
				     1 /*skip lock*/)))
  {
    send_error(&thd->net,error);
    unlock_slave_threads(mi);
    return 1;
  }
  thd->proc_info = "changing master";
  LEX_MASTER_INFO* lex_mi = &thd->lex.mi;
  // TODO: see if needs re-write
  if (init_master_info(mi,master_info_file,relay_log_info_file))
  {
    send_error(&thd->net, 0, "Could not initialize master info");
    unlock_slave_threads(mi);
    return 1;
  }

  pthread_mutex_lock(&mi->data_lock);
  if ((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name && !lex_mi->pos)
  {
    // if we change host or port, we must reset the postion
    mi->master_log_name[0] = 0;
    mi->master_log_pos = 4;				// skip magic number
    mi->rli.pending = 0;
  }

  if (lex_mi->log_file_name)
    strmake(mi->master_log_name, lex_mi->log_file_name,
	    sizeof(mi->master_log_name));
  if (lex_mi->pos)
  {
    mi->master_log_pos = lex_mi->pos;
    mi->rli.pending = 0;
  }

  if (lex_mi->host)
    strmake(mi->host, lex_mi->host, sizeof(mi->host));
  if (lex_mi->user)
    strmake(mi->user, lex_mi->user, sizeof(mi->user));
  if (lex_mi->password)
    strmake(mi->password, lex_mi->password, sizeof(mi->password));
  if (lex_mi->port)
    mi->port = lex_mi->port;
  if (lex_mi->connect_retry)
    mi->connect_retry = lex_mi->connect_retry;

  flush_master_info(mi);
  pthread_mutex_unlock(&mi->data_lock);
  thd->proc_info="purging old relay logs";
  if (purge_relay_logs(&mi->rli,0 /* not only reset, but also reinit*/,
		       &errmsg))
  {
    send_error(&thd->net, 0, "Failed purging old relay logs");
    unlock_slave_threads(mi);
    return 1;
  }
  pthread_mutex_lock(&mi->rli.data_lock);
  mi->rli.master_log_pos = mi->master_log_pos;
  strnmov(mi->rli.master_log_name,mi->master_log_name,
	  sizeof(mi->rli.master_log_name));
  if (!mi->rli.master_log_name[0]) // uninitialized case
    mi->rli.master_log_pos=0;
  pthread_cond_broadcast(&mi->rli.data_cond);
  pthread_mutex_unlock(&mi->rli.data_lock);

  thd->proc_info = "starting slave";
  if (restart_thread_mask) 
      error=start_slave_threads(0 /* mutex not needed*/,
			        1 /* wait for start*/,
			        mi,master_info_file,relay_log_info_file,
				restart_thread_mask);
err:  
  unlock_slave_threads(mi);
  thd->proc_info = 0;
  if (error)
    send_error(&thd->net,error);
  else
    send_ok(&thd->net);
  return 0;
}

int reset_master(THD* thd)
{
  if (!mysql_bin_log.is_open())
  {
    my_error(ER_FLUSH_MASTER_BINLOG_CLOSED,  MYF(ME_BELL+ME_WAITTANG));
    return 1;
  }
  return mysql_bin_log.reset_logs(thd);
}

int cmp_master_pos(const char* log_file_name1, ulonglong log_pos1,
		   const char* log_file_name2, ulonglong log_pos2)
{
  int res;
  if ((res = strcmp(log_file_name1, log_file_name2)))
    return res;
  if (log_pos1 > log_pos2)
    return 1;
  else if (log_pos1 == log_pos2)
    return 0;
  return -1;
}

int show_binlog_events(THD* thd)
{
  DBUG_ENTER("show_binlog_events");
  List<Item> field_list;
  const char* errmsg = 0;
  IO_CACHE log;
  File file = -1;

  Log_event::init_show_field_list(&field_list);
  if (send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);

  if (mysql_bin_log.is_open())
  {
    LOG_INFO linfo;
    char search_file_name[FN_REFLEN];
    LEX_MASTER_INFO* lex_mi = &thd->lex.mi;
    uint event_count, limit_start, limit_end;
    const char* log_file_name = lex_mi->log_file_name;
    Log_event* ev;
    my_off_t pos = lex_mi->pos;

    limit_start = thd->lex.select->offset_limit;
    limit_end = thd->lex.select->select_limit + limit_start;

    if (log_file_name)
      mysql_bin_log.make_log_name(search_file_name, log_file_name);
    else
      search_file_name[0] = 0;

    linfo.index_file_offset = 0;
    thd->current_linfo = &linfo;

    if (mysql_bin_log.find_first_log(&linfo, search_file_name))
    {
      errmsg = "Could not find target log";
      goto err;
    }

    if ((file=open_binlog(&log, linfo.log_file_name, &errmsg)) < 0)
      goto err;

    if (pos < 4)
    {
      errmsg = "Invalid log position";
      goto err;
    }

    pthread_mutex_lock(mysql_bin_log.get_log_lock());
    my_b_seek(&log, pos);

    for (event_count = 0;
	 (ev = Log_event::read_log_event(&log,(pthread_mutex_t*)0,0)); )
    {
      if (event_count >= limit_start &&
	  ev->net_send(thd, linfo.log_file_name, pos))
      {
	errmsg = "Net error";
	delete ev;
	pthread_mutex_unlock(mysql_bin_log.get_log_lock());
	goto err;
      }

      pos = my_b_tell(&log);
      delete ev;

      if (++event_count >= limit_end)
	break;
    }

    if (event_count < limit_end && log.error)
    {
      errmsg = "Wrong offset or I/O error";
      pthread_mutex_unlock(mysql_bin_log.get_log_lock());
      goto err;
    }

    pthread_mutex_unlock(mysql_bin_log.get_log_lock());
  }

err:
  if (file >= 0)
  {
    end_io_cache(&log);
    (void) my_close(file, MYF(MY_WME));
  }

  if (errmsg)
  {
    net_printf(&thd->net, ER_ERROR_WHEN_EXECUTING_COMMAND,
	       "SHOW BINLOG EVENTS", errmsg);
    DBUG_RETURN(1);
  }

  send_eof(&thd->net);
  DBUG_RETURN(0);
}

int show_binlog_info(THD* thd)
{
  DBUG_ENTER("show_binlog_info");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("File", FN_REFLEN));
  field_list.push_back(new Item_empty_string("Position",20));
  field_list.push_back(new Item_empty_string("Binlog_do_db",20));
  field_list.push_back(new Item_empty_string("Binlog_ignore_db",20));

  if (send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);
  String* packet = &thd->packet;
  packet->length(0);

  if (mysql_bin_log.is_open())
  {
    LOG_INFO li;
    mysql_bin_log.get_current_log(&li);
    int dir_len = dirname_length(li.log_file_name);
    net_store_data(packet, li.log_file_name + dir_len);
    net_store_data(packet, (longlong)li.pos);
    net_store_data(packet, &binlog_do_db);
    net_store_data(packet, &binlog_ignore_db);
  }
  else
  {
    net_store_null(packet);
    net_store_null(packet);
    net_store_null(packet);
    net_store_null(packet);
  }

  if (my_net_write(&thd->net, (char*)thd->packet.ptr(), packet->length()))
    DBUG_RETURN(-1);

  send_eof(&thd->net);
  DBUG_RETURN(0);
}


int show_binlogs(THD* thd)
{
  const char* errmsg = 0;
  File index_file;
  char fname[FN_REFLEN];
  NET* net = &thd->net;
  List<Item> field_list;
  String* packet = &thd->packet;
  IO_CACHE io_cache;
  uint length;

  if (!mysql_bin_log.is_open())
  {
    errmsg = "binlog is not open";
    goto err;
  }

  field_list.push_back(new Item_empty_string("Log_name", 128));
  if (send_fields(thd, field_list, 1))
  {
    sql_print_error("Failed in send_fields");
    return 1;
  }

  mysql_bin_log.lock_index();
  index_file = mysql_bin_log.get_index_file();
  if (index_file < 0)
  {
    errmsg = "Uninitialized index file pointer";
    goto err2;
  }
  if (init_io_cache(&io_cache, index_file, IO_SIZE, READ_CACHE, 0, 0,
		   MYF(MY_WME)))
  {
    errmsg = "Failed on init_io_cache()";
    goto err2;
  }
  while ((length=my_b_gets(&io_cache, fname, sizeof(fname))))
  {
    fname[--length]=0;
    int dir_len = dirname_length(fname);
    packet->length(0);
    net_store_data(packet, fname + dir_len, length-dir_len);
    if (my_net_write(net, (char*) packet->ptr(), packet->length()))
    {
      sql_print_error("Failed in my_net_write");
      end_io_cache(&io_cache);
      mysql_bin_log.unlock_index();
      return 1;
    }
  }

  mysql_bin_log.unlock_index();
  end_io_cache(&io_cache);
  send_eof(net);
  return 0;

err2:
  mysql_bin_log.unlock_index();
  end_io_cache(&io_cache);
err:
  send_error(net, 0, errmsg);
  return 1;
}

int log_loaded_block(IO_CACHE* file)
{
  LOAD_FILE_INFO* lf_info;
  uint block_len ;

  /* file->request_pos contains position where we started last read */
  char* buffer = (char*) file->request_pos;
  if (!(block_len = file->read_end - buffer))
    return 0;
  lf_info = (LOAD_FILE_INFO*)file->arg;
  if (lf_info->last_pos_in_file != HA_POS_ERROR &&
      lf_info->last_pos_in_file >= file->pos_in_file)
    return 0;
  lf_info->last_pos_in_file = file->pos_in_file;
  if (lf_info->wrote_create_file)
  {
    Append_block_log_event a(lf_info->thd, buffer, block_len);
    mysql_bin_log.write(&a);
  }
  else
  {
    Create_file_log_event c(lf_info->thd,lf_info->ex,lf_info->db,
			    lf_info->table_name, *lf_info->fields,
			    lf_info->handle_dup, buffer,
			    block_len);
    mysql_bin_log.write(&c);
    lf_info->wrote_create_file = 1;
    DBUG_SYNC_POINT("debug_lock.created_file_event",10);
  }
  return 0;
}


