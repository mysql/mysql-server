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

// Sasha Pachev <sasha@mysql.com> is currently in charge of this file
// Do not mess with it without his permission!

#include "mysql_priv.h"
#include "sql_repl.h"
#include "sql_acl.h"
#include "log_event.h"
#include "mini_client.h"
#include <thr_alarm.h>
#include <my_dir.h>

#define SLAVE_LIST_CHUNK 128

extern const char* any_db;
extern pthread_handler_decl(handle_slave,arg);

HASH slave_list;

static uint32* slave_list_key(SLAVE_INFO* si, uint* len,
			   my_bool not_used __attribute__((unused)))
{
  *len = 4;
  return &si->server_id;
}

static void slave_info_free(void *s)
{
  my_free((byte*)s, MYF(MY_WME));
}

void init_slave_list()
{
  hash_init(&slave_list, SLAVE_LIST_CHUNK, 0, 0,
	    (hash_get_key) slave_list_key, slave_info_free, 0);
  pthread_mutex_init(&LOCK_slave_list, MY_MUTEX_INIT_FAST);
}

void end_slave_list()
{
  pthread_mutex_lock(&LOCK_slave_list);
  hash_free(&slave_list);
  pthread_mutex_unlock(&LOCK_slave_list);
  pthread_mutex_destroy(&LOCK_slave_list);
}

static int fake_rotate_event(NET* net, String* packet, char* log_file_name,
			     const char**errmsg)
{
  char header[LOG_EVENT_HEADER_LEN];
  memset(header, 0, 4); // when does not matter
  header[EVENT_TYPE_OFFSET] = ROTATE_EVENT;
  char* p = strrchr(log_file_name, FN_LIBCHAR);
  // find the last slash
  if(p)
    p++;
  else
    p = log_file_name;

  uint ident_len = (uint) strlen(p);
  ulong event_len = ident_len + sizeof(header);
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int2store(header + FLAGS_OFFSET, 0);
  int4store(header + LOG_SEQ_OFFSET, 0);
  packet->append(header, sizeof(header));
  packet->append(p,ident_len);
  if(my_net_write(net, (char*)packet->ptr(), packet->length()))
    {
      *errmsg = "failed on my_net_write()";
      return -1;
    }
  return 0;
}

