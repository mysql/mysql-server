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
#include <thr_alarm.h>
#include <my_dir.h>

extern const char* any_db;
extern pthread_handler_decl(handle_slave,arg);

static int send_file(THD *thd)
{
  NET* net = &thd->net;
  int fd = -1,bytes, error = 1;
  char fname[FN_REFLEN+1];
  char buf[IO_SIZE*15];
  const char *errmsg = 0;
  int old_timeout;
  DBUG_ENTER("send_file");

  // the client might be slow loading the data, give him wait_timeout to do
  // the job
  old_timeout = thd->net.timeout;
  thd->net.timeout = thd->inactive_timeout;

  // we need net_flush here because the client will not know it needs to send
  // us the file name until it has processed the load event entry
  if (net_flush(net) || my_net_read(net) == packet_error)
  {
    errmsg = "Failed reading file name";
    goto err;
  }

  fn_format(fname, (char*)net->read_pos + 1, "", "", 4);
  // this is needed to make replicate-ignore-db
  if (!strcmp(fname,"/dev/null"))
    goto end;
  // TODO: work on the well-known system that does not have a /dev/null :-)

  if ((fd = my_open(fname, O_RDONLY, MYF(MY_WME))) < 0)
  {
    errmsg = "Failed on my_open()";
    goto err;
  }

  while ((bytes = (int) my_read(fd, (byte*) buf, sizeof(buf),
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
  char* errmsg = 0;
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
    case LOG_INFO_IN_USE: errmsg = "A purgable log is in use, will not purge";
      break;
    default:
      errmsg = "Unknown error during purge"; break;
    }
  
  if(errmsg)
   send_error(&thd->net, 0, errmsg);
  else
    send_ok(&thd->net);
}

void mysql_binlog_send(THD* thd, char* log_ident, ulong pos, ushort flags)
{
  LOG_INFO linfo;
  char *log_file_name = linfo.log_file_name;
  char search_file_name[FN_REFLEN];
  char magic[4];
  FILE* log = NULL;
  String* packet = &thd->packet;
  int error;
  const char *errmsg = "Unknown error";
  NET* net = &thd->net;

  DBUG_ENTER("mysql_binlog_send");

  if(!mysql_bin_log.is_open())
    {
      errmsg = "Binary log is not open";
      goto err;
    }

  if(log_ident[0])
    mysql_bin_log.make_log_name(search_file_name, log_ident);
  else
    search_file_name[0] = 0;
  
  linfo.index_file_offset = 0;
  thd->current_linfo = &linfo;

  if(mysql_bin_log.find_first_log(&linfo, search_file_name))
    {
      errmsg = "Could not find first log";
      goto err;
    }
  log = my_fopen(log_file_name, O_RDONLY|O_BINARY, MYF(MY_WME));

  if(!log)
    {
      errmsg = "Could not open log file";
      goto err;
    }
  
  if(my_fread(log, (byte*) magic, sizeof(magic), MYF(MY_NABP|MY_WME)))
    {
      errmsg = "I/O error reading binlog magic number";
      goto err;
    }
  if(memcmp(magic, BINLOG_MAGIC, 4))
    {
      errmsg = "Binlog has bad magic number, fire your magician";
      goto err;
    }

  if(pos < 4)
    {
      errmsg = "Contratulations! You have hit the magic number and can win \
sweepstakes if you report the bug";
      goto err;
    }
  
  if(my_fseek(log, pos, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR )
    {
      errmsg = "Error on fseek()";
      goto err;
    }


  packet->length(0);
  packet->append("\0", 1);
  // we need to start a packet with something other than 255
  // to distiquish it from error

  while(!net->error && net->vio != 0 && !thd->killed)
    {
      pthread_mutex_t *log_lock = mysql_bin_log.get_log_lock();
      
      while(!(error = Log_event::read_log_event(log, packet, log_lock)))
	{
	  if(my_net_write(net, (char*)packet->ptr(), packet->length()) )
	    {
	      errmsg = "Failed on my_net_write()";
	      goto err;
	    }
	  DBUG_PRINT("info", ("log event code %d",
			      (*packet)[LOG_EVENT_OFFSET+1] ));
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
	}
      if(error != LOG_READ_EOF)
	{
	  switch(error)
	    {
	    case LOG_READ_BOGUS: 
	      errmsg = "bogus data in log event";
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
	    clearerr(log);

            // tell the kill thread how to wake us up
	    pthread_mutex_lock(&thd->mysys_var->mutex);
	    thd->mysys_var->current_mutex = log_lock;
	    thd->mysys_var->current_cond = &COND_binlog_update;
	    const char* proc_info = thd->proc_info;
	    thd->proc_info = "Waiting for update";
	    pthread_mutex_unlock(&thd->mysys_var->mutex);

	    bool read_packet = 0, fatal_error = 0;

	    // no one will update the log while we are reading
	    // now, but we'll be quick and just read one record
	    switch(Log_event::read_log_event(log, packet, log_lock))
	      {
	      case 0:
		read_packet = 1;
		// we read successfully, so we'll need to send it to the
		// slave
		break;
	      case LOG_READ_EOF:
		pthread_mutex_lock(log_lock);
	        pthread_cond_wait(&COND_binlog_update, log_lock);
		pthread_mutex_unlock(log_lock);
		break;

	      default:
		fatal_error = 1;
		break;
	      }


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

	    clearerr(log);
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

	  (void) my_fclose(log, MYF(MY_WME));
	  log = my_fopen(log_file_name, O_RDONLY|O_BINARY, MYF(MY_WME));
	  if(!log)
	    {
	      errmsg = "Could not open next log";
	      goto err;
	    }

	  //check the magic
	  if(my_fread(log, (byte*) magic, sizeof(magic), MYF(MY_NABP|MY_WME)))
	    {
	      errmsg = "I/O error reading binlog magic number";
	      goto err;
	    }
	  if(memcmp(magic, BINLOG_MAGIC, 4))
	    {
	      errmsg = "Binlog has bad magic number, fire your magician";
	      goto err;
	    }
	  
	  // fake Rotate_log event just in case it did not make it to the log
	  // otherwise the slave make get confused about the offset
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
	    int4store(header + EVENT_TYPE_OFFSET + 1, server_id);
	    int4store(header + EVENT_LEN_OFFSET, event_len);
	    packet->append(header, sizeof(header));
	    packet->append(p,ident_len);
	    if(my_net_write(net, (char*)packet->ptr(), packet->length()))
	      {
		errmsg = "failed on my_net_write()";
	        goto err;
	      }
	    packet->length(0);
 	    packet->append("\0",1);
	  }
	}
    }

  (void)my_fclose(log, MYF(MY_WME));
  
  send_eof(&thd->net);
  thd->proc_info = "waiting to finalize termination";
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
 err:
  thd->proc_info = "waiting to finalize termination";
  pthread_mutex_lock(&LOCK_thread_count);
  // exclude  iteration through thread list
  // this is needed for purge_logs() - it will iterate through
  // thread list and update thd->current_linfo->index_file_offset
  // this mutex will make sure that it never tried to update our linfo
  // after we return from this stack frame
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  if(log)
   (void) my_fclose(log, MYF(MY_WME));
  send_error(&thd->net, 0, errmsg);
  DBUG_VOID_RETURN;
}

int start_slave(THD* thd , bool net_report)
{
  if(!thd) thd = current_thd;
  NET* net = &thd->net;
  const char* err = 0;
  if (check_access(thd, PROCESS_ACL, any_db))
    return 1;
  pthread_mutex_lock(&LOCK_slave);
  if(!slave_running)
    if(glob_mi.inited && glob_mi.host)
      {
	pthread_t hThread;
	if(pthread_create(&hThread, &connection_attrib, handle_slave, 0))
	  {
	    err = "cannot create slave thread";
	  }
      }
    else
      err = "Master host not set or master info not initialized";
  else
    err =  "Slave already running";

  pthread_mutex_unlock(&LOCK_slave);
  if(err)
    {
      if(net_report) send_error(net, 0, err);
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
  const char* err = 0;

  if (check_access(thd, PROCESS_ACL, any_db))
    return 1;

  pthread_mutex_lock(&LOCK_slave);
  if (slave_running)
  {
    abort_slave = 1;
    thr_alarm_kill(slave_real_id);
    // do not abort the slave in the middle of a query, so we do not set
    // thd->killed for the slave thread
    thd->proc_info = "waiting for slave to die";
    pthread_cond_wait(&COND_slave_stopped, &LOCK_slave);
  }
  else
    err = "Slave is not running";

  pthread_mutex_unlock(&LOCK_slave);
  thd->proc_info = 0;

  if(err)
    {
     if(net_report) send_error(net, 0, err);
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
  bool slave_was_running = slave_running;

  if(slave_running)
    stop_slave(0,0);

  fn_format(fname, master_info_file, mysql_data_home, "", 4+16+32);
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
      pthread_cond_wait(&COND_slave_stopped, &LOCK_slave); // wait until done
    }
  pthread_mutex_unlock(&LOCK_slave);
  thd->proc_info = "changing master";
  LEX_MASTER_INFO* lex_mi = &thd->lex.mi;

  if(!glob_mi.inited)
    init_master_info(&glob_mi);
  
  pthread_mutex_lock(&glob_mi.lock);
  if((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name && !lex_mi->pos)
    {
      // if we change host or port, we must reset the postion
      glob_mi.log_file_name[0] = 0;
      glob_mi.pos = 0;
    }

  if(lex_mi->log_file_name)
    strmake(glob_mi.log_file_name, lex_mi->log_file_name,
	    sizeof(glob_mi.log_file_name));
  if(lex_mi->pos)
    glob_mi.pos = lex_mi->pos;

  if(lex_mi->host)
    strmake(glob_mi.host, lex_mi->host, sizeof(glob_mi.host));
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
  FILE* index_file;
  char fname[FN_REFLEN];
  NET* net = &thd->net;
  List<Item> field_list;
  String* packet = &thd->packet;
  
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
  if(!index_file)
    {
	errmsg = "Uninitialized index file pointer";
	mysql_bin_log.unlock_index();
	goto err;
    }
  if(my_fseek(index_file, 0, MY_SEEK_SET, MYF(MY_WME)))
    {
	errmsg = "Failed on fseek()";
	mysql_bin_log.unlock_index();
	goto err;
    }
  
  while(fgets(fname, sizeof(fname), index_file))
    {
      char* fname_end;
      *(fname_end = (strend(fname) - 1)) = 0;
      int dir_len = dirname_length(fname);
      packet->length(0);
      net_store_data(packet, fname + dir_len, (fname_end - fname)-dir_len);
      if(my_net_write(net, (char*) packet->ptr(), packet->length()))
	{
	  sql_print_error("Failed in my_net_write");
	  mysql_bin_log.unlock_index();
	  return 1;
	}
    }
  
  mysql_bin_log.unlock_index();
  send_eof(net);   
 err:
  if(errmsg)
    {
     send_error(net, 0, errmsg);
     return 1;
    }

  send_ok(net);
  return 0;
}



