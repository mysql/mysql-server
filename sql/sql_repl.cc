/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysql_priv.h"
#ifdef HAVE_REPLICATION

#include "rpl_mi.h"
#include "sql_repl.h"
#include "log_event.h"
#include "rpl_filter.h"
#include <my_dir.h>
#include "debug_sync.h"

int max_binlog_dump_events = 0; // unlimited
my_bool opt_sporadic_binlog_dump_fail = 0;
#ifndef DBUG_OFF
static int binlog_dump_count = 0;
#endif

/*
    fake_rotate_event() builds a fake (=which does not exist physically in any
    binlog) Rotate event, which contains the name of the binlog we are going to
    send to the slave (because the slave may not know it if it just asked for
    MASTER_LOG_FILE='', MASTER_LOG_POS=4).
    < 4.0.14, fake_rotate_event() was called only if the requested pos was 4.
    After this version we always call it, so that a 3.23.58 slave can rely on
    it to detect if the master is 4.0 (and stop) (the _fake_ Rotate event has
    zeros in the good positions which, by chance, make it possible for the 3.23
    slave to detect that this event is unexpected) (this is luck which happens
    because the master and slave disagree on the size of the header of
    Log_event).

    Relying on the event length of the Rotate event instead of these
    well-placed zeros was not possible as Rotate events have a variable-length
    part.
*/