int register_slave(THD* thd, uchar* packet, uint packet_length)
{
  uint len;
  SLAVE_INFO* si, *old_si;
  int res = 1;
  uchar* p = packet, *p_end = packet + packet_length;

  if(check_access(thd, FILE_ACL, any_db))
    return 1;
  
  if(!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
    goto err;

  si->server_id = uint4korr(p);
  p += 4;
  len = (uint)*p++;
  if(p + len > p_end || len > sizeof(si->host) - 1)
    goto err;
  memcpy(si->host, p, len);
  si->host[len] = 0;
  p += len;
  len = *p++;
  if(p + len > p_end || len > sizeof(si->user) - 1)
    goto err;
  memcpy(si->user, p, len);
  si->user[len] = 0;
  p += len;
  len = *p++;
  if(p + len > p_end || len > sizeof(si->password) - 1)
    goto err;
  memcpy(si->password, p, len);
  si->password[len] = 0;
  p += len;
  si->port = uint2korr(p);
  pthread_mutex_lock(&LOCK_slave_list);

  if((old_si = (SLAVE_INFO*)hash_search(&slave_list,
					(byte*)&si->server_id, 4)))
     hash_delete(&slave_list, (byte*)old_si);
    
  res = hash_insert(&slave_list, (byte*)si);
  pthread_mutex_unlock(&LOCK_slave_list);
  return res;
err:
  if(si)
    my_free((byte*)si, MYF(MY_WME));
  return res;
}


static int send_file(THD *thd)
{
  NET* net = &thd->net;
  int fd = -1,bytes, error = 1;
  char fname[FN_REFLEN+1];
  char *buf;
  const char *errmsg = 0;
  int old_timeout;
  uint packet_len;
  DBUG_ENTER("send_file");

  // the client might be slow loading the data, give him wait_timeout to do
  // the job
  old_timeout = thd->net.timeout;
  thd->net.timeout = thd->inactive_timeout;

  // spare the stack
  if(!(buf = alloc_root(&thd->mem_root,IO_SIZE)))
    {
      errmsg = "Out of memory";
      goto err;
    }

  // we need net_flush here because the client will not know it needs to send
  // us the file name until it has processed the load event entry
  if (net_flush(net) || (packet_len = my_net_read(net)) == packet_error)
  {
    errmsg = "Failed reading file name";
    goto err;
  }

  *((char*)net->read_pos +  packet_len) = 0; // terminate with \0
   //for fn_format
  fn_format(fname, (char*)net->read_pos + 1, "", "", 4);
  // this is needed to make replicate-ignore-db
  if (!strcmp(fname,"/dev/null"))
    goto end;

  if ((fd = my_open(fname, O_RDONLY, MYF(MY_WME))) < 0)
  {
    errmsg = "Failed on my_open()";
    goto err;
  }

  while ((bytes = (int) my_read(fd, (byte*) buf, IO_SIZE,
				MYF(MY_WME))) > 0)
  {
    if (my_net_write(net, buf, bytes))
    {
      errmsg = "Failed on my_net_write()";
      goto err;
    }
  }

 end:
  if (my_net_write(net, "", 0) || net_flush(net) ||
      (my_net_read(net) == packet_error))
  {
    errmsg = "failed negotiating file transfer close";
    goto err;
  }
  error = 0;

 err:
  thd->net.timeout = old_timeout;
  if(fd >= 0)
    (void) my_close(fd, MYF(MY_WME));
  if (errmsg)
  {
    sql_print_error("failed in send_file() : %s", errmsg);
    DBUG_PRINT("error", (errmsg));
  }
  DBUG_RETURN(error);
}


File open_binlog(IO_CACHE *log, const char *log_file_name,
		     const char **errmsg)
{
  File file;
  char magic[4];
  if ((file = my_open(log_file_name, O_RDONLY | O_BINARY, MYF(MY_WME))) < 0 ||
      init_io_cache(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
		    MYF(MY_WME)))
  {
    *errmsg = "Could not open log file";		// This will not be sent
    goto err;
  }
  
  if (my_b_read(log, (byte*) magic, sizeof(magic)))
  {
    *errmsg = "I/O error reading binlog magic number";
    goto err;
  }
  if (memcmp(magic, BINLOG_MAGIC, 4))
  {
    *errmsg = "Binlog has bad magic number, fire your magician";
    goto err;
  }
  return file;

err:
  if (file > 0)
    my_close(file,MYF(0));
  end_io_cache(log);
  return -1;
}


void adjust_linfo_offsets(my_off_t purge_offset)
{
  THD *tmp;
  
  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);
  
  while((tmp=it++))
    {
      LOG_INFO* linfo;
	if((linfo = tmp->current_linfo))
	{
	  pthread_mutex_lock(&linfo->lock);
	  // no big deal if we just started reading the log
	  // nothing to adjust
	  if(linfo->index_file_offset < purge_offset)
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
  
  while((tmp=it++))
    {
      LOG_INFO* linfo;
      if((linfo = tmp->current_linfo))
	{
	  pthread_mutex_lock(&linfo->lock);
	  result = !memcmp(log_name, linfo->log_file_name, log_name_len);
	  pthread_mutex_unlock(&linfo->lock);
	  if(result) break;
	}
   }

  pthread_mutex_unlock(&LOCK_thread_count);
  return result;
}


int purge_master_logs(THD* thd, const char* to_log)
{
  char search_file_name[FN_REFLEN];
  mysql_bin_log.make_log_name(search_file_name, to_log);
  int res = mysql_bin_log.purge_logs(thd, search_file_name);
  const char* errmsg = 0;
  switch(res)
    {
    case 0: break;
    case LOG_INFO_EOF: errmsg = "Target log not found in binlog index"; break;
    case LOG_INFO_IO: errmsg = "I/O error reading log index file"; break;
    case LOG_INFO_INVALID: errmsg = "Server configuration does not permit \
binlog purge"; break;
    case LOG_INFO_SEEK: errmsg = "Failed on fseek()"; break;
    case LOG_INFO_PURGE_NO_ROTATE: errmsg = "Cannot purge unrotatable log";
      break;
    case LOG_INFO_MEM: errmsg = "Out of memory"; break;
    case LOG_INFO_FATAL: errmsg = "Fatal error during purge"; break;
    case LOG_INFO_IN_USE: errmsg = "A purgeable log is in use, will not purge";
      break;
    default:
      errmsg = "Unknown error during purge"; break;
    }
  
  if(errmsg)
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
  DBUG_ENTER("mysql_binlog_send");

  bzero((char*) &log,sizeof(log));

  if(!mysql_bin_log.is_open())
  {
    errmsg = "Binary log is not open";
    goto err;
  }
  if(!server_id_supplied)
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
  packet->append("\0", 1);
  // we need to start a packet with something other than 255
  // to distiquish it from error

  // tell the client log name with a fake rotate_event
  // if we are at the start of the log
  if(pos == 4) 
  {
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
      if (my_net_write(net, (char*)packet->ptr(), packet->length()) )
      {
	errmsg = "Failed on my_net_write()";
	goto err;
      }
      DBUG_PRINT("info", ("log event code %d",
			  (*packet)[LOG_EVENT_OFFSET+1] ));
      if ((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
      {
	if(send_file(thd))
	{
	  errmsg = "failed in send_file()";
	  goto err;
	}
      }
      packet->length(0);
      packet->append("\0",1);
    }
    
    if (error != LOG_READ_EOF)
    {
      switch(error)
      {
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

    if(!(flags & BINLOG_DUMP_NON_BLOCK) &&
       mysql_bin_log.is_active(log_file_name))
      // block until there is more data in the log
      // unless non-blocking mode requested
    {
      if(net_flush(net))
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

	// tell the kill thread how to wake us up
	pthread_mutex_lock(&thd->mysys_var->mutex);
	thd->mysys_var->current_mutex = log_lock;
	thd->mysys_var->current_cond = &COND_binlog_update;
	const char* proc_info = thd->proc_info;
	thd->proc_info = "Slave connection: waiting for binlog update";
	pthread_mutex_unlock(&thd->mysys_var->mutex);

	bool read_packet = 0, fatal_error = 0;

	// no one will update the log while we are reading
	// now, but we'll be quick and just read one record
	pthread_mutex_lock(log_lock);
	switch (Log_event::read_log_event(&log, packet, (pthread_mutex_t*) 0))
	{
	case 0:
	  read_packet = 1;
	  // we read successfully, so we'll need to send it to the
	  // slave
	  break;
	case LOG_READ_EOF:
	  DBUG_PRINT("wait",("waiting for data on binary log"));
	  pthread_cond_wait(&COND_binlog_update, log_lock);
	  break;

	default:
	  fatal_error = 1;
	  break;
	}
	pthread_mutex_unlock(log_lock);

	pthread_mutex_lock(&thd->mysys_var->mutex);
	thd->mysys_var->current_mutex= 0;
	thd->mysys_var->current_cond= 0;
	thd->proc_info= proc_info;
	pthread_mutex_unlock(&thd->mysys_var->mutex);

	if(read_packet)
	{
	  thd->proc_info = "sending update to slave";
	  if(my_net_write(net, (char*)packet->ptr(), packet->length()) )
	  {
	    errmsg = "Failed on my_net_write()";
	    goto err;
	  }

	  if((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
	  {
	    if(send_file(thd))
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

	if(fatal_error)
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
      switch(mysql_bin_log.find_next_log(&linfo))
      {
      case LOG_INFO_EOF:
	loop_breaker = (flags & BINLOG_DUMP_NON_BLOCK);
	break;
      case 0:
	break;
      default:
	errmsg = "could not find next log";
	goto err;
      }

      if(loop_breaker)
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

int start_slave(THD* thd , bool net_report)
{
  if(!thd) thd = current_thd;
  NET* net = &thd->net;
  int slave_errno = 0;
  if (check_access(thd, PROCESS_ACL, any_db))
    return 1;
  pthread_mutex_lock(&LOCK_slave);
  if(!slave_running)
    {
      if(init_master_info(&glob_mi))
	slave_errno = ER_MASTER_INFO;
      else if(server_id_supplied && *glob_mi.host)
	{
	  pthread_t hThread;
	  if(pthread_create(&hThread, &connection_attrib, handle_slave, 0))
	    {
	      slave_errno = ER_SLAVE_THREAD;
	    }
	  while(!slave_running) // slave might already be running by now
	   pthread_cond_wait(&COND_slave_start, &LOCK_slave);
	}
      else
	slave_errno = ER_BAD_SLAVE;
    }
  else
    slave_errno = ER_SLAVE_MUST_STOP;

  pthread_mutex_unlock(&LOCK_slave);
  if(slave_errno)
    {
      if(net_report) send_error(net, slave_errno);
      return 1;
    }
  else if(net_report)
    send_ok(net);

  return 0;
}

int stop_slave(THD* thd, bool net_report )
{
  if(!thd) thd = current_thd;
  NET* net = &thd->net;
  int slave_errno = 0;
  
  if (check_access(thd, PROCESS_ACL, any_db))
    return 1;

  pthread_mutex_lock(&LOCK_slave);
  if (slave_running)
  {
    abort_slave = 1;
    thr_alarm_kill(slave_real_id);
#ifdef SIGNAL_WITH_VIO_CLOSE
    slave_thd->close_active_vio();
#endif    
    // do not abort the slave in the middle of a query, so we do not set
    // thd->killed for the slave thread
    thd->proc_info = "waiting for slave to die";
    while(slave_running) 
      pthread_cond_wait(&COND_slave_stopped, &LOCK_slave);
  }
  else
    slave_errno = ER_SLAVE_NOT_RUNNING;

  pthread_mutex_unlock(&LOCK_slave);
  thd->proc_info = 0;

  if(slave_errno)
    {
     if(net_report) send_error(net, slave_errno);
     return 1;
    }
  else if(net_report)
    send_ok(net);

  return 0;
}

void reset_slave()
{
  MY_STAT stat_area;
  char fname[FN_REFLEN];
  bool slave_was_running ;

  pthread_mutex_lock(&LOCK_slave);
  if((slave_was_running = slave_running))
    {
      pthread_mutex_unlock(&LOCK_slave);
      stop_slave(0,0);
    }
  else
    pthread_mutex_unlock(&LOCK_slave);
  
  end_master_info(&glob_mi);
  fn_format(fname, master_info_file, mysql_data_home, "", 4+32);
  if(my_stat(fname, &stat_area, MYF(0)))
    if(my_delete(fname, MYF(MY_WME)))
        return;
  if(slave_was_running)
    start_slave(0,0);
}

void kill_zombie_dump_threads(uint32 slave_server_id)
{
  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);
  THD *tmp;

  while((tmp=it++))
    {
      if(tmp->command == COM_BINLOG_DUMP &&
	 tmp->server_id == slave_server_id)
	{
	  // here we do not call kill_one_thread()
	  // it will be slow because it will iterate through the list
	  // again. Plus it double-locks LOCK_thread_count, which
	  // make safe_mutex complain and abort
	  // so we just to our own thread murder
	  
	  thr_alarm_kill(tmp->real_id);
	  tmp->killed = 1;
	  pthread_mutex_lock(&tmp->mysys_var->mutex);
          tmp->mysys_var->abort = 1;
	  if(tmp->mysys_var->current_mutex)
	    {
	      pthread_mutex_lock(tmp->mysys_var->current_mutex);
	      pthread_cond_broadcast(tmp->mysys_var->current_cond);
	      pthread_mutex_unlock(tmp->mysys_var->current_mutex);
	    }
	  pthread_mutex_unlock(&tmp->mysys_var->mutex);
	}
   }
  
  pthread_mutex_unlock(&LOCK_thread_count);
}

int change_master(THD* thd)
{
  bool slave_was_running;
  // kill slave thread
  pthread_mutex_lock(&LOCK_slave);
  if((slave_was_running = slave_running))
    {
      abort_slave = 1;
      thr_alarm_kill(slave_real_id);
      thd->proc_info = "waiting for slave to die";
      while(slave_running)
       pthread_cond_wait(&COND_slave_stopped, &LOCK_slave); // wait until done
    }
  pthread_mutex_unlock(&LOCK_slave);
  thd->proc_info = "changing master";
  LEX_MASTER_INFO* lex_mi = &thd->lex.mi;

  if(init_master_info(&glob_mi))
    {
      send_error(&thd->net, 0, "Could not initialize master info");
      return 1;
    }
  
  pthread_mutex_lock(&glob_mi.lock);
  if((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name && !lex_mi->pos)
    {
      // if we change host or port, we must reset the postion
      glob_mi.log_file_name[0] = 0;
      glob_mi.pos = 4; // skip magic number
      glob_mi.pending = 0;
    }

  if(lex_mi->log_file_name)
    strmake(glob_mi.log_file_name, lex_mi->log_file_name,
	    sizeof(glob_mi.log_file_name));
  if(lex_mi->pos)
  {
    glob_mi.pos = lex_mi->pos;
    glob_mi.pending = 0;
  }
  
  if(lex_mi->host)
    {
      strmake(glob_mi.host, lex_mi->host, sizeof(glob_mi.host));
    }
  if(lex_mi->user)
    strmake(glob_mi.user, lex_mi->user, sizeof(glob_mi.user));
  if(lex_mi->password)
    strmake(glob_mi.password, lex_mi->password, sizeof(glob_mi.password));
  if(lex_mi->port)
    glob_mi.port = lex_mi->port;
  if(lex_mi->connect_retry)
    glob_mi.connect_retry = lex_mi->connect_retry;

  flush_master_info(&glob_mi);
  pthread_mutex_unlock(&glob_mi.lock);
  thd->proc_info = "starting slave";
  if(slave_was_running)
    start_slave(0,0);
  thd->proc_info = 0;

  send_ok(&thd->net);
  return 0;
}

void reset_master()
{
  if(!mysql_bin_log.is_open())
  {
    my_error(ER_FLUSH_MASTER_BINLOG_CLOSED,  MYF(ME_BELL+ME_WAITTANG));
    return;
  }

  LOG_INFO linfo;
  if (mysql_bin_log.find_first_log(&linfo, ""))
    return;

  for(;;)
  {
    my_delete(linfo.log_file_name, MYF(MY_WME));
    if (mysql_bin_log.find_next_log(&linfo))
      break;
  }
  mysql_bin_log.close(1); // exiting close
  my_delete(mysql_bin_log.get_index_fname(), MYF(MY_WME));
  mysql_bin_log.open(opt_bin_logname,LOG_BIN);

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
    ulong pos = (ulong) lex_mi->pos;
      
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
	(ev = Log_event::read_log_event(&log, 0));)
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
    net_printf(&thd->net, ER_SHOW_BINLOG_EVENTS, errmsg);
    DBUG_RETURN(1);
  }

  send_eof(&thd->net);
  DBUG_RETURN(0);
}


int show_slave_hosts(THD* thd)
{
  DBUG_ENTER("show_slave_hosts");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Server_id", 20));
  field_list.push_back(new Item_empty_string("Host", 20));
  if(opt_show_slave_auth_info)
  {
    field_list.push_back(new Item_empty_string("User",20));
    field_list.push_back(new Item_empty_string("Password",20));
  }
  field_list.push_back(new Item_empty_string("Port",20));

  if(send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);
  String* packet = &thd->packet;
  uint i;
  NET* net = &thd->net;

  pthread_mutex_lock(&LOCK_slave_list);
  
  for(i = 0; i < slave_list.records; ++i)
  {
    SLAVE_INFO* si = (SLAVE_INFO*)hash_element(&slave_list, i);
    packet->length(0);
    net_store_data(packet, si->server_id);
    net_store_data(packet, si->host);
    if(opt_show_slave_auth_info)
    {
      net_store_data(packet, si->user);
      net_store_data(packet, si->password);
    }
    net_store_data(packet, (uint)si->port);
    if(my_net_write(net, (char*)packet->ptr(), packet->length()))
    {
      pthread_mutex_unlock(&LOCK_slave_list);
      DBUG_RETURN(-1);
    }
  }
  pthread_mutex_unlock(&LOCK_slave_list);
  send_eof(net);
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

  if(send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);
  String* packet = &thd->packet;
  packet->length(0);

  if(mysql_bin_log.is_open())
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

  if(my_net_write(&thd->net, (char*)thd->packet.ptr(), packet->length()))
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
  
  if(!mysql_bin_log.is_open())
  {
    errmsg = "binlog is not open";
    goto err;
  }

  field_list.push_back(new Item_empty_string("Log_name", 128));
  if(send_fields(thd, field_list, 1))
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
    if(my_net_write(net, (char*) packet->ptr(), packet->length()))
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

int connect_to_master(THD *thd, MYSQL* mysql, MASTER_INFO* mi)
{
  if(!mc_mysql_connect(mysql, mi->host, mi->user, mi->password, 0,
		   mi->port, 0, 0))
  {
    sql_print_error("Connection to master failed: %s",
		    mc_mysql_error(mysql));
    return 1;
  }
  return 0;
}

static inline void cleanup_mysql_results(MYSQL_RES* db_res,
				  MYSQL_RES** cur, MYSQL_RES** start)
{
  for( ; cur >= start; --cur)
    if(*cur)
      mc_mysql_free_result(*cur);
  mc_mysql_free_result(db_res);
}

static inline int fetch_db_tables(THD* thd, MYSQL* mysql, const char* db,
				  MYSQL_RES* table_res)
{
  MYSQL_ROW row;
  
  for( row = mc_mysql_fetch_row(table_res); row;
       row = mc_mysql_fetch_row(table_res))
  {
    TABLE_LIST table;
    const char* table_name = row[0];
    int error;
    if(table_rules_on)
    {
      table.next = 0;
      table.db = (char*)db;
      table.real_name = (char*)table_name;
      if(!tables_ok(thd, &table))
	continue;
    }
    
    if((error = fetch_nx_table(thd, db, table_name, &glob_mi, mysql)))
      return error;
  }

  return 0;
}

int load_master_data(THD* thd)
{
  MYSQL mysql;
  MYSQL_RES* master_status_res = 0;
  bool slave_was_running = 0;
  int error = 0;
  
  mc_mysql_init(&mysql);

  pthread_mutex_lock(&LOCK_slave);
  // we do not want anyone messing with the slave at all for the entire
  // duration of the data load;

  // first, kill the slave
  if((slave_was_running = slave_running))
  {
    abort_slave = 1;
    thr_alarm_kill(slave_real_id);
    thd->proc_info = "waiting for slave to die";
    while(slave_running)
      pthread_cond_wait(&COND_slave_stopped, &LOCK_slave); // wait until done
  }
  

  if(connect_to_master(thd, &mysql, &glob_mi))
  {
    net_printf(&thd->net, error = ER_CONNECT_TO_MASTER,
		 mc_mysql_error(&mysql));
    goto err;
  }

  // now that we are connected, get all database and tables in each
  {
    MYSQL_RES *db_res, **table_res, **table_res_end, **cur_table_res;
    uint num_dbs;
    MYSQL_ROW row;
    
    if(mc_mysql_query(&mysql, "show databases", 0) ||
       !(db_res = mc_mysql_store_result(&mysql)))
    {
      net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		 mc_mysql_error(&mysql));
      goto err;
    }

    if(!(num_dbs = mc_mysql_num_rows(db_res)))
      goto err;
    // in theory, the master could have no databases at all
    // and run with skip-grant
    
    if(!(table_res = (MYSQL_RES**)thd->alloc(num_dbs * sizeof(MYSQL_RES*))))
    {
      net_printf(&thd->net, error = ER_OUTOFMEMORY);
      goto err;
    }

    // this is a temporary solution until we have online backup
    // capabilities - to be replaced once online backup is working
    // we wait to issue FLUSH TABLES WITH READ LOCK for as long as we
    // can to minimize the lock time
    if(mc_mysql_query(&mysql, "FLUSH TABLES WITH READ LOCK", 0)
       || mc_mysql_query(&mysql, "SHOW MASTER STATUS",0) ||
       !(master_status_res = mc_mysql_store_result(&mysql)))
    {
      net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		 mc_mysql_error(&mysql));
      goto err;
    }
    
    // go through every table in every database, and if the replication
    // rules allow replicating it, get it

    table_res_end = table_res + num_dbs;

    for(cur_table_res = table_res; cur_table_res < table_res_end;
	++cur_table_res)
    {
      MYSQL_ROW row = mc_mysql_fetch_row(db_res);
      // since we know how many rows we have, this can never be NULL

      char* db = row[0];
      int drop_error = 0;

      // do not replicate databases excluded by rules
      // also skip mysql database - in most cases the user will
      // mess up and not exclude mysql database with the rules when
      // he actually means to - in this case, he is up for a surprise if
      // his priv tables get dropped and downloaded from master
      // TO DO - add special option, not enabled
      // by default, to allow inclusion of mysql database into load
      // data from master
      if(!db_ok(db, replicate_do_db, replicate_ignore_db) ||
	 !strcmp(db,"mysql"))
      {
	*cur_table_res = 0;
	continue;
      }
      
      if((drop_error = mysql_rm_db(0, db, 1)) ||
	 mysql_create_db(0, db, 0))
      {
	error = (drop_error) ? ER_DB_DROP_DELETE : ER_CANT_CREATE_DB;
	net_printf(&thd->net, error, db, my_error);
	cleanup_mysql_results(db_res, cur_table_res - 1, table_res);
	goto err;
      }

      if(mc_mysql_select_db(&mysql, db) ||
	 mc_mysql_query(&mysql, "show tables", 0) ||
	 !(*cur_table_res = mc_mysql_store_result(&mysql)))
      {
	net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		   mc_mysql_error(&mysql));
	cleanup_mysql_results(db_res, cur_table_res - 1, table_res);
	goto err;
      }

      if((error = fetch_db_tables(thd, &mysql, db, *cur_table_res)))
      {
	// we do not report the error - fetch_db_tables handles it
	cleanup_mysql_results(db_res, cur_table_res, table_res);
	goto err;
      }
    }

    cleanup_mysql_results(db_res, cur_table_res - 1, table_res);

    // adjust position in the master
    if(master_status_res)
    {
      MYSQL_ROW row = mc_mysql_fetch_row(master_status_res);

      // we need this check because the master may not be running with
      // log-bin, but it will still allow us to do all the steps
      // of LOAD DATA FROM MASTER - no reason to forbid it, really,
      // although it does not make much sense for the user to do it 
      if(row[0] && row[1])
      {
	strmake(glob_mi.log_file_name, row[0], sizeof(glob_mi.log_file_name));
	glob_mi.pos = atoi(row[1]); // atoi() is ok, since offset is <= 1GB
	if(glob_mi.pos < 4)
	  glob_mi.pos = 4; // don't hit the magic number
	glob_mi.pending = 0;
	flush_master_info(&glob_mi);
      }

      mc_mysql_free_result(master_status_res);
    }
    
    if(mc_mysql_query(&mysql, "UNLOCK TABLES", 0))
    {
      net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		 mc_mysql_error(&mysql));
      goto err;
    }
  }
err:  
  pthread_mutex_unlock(&LOCK_slave);
  if(slave_was_running)
    start_slave(0, 0);
  mc_mysql_close(&mysql); // safe to call since we always do mc_mysql_init()
  if(!error)
    send_ok(&thd->net);
  
  return error;
}