static int fake_rotate_event(NET* net, String* packet, char* log_file_name,
                             ulonglong position, const char** errmsg)
{
  DBUG_ENTER("fake_rotate_event");
  char header[LOG_EVENT_HEADER_LEN], buf[ROTATE_HEADER_LEN+100];
  /*
    'when' (the timestamp) is set to 0 so that slave could distinguish between
    real and fake Rotate events (if necessary)
  */
  memset(header, 0, 4);
  header[EVENT_TYPE_OFFSET] = ROTATE_EVENT;

  char* p = log_file_name+dirname_length(log_file_name);
  uint ident_len = (uint) strlen(p);
  ulong event_len = ident_len + LOG_EVENT_HEADER_LEN + ROTATE_HEADER_LEN;
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int2store(header + FLAGS_OFFSET, LOG_EVENT_ARTIFICIAL_F);

  // TODO: check what problems this may cause and fix them
  int4store(header + LOG_POS_OFFSET, 0);

  packet->append(header, sizeof(header));
  int8store(buf+R_POS_OFFSET,position);
  packet->append(buf, ROTATE_HEADER_LEN);
  packet->append(p,ident_len);
  if (my_net_write(net, (uchar*) packet->ptr(), packet->length()))
  {
    *errmsg = "failed on my_net_write()";
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

static int send_file(THD *thd)
{
  NET* net = &thd->net;
  int fd = -1, error = 1;
  size_t bytes;
  char fname[FN_REFLEN+1];
  const char *errmsg = 0;
  int old_timeout;
  unsigned long packet_len;
  uchar buf[IO_SIZE];				// It's safe to alloc this
  DBUG_ENTER("send_file");

  /*
    The client might be slow loading the data, give him wait_timeout to do
    the job
  */
  old_timeout= net->read_timeout;
  my_net_set_read_timeout(net, thd->variables.net_wait_timeout);

  /*
    We need net_flush here because the client will not know it needs to send
    us the file name until it has processed the load event entry
  */
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

  while ((long) (bytes= my_read(fd, buf, IO_SIZE, MYF(0))) > 0)
  {
    if (my_net_write(net, buf, bytes))
    {
      errmsg = "while writing data to client";
      goto err;
    }
  }

 end:
  if (my_net_write(net, (uchar*) "", 0) || net_flush(net) ||
      (my_net_read(net) == packet_error))
  {
    errmsg = "while negotiating file transfer close";
    goto err;
  }
  error = 0;

 err:
  my_net_set_read_timeout(net, old_timeout);
  if (fd >= 0)
    (void) my_close(fd, MYF(0));
  if (errmsg)
  {
    sql_print_error("Failed in send_file() %s", errmsg);
    DBUG_PRINT("error", ("%s", errmsg));
  }
  DBUG_RETURN(error);
}


/*
  Adjust the position pointer in the binary log file for all running slaves

  SYNOPSIS
    adjust_linfo_offsets()
    purge_offset	Number of bytes removed from start of log index file

  NOTES
    - This is called when doing a PURGE when we delete lines from the
      index log file

  REQUIREMENTS
    - Before calling this function, we have to ensure that no threads are
      using any binary log file before purge_offset.a

  TODO
    - Inform the slave threads that they should sync the position
      in the binary log file with flush_relay_log_info.
      Now they sync is done for next read.
*/

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
      /*
	Index file offset can be less that purge offset only if
	we just started reading the index file. In that case
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
  size_t log_name_len = strlen(log_name) + 1;
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
      if (result)
	break;
    }
  }

  pthread_mutex_unlock(&LOCK_thread_count);
  return result;
}

bool purge_error_message(THD* thd, int res)
{
  uint errmsg= 0;

  switch (res)  {
  case 0: break;
  case LOG_INFO_EOF:	errmsg= ER_UNKNOWN_TARGET_BINLOG; break;
  case LOG_INFO_IO:	errmsg= ER_IO_ERR_LOG_INDEX_READ; break;
  case LOG_INFO_INVALID:errmsg= ER_BINLOG_PURGE_PROHIBITED; break;
  case LOG_INFO_SEEK:	errmsg= ER_FSEEK_FAIL; break;
  case LOG_INFO_MEM:	errmsg= ER_OUT_OF_RESOURCES; break;
  case LOG_INFO_FATAL:	errmsg= ER_BINLOG_PURGE_FATAL_ERR; break;
  case LOG_INFO_IN_USE: errmsg= ER_LOG_IN_USE; break;
  case LOG_INFO_EMFILE: errmsg= ER_BINLOG_PURGE_EMFILE; break;
  default:		errmsg= ER_LOG_PURGE_UNKNOWN_ERR; break;
  }

  if (errmsg)
  {
    my_message(errmsg, ER(errmsg), MYF(0));
    return TRUE;
  }
  my_ok(thd);
  return FALSE;
}


/**
  Execute a PURGE BINARY LOGS TO <log> command.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param to_log Name of the last log to purge.

  @retval FALSE success
  @retval TRUE failure
*/
bool purge_master_logs(THD* thd, const char* to_log)
{
  char search_file_name[FN_REFLEN];
  if (!mysql_bin_log.is_open())
  {
    my_ok(thd);
    return FALSE;
  }

  mysql_bin_log.make_log_name(search_file_name, to_log);
  return purge_error_message(thd,
			     mysql_bin_log.purge_logs(search_file_name, 0, 1,
						      1, NULL));
}


/**
  Execute a PURGE BINARY LOGS BEFORE <date> command.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param purge_time Date before which logs should be purged.

  @retval FALSE success
  @retval TRUE failure
*/
bool purge_master_logs_before_date(THD* thd, time_t purge_time)
{
  if (!mysql_bin_log.is_open())
  {
    my_ok(thd);
    return 0;
  }
  return purge_error_message(thd,
                             mysql_bin_log.purge_logs_before_date(purge_time));
}

int test_for_non_eof_log_read_errors(int error, const char **errmsg)
{
  if (error == LOG_READ_EOF)
    return 0;
  my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
  switch (error) {
  case LOG_READ_BOGUS:
    *errmsg = "bogus data in log event";
    break;
  case LOG_READ_TOO_LARGE:
    *errmsg = "log event entry exceeded max_allowed_packet; \
Increase max_allowed_packet on master";
    break;
  case LOG_READ_IO:
    *errmsg = "I/O error reading log event";
    break;
  case LOG_READ_MEM:
    *errmsg = "memory allocation failed reading log event";
    break;
  case LOG_READ_TRUNC:
    *errmsg = "binlog truncated in the middle of event";
    break;
  default:
    *errmsg = "unknown error reading log event on the master";
    break;
  }
  return error;
}


/*
  TODO: Clean up loop to only have one call to send_file()
*/

void mysql_binlog_send(THD* thd, char* log_ident, my_off_t pos,
		       ushort flags)
{
  LOG_INFO linfo;
  char *log_file_name = linfo.log_file_name;
  char search_file_name[FN_REFLEN], *name;
  IO_CACHE log;
  File file = -1;
  String* packet = &thd->packet;
  int error;
  const char *errmsg = "Unknown error";
  NET* net = &thd->net;
  pthread_mutex_t *log_lock;
  bool binlog_can_be_corrupted= FALSE;
#ifndef DBUG_OFF
  int left_events = max_binlog_dump_events;
#endif
  int old_max_allowed_packet= thd->variables.max_allowed_packet;
  DBUG_ENTER("mysql_binlog_send");
  DBUG_PRINT("enter",("log_ident: '%s'  pos: %ld", log_ident, (long) pos));

  bzero((char*) &log,sizeof(log));

#ifndef DBUG_OFF
  if (opt_sporadic_binlog_dump_fail && (binlog_dump_count++ % 2))
  {
    errmsg = "Master failed COM_BINLOG_DUMP to test if slave can recover";
    my_errno= ER_UNKNOWN_ERROR;
    goto err;
  }
#endif

  if (!mysql_bin_log.is_open())
  {
    errmsg = "Binary log is not open";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }
  if (!server_id_supplied)
  {
    errmsg = "Misconfigured master - server id was not set";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }

  name=search_file_name;
  if (log_ident[0])
    mysql_bin_log.make_log_name(search_file_name, log_ident);
  else
    name=0;					// Find first log

  linfo.index_file_offset = 0;

  if (mysql_bin_log.find_log_pos(&linfo, name, 1))
  {
    errmsg = "Could not find first log file name in binary log index file";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }

  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = &linfo;
  pthread_mutex_unlock(&LOCK_thread_count);

  if ((file=open_binlog(&log, log_file_name, &errmsg)) < 0)
  {
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }
  if (pos < BIN_LOG_HEADER_SIZE || pos > my_b_filelength(&log))
  {
    errmsg= "Client requested master to start replication from \
impossible position";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }

  /*
    We need to start a packet with something other than 255
    to distinguish it from error
  */
  packet->set("\0", 1, &my_charset_bin); /* This is the start of a new packet */

  /*
    Tell the client about the log name with a fake Rotate event;
    this is needed even if we also send a Format_description_log_event
    just after, because that event does not contain the binlog's name.
    Note that as this Rotate event is sent before
    Format_description_log_event, the slave cannot have any info to
    understand this event's format, so the header len of
    Rotate_log_event is FROZEN (so in 5.0 it will have a header shorter
    than other events except FORMAT_DESCRIPTION_EVENT).
    Before 4.0.14 we called fake_rotate_event below only if (pos ==
    BIN_LOG_HEADER_SIZE), because if this is false then the slave
    already knows the binlog's name.
    Since, we always call fake_rotate_event; if the slave already knew
    the log's name (ex: CHANGE MASTER TO MASTER_LOG_FILE=...) this is
    useless but does not harm much. It is nice for 3.23 (>=.58) slaves
    which test Rotate events to see if the master is 4.0 (then they
    choose to stop because they can't replicate 4.0); by always calling
    fake_rotate_event we are sure that 3.23.58 and newer will detect the
    problem as soon as replication starts (BUG#198).
    Always calling fake_rotate_event makes sending of normal
    (=from-binlog) Rotate events a priori unneeded, but it is not so
    simple: the 2 Rotate events are not equivalent, the normal one is
    before the Stop event, the fake one is after. If we don't send the
    normal one, then the Stop event will be interpreted (by existing 4.0
    slaves) as "the master stopped", which is wrong. So for safety,
    given that we want minimum modification of 4.0, we send the normal
    and fake Rotates.
  */
  if (fake_rotate_event(net, packet, log_file_name, pos, &errmsg))
  {
    /*
       This error code is not perfect, as fake_rotate_event() does not
       read anything from the binlog; if it fails it's because of an
       error in my_net_write(), fortunately it will say so in errmsg.
    */
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }
  packet->set("\0", 1, &my_charset_bin);
  /*
    Adding MAX_LOG_EVENT_HEADER_LEN, since a binlog event can become
    this larger than the corresponding packet (query) sent 
    from client to master.
  */
  thd->variables.max_allowed_packet= MAX_MAX_ALLOWED_PACKET;

  /*
    We can set log_lock now, it does not move (it's a member of
    mysql_bin_log, and it's already inited, and it will be destroyed
    only at shutdown).
  */
  log_lock = mysql_bin_log.get_log_lock();
  if (pos > BIN_LOG_HEADER_SIZE)
  {
     /*
       Try to find a Format_description_log_event at the beginning of
       the binlog
     */
     if (!(error = Log_event::read_log_event(&log, packet, log_lock)))
     {
       /*
         The packet has offsets equal to the normal offsets in a binlog
         event +1 (the first character is \0).
       */
       DBUG_PRINT("info",
                  ("Looked for a Format_description_log_event, found event type %d",
                   (*packet)[EVENT_TYPE_OFFSET+1]));
       if ((*packet)[EVENT_TYPE_OFFSET+1] == FORMAT_DESCRIPTION_EVENT)
       {
         binlog_can_be_corrupted= test((*packet)[FLAGS_OFFSET+1] &
                                       LOG_EVENT_BINLOG_IN_USE_F);
         (*packet)[FLAGS_OFFSET+1] &= ~LOG_EVENT_BINLOG_IN_USE_F;
         /*
           mark that this event with "log_pos=0", so the slave
           should not increment master's binlog position
           (rli->group_master_log_pos)
         */
         int4store((char*) packet->ptr()+LOG_POS_OFFSET+1, 0);
         /*
           if reconnect master sends FD event with `created' as 0
           to avoid destroying temp tables.
          */
         int4store((char*) packet->ptr()+LOG_EVENT_MINIMAL_HEADER_LEN+
                   ST_CREATED_OFFSET+1, (ulong) 0);
         /* send it */
         if (my_net_write(net, (uchar*) packet->ptr(), packet->length()))
         {
           errmsg = "Failed on my_net_write()";
           my_errno= ER_UNKNOWN_ERROR;
           goto err;
         }

         /*
           No need to save this event. We are only doing simple reads
           (no real parsing of the events) so we don't need it. And so
           we don't need the artificial Format_description_log_event of
           3.23&4.x.
         */
       }
     }
     else
     {
       if (test_for_non_eof_log_read_errors(error, &errmsg))
         goto err;
       /*
         It's EOF, nothing to do, go on reading next events, the
         Format_description_log_event will be found naturally if it is written.
       */
     }
     /* reset the packet as we wrote to it in any case */
     packet->set("\0", 1, &my_charset_bin);
  } /* end of if (pos > BIN_LOG_HEADER_SIZE); */
  else
  {
    /* The Format_description_log_event event will be found naturally. */
  }

  /* seek to the requested position, to start the requested dump */
  my_b_seek(&log, pos);			// Seek will done on next read

  while (!net->error && net->vio != 0 && !thd->killed)
  {
    my_off_t prev_pos= pos;
    while (!(error = Log_event::read_log_event(&log, packet, log_lock)))
    {
      prev_pos= my_b_tell(&log);
#ifndef DBUG_OFF
      if (max_binlog_dump_events && !left_events--)
      {
	net_flush(net);
	errmsg = "Debugging binlog dump abort";
	my_errno= ER_UNKNOWN_ERROR;
	goto err;
      }
#endif

      DBUG_EXECUTE_IF("dump_thread_wait_before_send_xid",
                      {
                        if ((*packet)[EVENT_TYPE_OFFSET+1] == XID_EVENT)
                        {
                          net_flush(net);
                          const char act[]=
                            "now "
                            "wait_for signal.continue";
                          DBUG_ASSERT(opt_debug_sync_timeout > 0);
                          DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                             STRING_WITH_LEN(act)));
                        }
                      });

      if ((*packet)[EVENT_TYPE_OFFSET+1] == FORMAT_DESCRIPTION_EVENT)
      {
        binlog_can_be_corrupted= test((*packet)[FLAGS_OFFSET+1] &
                                      LOG_EVENT_BINLOG_IN_USE_F);
        (*packet)[FLAGS_OFFSET+1] &= ~LOG_EVENT_BINLOG_IN_USE_F;
      }
      else if ((*packet)[EVENT_TYPE_OFFSET+1] == STOP_EVENT)
        binlog_can_be_corrupted= FALSE;

      if (my_net_write(net, (uchar*) packet->ptr(), packet->length()))
      {
	errmsg = "Failed on my_net_write()";
	my_errno= ER_UNKNOWN_ERROR;
	goto err;
      }

      DBUG_EXECUTE_IF("dump_thread_wait_before_send_xid",
                      {
                        if ((*packet)[EVENT_TYPE_OFFSET+1] == XID_EVENT)
                        {
                          net_flush(net);
                        }
                      });

      DBUG_PRINT("info", ("log event code %d",
			  (*packet)[LOG_EVENT_OFFSET+1] ));
      if ((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
      {
	if (send_file(thd))
	{
	  errmsg = "failed in send_file()";
	  my_errno= ER_UNKNOWN_ERROR;
	  goto err;
	}
      }
      packet->set("\0", 1, &my_charset_bin);
    }

    /*
      here we were reading binlog that was not closed properly (as a result
      of a crash ?). treat any corruption as EOF
    */
    if (binlog_can_be_corrupted &&
        error != LOG_READ_MEM && error != LOG_READ_EOF)
    {
      my_b_seek(&log, prev_pos);
      error=LOG_READ_EOF;
    }

    /*
      TODO: now that we are logging the offset, check to make sure
      the recorded offset and the actual match.
      Guilhem 2003-06: this is not true if this master is a slave
      <4.0.15 running with --log-slave-updates, because then log_pos may
      be the offset in the-master-of-this-master's binlog.
    */
    if (test_for_non_eof_log_read_errors(error, &errmsg))
      goto err;

    if (!(flags & BINLOG_DUMP_NON_BLOCK) &&
        mysql_bin_log.is_active(log_file_name))
    {
      /*
	Block until there is more data in the log
      */
      if (net_flush(net))
      {
	errmsg = "failed on net_flush()";
	my_errno= ER_UNKNOWN_ERROR;
	goto err;
      }

      /*
	We may have missed the update broadcast from the log
	that has just happened, let's try to catch it if it did.
	If we did not miss anything, we just wait for other threads
	to signal us.
      */
      {
	log.error=0;
	bool read_packet = 0;

#ifndef DBUG_OFF
	if (max_binlog_dump_events && !left_events--)
	{
	  errmsg = "Debugging binlog dump abort";
	  my_errno= ER_UNKNOWN_ERROR;
	  goto err;
	}
#endif

	/*
	  No one will update the log while we are reading
	  now, but we'll be quick and just read one record

	  TODO:
          Add an counter that is incremented for each time we update the
          binary log.  We can avoid the following read if the counter
          has not been updated since last read.
	*/

	pthread_mutex_lock(log_lock);
	switch (error= Log_event::read_log_event(&log, packet, (pthread_mutex_t*) 0)) {
	case 0:
	  /* we read successfully, so we'll need to send it to the slave */
	  pthread_mutex_unlock(log_lock);
	  read_packet = 1;
	  break;

	case LOG_READ_EOF:
	  DBUG_PRINT("wait",("waiting for data in binary log"));
	  if (thd->server_id==0) // for mysqlbinlog (mysqlbinlog.server_id==0)
	  {
	    pthread_mutex_unlock(log_lock);
	    goto end;
	  }
	  if (!thd->killed)
	  {
	    /* Note that the following call unlocks lock_log */
	    mysql_bin_log.wait_for_update(thd, 0);
	  }
	  else
	    pthread_mutex_unlock(log_lock);
	  DBUG_PRINT("wait",("binary log received update"));
	  break;

	default:
	  pthread_mutex_unlock(log_lock);
          test_for_non_eof_log_read_errors(error, &errmsg);
          goto err;
	}

	if (read_packet)
	{
	  thd_proc_info(thd, "Sending binlog event to slave");
	  if (my_net_write(net, (uchar*) packet->ptr(), packet->length()) )
	  {
	    errmsg = "Failed on my_net_write()";
	    my_errno= ER_UNKNOWN_ERROR;
	    goto err;
	  }

	  if ((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
	  {
	    if (send_file(thd))
	    {
	      errmsg = "failed in send_file()";
	      my_errno= ER_UNKNOWN_ERROR;
	      goto err;
	    }
	  }
	  packet->set("\0", 1, &my_charset_bin);
	  /*
	    No need to net_flush because we will get to flush later when
	    we hit EOF pretty quick
	  */
	}

	log.error=0;
      }
    }
    else
    {
      bool loop_breaker = 0;
      /* need this to break out of the for loop from switch */

      thd_proc_info(thd, "Finished reading one binlog; switching to next binlog");
      switch (mysql_bin_log.find_next_log(&linfo, 1)) {
      case 0:
	break;
      case LOG_INFO_EOF:
        if (mysql_bin_log.is_active(log_file_name))
        {
          loop_breaker = (flags & BINLOG_DUMP_NON_BLOCK);
          break;
        }
      default:
	errmsg = "could not find next log";
	my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
	goto err;
      }

      if (loop_breaker)
        break;

      end_io_cache(&log);
      (void) my_close(file, MYF(MY_WME));

      /*
        Call fake_rotate_event() in case the previous log (the one which
        we have just finished reading) did not contain a Rotate event
        (for example (I don't know any other example) the previous log
        was the last one before the master was shutdown & restarted).
        This way we tell the slave about the new log's name and
        position.  If the binlog is 5.0, the next event we are going to
        read and send is Format_description_log_event.
      */
      if ((file=open_binlog(&log, log_file_name, &errmsg)) < 0 ||
	  fake_rotate_event(net, packet, log_file_name, BIN_LOG_HEADER_SIZE,
                            &errmsg))
      {
	my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
	goto err;
      }

      packet->length(0);
      packet->append('\0');
    }
  }

end:
  end_io_cache(&log);
  (void)my_close(file, MYF(MY_WME));

  my_eof(thd);
  thd_proc_info(thd, "Waiting to finalize termination");
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  thd->variables.max_allowed_packet= old_max_allowed_packet;
  DBUG_VOID_RETURN;

err:
  thd_proc_info(thd, "Waiting to finalize termination");
  end_io_cache(&log);
  /*
    Exclude  iteration through thread list
    this is needed for purge_logs() - it will iterate through
    thread list and update thd->current_linfo->index_file_offset
    this mutex will make sure that it never tried to update our linfo
    after we return from this stack frame
  */
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  if (file >= 0)
    (void) my_close(file, MYF(MY_WME));
  thd->variables.max_allowed_packet= old_max_allowed_packet;

  my_message(my_errno, errmsg, MYF(0));
  DBUG_VOID_RETURN;
}


/**
  Execute a START SLAVE statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the slave's IO thread.

  @param net_report If true, saves the exit status into thd->main_da.

  @retval 0 success
  @retval 1 error
*/
int start_slave(THD* thd , Master_info* mi,  bool net_report)
{
  int slave_errno= 0;
  int thread_mask;
  DBUG_ENTER("start_slave");

  if (check_access(thd, SUPER_ACL, any_db,0,0,0,0))
    DBUG_RETURN(1);
  lock_slave_threads(mi);  // this allows us to cleanly read slave_running
  // Get a mask of _stopped_ threads
  init_thread_mask(&thread_mask,mi,1 /* inverse */);
  /*
    Below we will start all stopped threads.  But if the user wants to
    start only one thread, do as if the other thread was running (as we
    don't wan't to touch the other thread), so set the bit to 0 for the
    other thread
  */
  if (thd->lex->slave_thd_opt)
    thread_mask&= thd->lex->slave_thd_opt;
  if (thread_mask) //some threads are stopped, start them
  {
    if (init_master_info(mi,master_info_file,relay_log_info_file, 0,
			 thread_mask))
      slave_errno=ER_MASTER_INFO;
    else if (server_id_supplied && *mi->host)
    {
      /*
        If we will start SQL thread we will care about UNTIL options If
        not and they are specified we will ignore them and warn user
        about this fact.
      */
      if (thread_mask & SLAVE_SQL)
      {
        pthread_mutex_lock(&mi->rli.data_lock);

        if (thd->lex->mi.pos)
        {
          if (thd->lex->mi.relay_log_pos)
            slave_errno=ER_BAD_SLAVE_UNTIL_COND;
          mi->rli.until_condition= Relay_log_info::UNTIL_MASTER_POS;
          mi->rli.until_log_pos= thd->lex->mi.pos;
          /*
             We don't check thd->lex->mi.log_file_name for NULL here
             since it is checked in sql_yacc.yy
          */
          strmake(mi->rli.until_log_name, thd->lex->mi.log_file_name,
                  sizeof(mi->rli.until_log_name)-1);
        }
        else if (thd->lex->mi.relay_log_pos)
        {
          if (thd->lex->mi.pos)
            slave_errno=ER_BAD_SLAVE_UNTIL_COND;
          mi->rli.until_condition= Relay_log_info::UNTIL_RELAY_POS;
          mi->rli.until_log_pos= thd->lex->mi.relay_log_pos;
          strmake(mi->rli.until_log_name, thd->lex->mi.relay_log_name,
                  sizeof(mi->rli.until_log_name)-1);
        }
        else
          mi->rli.clear_until_condition();

        if (mi->rli.until_condition != Relay_log_info::UNTIL_NONE)
        {
          /* Preparing members for effective until condition checking */
          const char *p= fn_ext(mi->rli.until_log_name);
          char *p_end;
          if (*p)
          {
            //p points to '.'
            mi->rli.until_log_name_extension= strtoul(++p,&p_end, 10);
            /*
              p_end points to the first invalid character. If it equals
              to p, no digits were found, error. If it contains '\0' it
              means  conversion went ok.
            */
            if (p_end==p || *p_end)
              slave_errno=ER_BAD_SLAVE_UNTIL_COND;
          }
          else
            slave_errno=ER_BAD_SLAVE_UNTIL_COND;

          /* mark the cached result of the UNTIL comparison as "undefined" */
          mi->rli.until_log_names_cmp_result=
            Relay_log_info::UNTIL_LOG_NAMES_CMP_UNKNOWN;

          /* Issuing warning then started without --skip-slave-start */
          if (!opt_skip_slave_start)
            push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                         ER_MISSING_SKIP_SLAVE,
                         ER(ER_MISSING_SKIP_SLAVE));
        }

        pthread_mutex_unlock(&mi->rli.data_lock);
      }
      else if (thd->lex->mi.pos || thd->lex->mi.relay_log_pos)
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_UNTIL_COND_IGNORED,
                     ER(ER_UNTIL_COND_IGNORED));

      if (!slave_errno)
        slave_errno = start_slave_threads(0 /*no mutex */,
					1 /* wait for start */,
					mi,
					master_info_file,relay_log_info_file,
					thread_mask);
    }
    else
      slave_errno = ER_BAD_SLAVE;
  }
  else
  {
    /* no error if all threads are already started, only a warning */
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_SLAVE_WAS_RUNNING,
                 ER(ER_SLAVE_WAS_RUNNING));
  }

  unlock_slave_threads(mi);

  if (slave_errno)
  {
    if (net_report)
      my_message(slave_errno, ER(slave_errno), MYF(0));
    DBUG_RETURN(1);
  }
  else if (net_report)
    my_ok(thd);

  DBUG_RETURN(0);
}


/**
  Execute a STOP SLAVE statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the slave's IO thread.

  @param net_report If true, saves the exit status into thd->main_da.

  @retval 0 success
  @retval 1 error
*/
int stop_slave(THD* thd, Master_info* mi, bool net_report )
{
  DBUG_ENTER("stop_slave");
  
  int slave_errno;
  if (!thd)
    thd = current_thd;

  if (check_access(thd, SUPER_ACL, any_db,0,0,0,0))
    DBUG_RETURN(1);
  thd_proc_info(thd, "Killing slave");
  int thread_mask;
  lock_slave_threads(mi);
  // Get a mask of _running_ threads
  init_thread_mask(&thread_mask,mi,0 /* not inverse*/);
  /*
    Below we will stop all running threads.
    But if the user wants to stop only one thread, do as if the other thread
    was stopped (as we don't wan't to touch the other thread), so set the
    bit to 0 for the other thread
  */
  if (thd->lex->slave_thd_opt)
    thread_mask &= thd->lex->slave_thd_opt;

  if (thread_mask)
  {
    slave_errno= terminate_slave_threads(mi,thread_mask,
                                         1 /*skip lock */);
  }
  else
  {
    //no error if both threads are already stopped, only a warning
    slave_errno= 0;
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_SLAVE_WAS_NOT_RUNNING,
                 ER(ER_SLAVE_WAS_NOT_RUNNING));
  }
  unlock_slave_threads(mi);
  thd_proc_info(thd, 0);

  if (slave_errno)
  {
    if (net_report)
      my_message(slave_errno, ER(slave_errno), MYF(0));
    DBUG_RETURN(1);
  }
  else if (net_report)
    my_ok(thd);

  DBUG_RETURN(0);
}


/**
  Execute a RESET SLAVE statement.

  @param thd Pointer to THD object of the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the slave.

  @retval 0 success
  @retval 1 error
*/
int reset_slave(THD *thd, Master_info* mi)
{
  MY_STAT stat_area;
  char fname[FN_REFLEN];
  int thread_mask= 0, error= 0;
  uint sql_errno=ER_UNKNOWN_ERROR;
  const char* errmsg= "Unknown error occured while reseting slave";
  DBUG_ENTER("reset_slave");

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /* not inverse */);
  if (thread_mask) // We refuse if any slave thread is running
  {
    sql_errno= ER_SLAVE_MUST_STOP;
    error=1;
    goto err;
  }

  ha_reset_slave(thd);

  // delete relay logs, clear relay log coordinates
  if ((error= purge_relay_logs(&mi->rli, thd,
			       1 /* just reset */,
			       &errmsg)))
  {
    sql_errno= ER_RELAY_LOG_FAIL;
    goto err;
  }

  /*
    Clear master's log coordinates and reset host/user/etc to the values
    specified in mysqld's options (only for good display of SHOW SLAVE STATUS;
    next init_master_info() (in start_slave() for example) would have set them
    the same way; but here this is for the case where the user does SHOW SLAVE
    STATUS; before doing START SLAVE;
  */
  init_master_info_with_options(mi);
  /*
     Reset errors (the idea is that we forget about the
     old master).
  */
  mi->clear_error();
  mi->rli.clear_error();
  mi->rli.clear_until_condition();

  // close master_info_file, relay_log_info_file, set mi->inited=rli->inited=0
  end_master_info(mi);
  // and delete these two files
  fn_format(fname, master_info_file, mysql_data_home, "", 4+32);
  if (my_stat(fname, &stat_area, MYF(0)) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }
  // delete relay_log_info_file
  fn_format(fname, relay_log_info_file, mysql_data_home, "", 4+32);
  if (my_stat(fname, &stat_area, MYF(0)) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }

err:
  unlock_slave_threads(mi);
  if (error)
    my_error(sql_errno, MYF(0), errmsg);
  DBUG_RETURN(error);
}

/*

  Kill all Binlog_dump threads which previously talked to the same slave
  ("same" means with the same server id). Indeed, if the slave stops, if the
  Binlog_dump thread is waiting (pthread_cond_wait) for binlog update, then it
  will keep existing until a query is written to the binlog. If the master is
  idle, then this could last long, and if the slave reconnects, we could have 2
  Binlog_dump threads in SHOW PROCESSLIST, until a query is written to the
  binlog. To avoid this, when the slave reconnects and sends COM_BINLOG_DUMP,
  the master kills any existing thread with the slave's server id (if this id is
  not zero; it will be true for real slaves, but false for mysqlbinlog when it
  sends COM_BINLOG_DUMP to get a remote binlog dump).

  SYNOPSIS
    kill_zombie_dump_threads()
    slave_server_id     the slave's server id

*/


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
      pthread_mutex_lock(&tmp->LOCK_thd_kill);	// Lock from delete
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  if (tmp)
  {
    /*
      Here we do not call kill_one_thread() as
      it will be slow because it will iterate through the list
      again. We just to do kill the thread ourselves.
    */
    tmp->awake(THD::KILL_QUERY);
    pthread_mutex_unlock(&tmp->LOCK_thd_kill);
  }
}


/**
  Execute a CHANGE MASTER statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object belonging to the slave's IO
  thread.

  @retval FALSE success
  @retval TRUE error
*/
bool change_master(THD* thd, Master_info* mi)
{
  int thread_mask;
  const char* errmsg= 0;
  bool need_relay_log_purge= 1;
  char saved_host[HOSTNAME_LENGTH + 1];
  uint saved_port;
  char saved_log_name[FN_REFLEN];
  my_off_t saved_log_pos;
  DBUG_ENTER("change_master");

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /*not inverse*/);
  if (thread_mask) // We refuse if any slave thread is running
  {
    my_message(ER_SLAVE_MUST_STOP, ER(ER_SLAVE_MUST_STOP), MYF(0));
    unlock_slave_threads(mi);
    DBUG_RETURN(TRUE);
  }

  thd_proc_info(thd, "Changing master");
  LEX_MASTER_INFO* lex_mi= &thd->lex->mi;
  // TODO: see if needs re-write
  if (init_master_info(mi, master_info_file, relay_log_info_file, 0,
		       thread_mask))
  {
    my_message(ER_MASTER_INFO, ER(ER_MASTER_INFO), MYF(0));
    unlock_slave_threads(mi);
    DBUG_RETURN(TRUE);
  }

  /*
    Data lock not needed since we have already stopped the running threads,
    and we have the hold on the run locks which will keep all threads that
    could possibly modify the data structures from running
  */

  /*
    Before processing the command, save the previous state.
  */
  strmake(saved_host, mi->host, HOSTNAME_LENGTH);
  saved_port= mi->port;
  strmake(saved_log_name, mi->master_log_name, FN_REFLEN - 1);
  saved_log_pos= mi->master_log_pos;

  /*
    If the user specified host or port without binlog or position,
    reset binlog's name to FIRST and position to 4.
  */

  if ((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name && !lex_mi->pos)
  {
    mi->master_log_name[0] = 0;
    mi->master_log_pos= BIN_LOG_HEADER_SIZE;
  }

  if (lex_mi->log_file_name)
    strmake(mi->master_log_name, lex_mi->log_file_name,
	    sizeof(mi->master_log_name)-1);
  if (lex_mi->pos)
  {
    mi->master_log_pos= lex_mi->pos;
  }
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));

  if (lex_mi->host)
    strmake(mi->host, lex_mi->host, sizeof(mi->host)-1);
  if (lex_mi->user)
    strmake(mi->user, lex_mi->user, sizeof(mi->user)-1);
  if (lex_mi->password)
    strmake(mi->password, lex_mi->password, sizeof(mi->password)-1);
  if (lex_mi->port)
    mi->port = lex_mi->port;
  if (lex_mi->connect_retry)
    mi->connect_retry = lex_mi->connect_retry;

  if (lex_mi->ssl != LEX_MASTER_INFO::SSL_UNCHANGED)
    mi->ssl= (lex_mi->ssl == LEX_MASTER_INFO::SSL_ENABLE);

  if (lex_mi->ssl_verify_server_cert != LEX_MASTER_INFO::SSL_UNCHANGED)
    mi->ssl_verify_server_cert=
      (lex_mi->ssl_verify_server_cert == LEX_MASTER_INFO::SSL_ENABLE);

  if (lex_mi->ssl_ca)
    strmake(mi->ssl_ca, lex_mi->ssl_ca, sizeof(mi->ssl_ca)-1);
  if (lex_mi->ssl_capath)
    strmake(mi->ssl_capath, lex_mi->ssl_capath, sizeof(mi->ssl_capath)-1);
  if (lex_mi->ssl_cert)
    strmake(mi->ssl_cert, lex_mi->ssl_cert, sizeof(mi->ssl_cert)-1);
  if (lex_mi->ssl_cipher)
    strmake(mi->ssl_cipher, lex_mi->ssl_cipher, sizeof(mi->ssl_cipher)-1);
  if (lex_mi->ssl_key)
    strmake(mi->ssl_key, lex_mi->ssl_key, sizeof(mi->ssl_key)-1);
#ifndef HAVE_OPENSSL
  if (lex_mi->ssl || lex_mi->ssl_ca || lex_mi->ssl_capath ||
      lex_mi->ssl_cert || lex_mi->ssl_cipher || lex_mi->ssl_key ||
      lex_mi->ssl_verify_server_cert )
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                 ER_SLAVE_IGNORED_SSL_PARAMS, ER(ER_SLAVE_IGNORED_SSL_PARAMS));
#endif

  if (lex_mi->relay_log_name)
  {
    need_relay_log_purge= 0;
    strmake(mi->rli.group_relay_log_name,lex_mi->relay_log_name,
	    sizeof(mi->rli.group_relay_log_name)-1);
    strmake(mi->rli.event_relay_log_name,lex_mi->relay_log_name,
	    sizeof(mi->rli.event_relay_log_name)-1);
  }

  if (lex_mi->relay_log_pos)
  {
    need_relay_log_purge= 0;
    mi->rli.group_relay_log_pos= mi->rli.event_relay_log_pos= lex_mi->relay_log_pos;
  }

  /*
    If user did specify neither host nor port nor any log name nor any log
    pos, i.e. he specified only user/password/master_connect_retry, he probably
    wants replication to resume from where it had left, i.e. from the
    coordinates of the **SQL** thread (imagine the case where the I/O is ahead
    of the SQL; restarting from the coordinates of the I/O would lose some
    events which is probably unwanted when you are just doing minor changes
    like changing master_connect_retry).
    A side-effect is that if only the I/O thread was started, this thread may
    restart from ''/4 after the CHANGE MASTER. That's a minor problem (it is a
    much more unlikely situation than the one we are fixing here).
    Note: coordinates of the SQL thread must be read here, before the
    'if (need_relay_log_purge)' block which resets them.
  */
  if (!lex_mi->host && !lex_mi->port &&
      !lex_mi->log_file_name && !lex_mi->pos &&
      need_relay_log_purge)
   {
     /*
       Sometimes mi->rli.master_log_pos == 0 (it happens when the SQL thread is
       not initialized), so we use a max().
       What happens to mi->rli.master_log_pos during the initialization stages
       of replication is not 100% clear, so we guard against problems using
       max().
      */
     mi->master_log_pos = max(BIN_LOG_HEADER_SIZE,
			      mi->rli.group_master_log_pos);
     strmake(mi->master_log_name, mi->rli.group_master_log_name,
             sizeof(mi->master_log_name)-1);
  }
  /*
    Relay log's IO_CACHE may not be inited, if rli->inited==0 (server was never
    a slave before).
  */
  if (flush_master_info(mi, FALSE, FALSE))
  {
    my_error(ER_RELAY_LOG_INIT, MYF(0), "Failed to flush master info file");
    unlock_slave_threads(mi);
    DBUG_RETURN(TRUE);
  }
  if (need_relay_log_purge)
  {
    relay_log_purge= 1;
    thd_proc_info(thd, "Purging old relay logs");
    if (purge_relay_logs(&mi->rli, thd,
			 0 /* not only reset, but also reinit */,
			 &errmsg))
    {
      my_error(ER_RELAY_LOG_FAIL, MYF(0), errmsg);
      unlock_slave_threads(mi);
      DBUG_RETURN(TRUE);
    }
  }
  else
  {
    const char* msg;
    relay_log_purge= 0;
    /* Relay log is already initialized */
    if (init_relay_log_pos(&mi->rli,
			   mi->rli.group_relay_log_name,
			   mi->rli.group_relay_log_pos,
			   0 /*no data lock*/,
			   &msg, 0))
    {
      my_error(ER_RELAY_LOG_INIT, MYF(0), msg);
      unlock_slave_threads(mi);
      DBUG_RETURN(TRUE);
    }
  }
  /*
    Coordinates in rli were spoilt by the 'if (need_relay_log_purge)' block,
    so restore them to good values. If we left them to ''/0, that would work;
    but that would fail in the case of 2 successive CHANGE MASTER (without a
    START SLAVE in between): because first one would set the coords in mi to
    the good values of those in rli, the set those in rli to ''/0, then
    second CHANGE MASTER would set the coords in mi to those of rli, i.e. to
    ''/0: we have lost all copies of the original good coordinates.
    That's why we always save good coords in rli.
  */
  mi->rli.group_master_log_pos= mi->master_log_pos;
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
  strmake(mi->rli.group_master_log_name,mi->master_log_name,
	  sizeof(mi->rli.group_master_log_name)-1);

  if (!mi->rli.group_master_log_name[0]) // uninitialized case
    mi->rli.group_master_log_pos=0;

  pthread_mutex_lock(&mi->rli.data_lock);
  mi->rli.abort_pos_wait++; /* for MASTER_POS_WAIT() to abort */
  /* Clear the errors, for a clean start */
  mi->rli.clear_error();
  mi->rli.clear_until_condition();

  sql_print_information("'CHANGE MASTER TO executed'. "
    "Previous state master_host='%s', master_port='%u', master_log_file='%s', "
    "master_log_pos='%ld'. "
    "New state master_host='%s', master_port='%u', master_log_file='%s', "
    "master_log_pos='%ld'.", saved_host, saved_port, saved_log_name,
    (ulong) saved_log_pos, mi->host, mi->port, mi->master_log_name,
    (ulong) mi->master_log_pos);

  /*
    If we don't write new coordinates to disk now, then old will remain in
    relay-log.info until START SLAVE is issued; but if mysqld is shutdown
    before START SLAVE, then old will remain in relay-log.info, and will be the
    in-memory value at restart (thus causing errors, as the old relay log does
    not exist anymore).
  */
  flush_relay_log_info(&mi->rli);
  pthread_cond_broadcast(&mi->data_cond);
  pthread_mutex_unlock(&mi->rli.data_lock);

  unlock_slave_threads(mi);
  thd_proc_info(thd, 0);
  my_ok(thd);
  DBUG_RETURN(FALSE);
}


/**
  Execute a RESET MASTER statement.

  @param thd Pointer to THD object of the client thread executing the
  statement.

  @retval 0 success
  @retval 1 error
*/
int reset_master(THD* thd)
{
  if (!mysql_bin_log.is_open())
  {
    my_message(ER_FLUSH_MASTER_BINLOG_CLOSED,
               ER(ER_FLUSH_MASTER_BINLOG_CLOSED), MYF(ME_BELL+ME_WAITTANG));
    return 1;
  }
  return mysql_bin_log.reset_logs(thd);
}

int cmp_master_pos(const char* log_file_name1, ulonglong log_pos1,
		   const char* log_file_name2, ulonglong log_pos2)
{
  int res;
  size_t log_file_name1_len=  strlen(log_file_name1);
  size_t log_file_name2_len=  strlen(log_file_name2);

  //  We assume that both log names match up to '.'
  if (log_file_name1_len == log_file_name2_len)
  {
    if ((res= strcmp(log_file_name1, log_file_name2)))
      return res;
    return (log_pos1 < log_pos2) ? -1 : (log_pos1 == log_pos2) ? 0 : 1;
  }
  return ((log_file_name1_len < log_file_name2_len) ? -1 : 1);
}


/**
  Execute a SHOW BINLOG EVENTS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool mysql_show_binlog_events(THD* thd)
{
  Protocol *protocol= thd->protocol;
  List<Item> field_list;
  const char *errmsg = 0;
  bool ret = TRUE;
  IO_CACHE log;
  File file = -1;
  int old_max_allowed_packet= thd->variables.max_allowed_packet;
  LOG_INFO linfo;

  DBUG_ENTER("mysql_show_binlog_events");

  Log_event::init_show_field_list(&field_list);
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  Format_description_log_event *description_event= new
    Format_description_log_event(3); /* MySQL 4.0 by default */

  /*
    Wait for handlers to insert any pending information
    into the binlog.  For e.g. ndb which updates the binlog asynchronously
    this is needed so that the uses sees all its own commands in the binlog
  */
  ha_binlog_wait(thd);

  if (mysql_bin_log.is_open())
  {
    LEX_MASTER_INFO *lex_mi= &thd->lex->mi;
    SELECT_LEX_UNIT *unit= &thd->lex->unit;
    ha_rows event_count, limit_start, limit_end;
    my_off_t pos = max(BIN_LOG_HEADER_SIZE, lex_mi->pos); // user-friendly
    char search_file_name[FN_REFLEN], *name;
    const char *log_file_name = lex_mi->log_file_name;
    pthread_mutex_t *log_lock = mysql_bin_log.get_log_lock();
    Log_event* ev;

    unit->set_limit(thd->lex->current_select);
    limit_start= unit->offset_limit_cnt;
    limit_end= unit->select_limit_cnt;

    name= search_file_name;
    if (log_file_name)
      mysql_bin_log.make_log_name(search_file_name, log_file_name);
    else
      name=0;					// Find first log

    linfo.index_file_offset = 0;

    if (mysql_bin_log.find_log_pos(&linfo, name, 1))
    {
      errmsg = "Could not find target log";
      goto err;
    }

    pthread_mutex_lock(&LOCK_thread_count);
    thd->current_linfo = &linfo;
    pthread_mutex_unlock(&LOCK_thread_count);

    if ((file=open_binlog(&log, linfo.log_file_name, &errmsg)) < 0)
      goto err;

    /*
      to account binlog event header size
    */
    thd->variables.max_allowed_packet += MAX_LOG_EVENT_HEADER;

    pthread_mutex_lock(log_lock);

    /*
      open_binlog() sought to position 4.
      Read the first event in case it's a Format_description_log_event, to
      know the format. If there's no such event, we are 3.23 or 4.x. This
      code, like before, can't read 3.23 binlogs.
      This code will fail on a mixed relay log (one which has Format_desc then
      Rotate then Format_desc).
    */
    ev = Log_event::read_log_event(&log,(pthread_mutex_t*)0,description_event);
    if (ev)
    {
      if (ev->get_type_code() == FORMAT_DESCRIPTION_EVENT)
      {
        delete description_event;
        description_event= (Format_description_log_event*) ev;
      }
      else
        delete ev;
    }

    my_b_seek(&log, pos);

    if (!description_event->is_valid())
    {
      errmsg="Invalid Format_description event; could be out of memory";
      goto err;
    }

    for (event_count = 0;
	 (ev = Log_event::read_log_event(&log,(pthread_mutex_t*) 0,
                                         description_event)); )
    {
      if (event_count >= limit_start &&
	  ev->net_send(protocol, linfo.log_file_name, pos))
      {
	errmsg = "Net error";
	delete ev;
	pthread_mutex_unlock(log_lock);
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
      pthread_mutex_unlock(log_lock);
      goto err;
    }

    pthread_mutex_unlock(log_lock);
  }
  // Check that linfo is still on the function scope.
  DEBUG_SYNC(thd, "after_show_binlog_events");

  ret= FALSE;

err:
  delete description_event;
  if (file >= 0)
  {
    end_io_cache(&log);
    (void) my_close(file, MYF(MY_WME));
  }

  if (errmsg)
    my_error(ER_ERROR_WHEN_EXECUTING_COMMAND, MYF(0),
             "SHOW BINLOG EVENTS", errmsg);
  else
    my_eof(thd);

  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  thd->variables.max_allowed_packet= old_max_allowed_packet;
  DBUG_RETURN(ret);
}


/**
  Execute a SHOW MASTER STATUS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool show_binlog_info(THD* thd)
{
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_binlog_info");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("File", FN_REFLEN));
  field_list.push_back(new Item_return_int("Position",20,
					   MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Binlog_Do_DB",255));
  field_list.push_back(new Item_empty_string("Binlog_Ignore_DB",255));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);
  protocol->prepare_for_resend();

  if (mysql_bin_log.is_open())
  {
    LOG_INFO li;
    mysql_bin_log.get_current_log(&li);
    int dir_len = dirname_length(li.log_file_name);
    protocol->store(li.log_file_name + dir_len, &my_charset_bin);
    protocol->store((ulonglong) li.pos);
    protocol->store(binlog_filter->get_do_db());
    protocol->store(binlog_filter->get_ignore_db());
    if (protocol->write())
      DBUG_RETURN(TRUE);
  }
  my_eof(thd);
  DBUG_RETURN(FALSE);
}


/**
  Execute a SHOW BINARY LOGS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool show_binlogs(THD* thd)
{
  IO_CACHE *index_file;
  LOG_INFO cur;
  File file;
  char fname[FN_REFLEN];
  List<Item> field_list;
  uint length;
  int cur_dir_len;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_binlogs");

  if (!mysql_bin_log.is_open())
  {
    my_message(ER_NO_BINARY_LOGGING, ER(ER_NO_BINARY_LOGGING), MYF(0));
    DBUG_RETURN(TRUE);
  }

  field_list.push_back(new Item_empty_string("Log_name", 255));
  field_list.push_back(new Item_return_int("File_size", 20,
                                           MYSQL_TYPE_LONGLONG));
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);
  
  pthread_mutex_lock(mysql_bin_log.get_log_lock());
  mysql_bin_log.lock_index();
  index_file=mysql_bin_log.get_index_file();
  
  mysql_bin_log.raw_get_current_log(&cur); // dont take mutex
  pthread_mutex_unlock(mysql_bin_log.get_log_lock()); // lockdep, OK
  
  cur_dir_len= dirname_length(cur.log_file_name);

  reinit_io_cache(index_file, READ_CACHE, (my_off_t) 0, 0, 0);

  /* The file ends with EOF or empty line */
  while ((length=my_b_gets(index_file, fname, sizeof(fname))) > 1)
  {
    int dir_len;
    ulonglong file_length= 0;                   // Length if open fails
    fname[--length] = '\0';                     // remove the newline

    protocol->prepare_for_resend();
    dir_len= dirname_length(fname);
    length-= dir_len;
    protocol->store(fname + dir_len, length, &my_charset_bin);

    if (!(strncmp(fname+dir_len, cur.log_file_name+cur_dir_len, length)))
      file_length= cur.pos;  /* The active log, use the active position */
    else
    {
      /* this is an old log, open it and find the size */
      if ((file= my_open(fname, O_RDONLY | O_SHARE | O_BINARY,
                         MYF(0))) >= 0)
      {
        file_length= (ulonglong) my_seek(file, 0L, MY_SEEK_END, MYF(0));
        my_close(file, MYF(0));
      }
    }
    protocol->store(file_length);
    if (protocol->write())
      goto err;
  }
  mysql_bin_log.unlock_index();
  my_eof(thd);
  DBUG_RETURN(FALSE);

err:
  mysql_bin_log.unlock_index();
  DBUG_RETURN(TRUE);
}

/**
   Load data's io cache specific hook to be executed
   before a chunk of data is being read into the cache's buffer
   The fuction instantianates and writes into the binlog
   replication events along LOAD DATA processing.
   
   @param file  pointer to io-cache
   @retval 0 success
   @retval 1 failure
*/
int log_loaded_block(IO_CACHE* file)
{
  DBUG_ENTER("log_loaded_block");
  LOAD_FILE_INFO *lf_info;
  uint block_len;
  /* buffer contains position where we started last read */
  uchar* buffer= (uchar*) my_b_get_buffer_start(file);
  uint max_event_size= current_thd->variables.max_allowed_packet;
  lf_info= (LOAD_FILE_INFO*) file->arg;
  if (lf_info->thd->current_stmt_binlog_row_based)
    DBUG_RETURN(0);
  if (lf_info->last_pos_in_file != HA_POS_ERROR &&
      lf_info->last_pos_in_file >= my_b_get_pos_in_file(file))
    DBUG_RETURN(0);
  
  for (block_len= (uint) (my_b_get_bytes_in_buffer(file)); block_len > 0;
       buffer += min(block_len, max_event_size),
       block_len -= min(block_len, max_event_size))
  {
    lf_info->last_pos_in_file= my_b_get_pos_in_file(file);
    if (lf_info->wrote_create_file)
    {
      Append_block_log_event a(lf_info->thd, lf_info->thd->db, buffer,
                               min(block_len, max_event_size),
                               lf_info->log_delayed);
      if (mysql_bin_log.write(&a))
        DBUG_RETURN(1);
    }
    else
    {
      Begin_load_query_log_event b(lf_info->thd, lf_info->thd->db,
                                   buffer,
                                   min(block_len, max_event_size),
                                   lf_info->log_delayed);
      if (mysql_bin_log.write(&b))
        DBUG_RETURN(1);
      lf_info->wrote_create_file= 1;
    }
  }
  DBUG_RETURN(0);
}

/*
  Replication System Variables
*/

class sys_var_slave_skip_counter :public sys_var
{
public:
  sys_var_slave_skip_counter(sys_var_chain *chain, const char *name_arg)
    :sys_var(name_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type != OPT_GLOBAL; }
  /*
    We can't retrieve the value of this, so we don't have to define
    type() or value_ptr()
  */
};

class sys_var_sync_binlog_period :public sys_var_long_ptr
{
public:
  sys_var_sync_binlog_period(sys_var_chain *chain, const char *name_arg, 
                             ulong *value_ptr)
    :sys_var_long_ptr(chain, name_arg,value_ptr) {}
  bool update(THD *thd, set_var *var);
};

static sys_var_chain vars = { NULL, NULL };

static sys_var_const    sys_log_slave_updates(&vars, "log_slave_updates",
                                              OPT_GLOBAL, SHOW_MY_BOOL,
                                              (uchar*) &opt_log_slave_updates);
static sys_var_const    sys_relay_log(&vars, "relay_log",
                                      OPT_GLOBAL, SHOW_CHAR_PTR,
                                      (uchar*) &opt_relay_logname);
static sys_var_const    sys_relay_log_index(&vars, "relay_log_index",
                                      OPT_GLOBAL, SHOW_CHAR_PTR,
                                      (uchar*) &opt_relaylog_index_name);
static sys_var_const    sys_relay_log_info_file(&vars, "relay_log_info_file",
                                      OPT_GLOBAL, SHOW_CHAR_PTR,
                                      (uchar*) &relay_log_info_file);
static sys_var_bool_ptr	sys_relay_log_purge(&vars, "relay_log_purge",
					    &relay_log_purge);
static sys_var_const    sys_relay_log_space_limit(&vars,
                                                  "relay_log_space_limit",
                                                  OPT_GLOBAL, SHOW_LONGLONG,
                                                  (uchar*)
                                                  &relay_log_space_limit);
static sys_var_const    sys_slave_load_tmpdir(&vars, "slave_load_tmpdir",
                                              OPT_GLOBAL, SHOW_CHAR_PTR,
                                              (uchar*) &slave_load_tmpdir);
static sys_var_long_ptr	sys_slave_net_timeout(&vars, "slave_net_timeout",
					      &slave_net_timeout);
static sys_var_const    sys_slave_skip_errors(&vars, "slave_skip_errors",
                                              OPT_GLOBAL, SHOW_CHAR,
                                              (uchar*) slave_skip_error_names);
static sys_var_long_ptr	sys_slave_trans_retries(&vars, "slave_transaction_retries",
						&slave_trans_retries);
static sys_var_sync_binlog_period sys_sync_binlog_period(&vars, "sync_binlog", &sync_binlog_period);
static sys_var_slave_skip_counter sys_slave_skip_counter(&vars, "sql_slave_skip_counter");


bool sys_var_slave_skip_counter::check(THD *thd, set_var *var)
{
  int result= 0;
  pthread_mutex_lock(&LOCK_active_mi);
  pthread_mutex_lock(&active_mi->rli.run_lock);
  if (active_mi->rli.slave_running)
  {
    my_message(ER_SLAVE_MUST_STOP, ER(ER_SLAVE_MUST_STOP), MYF(0));
    result=1;
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
  var->save_result.ulong_value= (ulong) var->value->val_int();
  return result;
}


bool sys_var_slave_skip_counter::update(THD *thd, set_var *var)
{
  pthread_mutex_lock(&LOCK_active_mi);
  pthread_mutex_lock(&active_mi->rli.run_lock);
  /*
    The following test should normally never be true as we test this
    in the check function;  To be safe against multiple
    SQL_SLAVE_SKIP_COUNTER request, we do the check anyway
  */
  if (!active_mi->rli.slave_running)
  {
    pthread_mutex_lock(&active_mi->rli.data_lock);
    active_mi->rli.slave_skip_counter= var->save_result.ulong_value;
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}


bool sys_var_sync_binlog_period::update(THD *thd, set_var *var)
{
  sync_binlog_period= (ulong) var->save_result.ulonglong_value;
  return 0;
}

int init_replication_sys_vars()
{
  if (mysql_add_sys_var_chain(vars.first, my_long_options))
  {
    /* should not happen */
    fprintf(stderr, "failed to initialize replication system variables");
    unireg_abort(1);
  }
  return 0;
}

#endif /* HAVE_REPLICATION */


