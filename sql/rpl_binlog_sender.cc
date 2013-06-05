/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "rpl_binlog_sender.h"

#ifdef HAVE_REPLICATION
#include "global_threads.h"
#include "rpl_handler.h"
#include "debug_sync.h"
#include "my_pthread.h"
#include "rpl_master.h"

#ifndef DBUG_OFF
  static uint binlog_dump_count= 0;
#endif

void Binlog_sender::init()
{
  DBUG_ENTER("Binlog_sender::init");
  THD *thd= m_thd;

  thd->push_diagnostics_area(&m_diag_area);
  init_heartbeat_period();

  mysql_mutex_lock(&LOCK_thread_count);
  thd->current_linfo= &m_linfo;
  mysql_mutex_unlock(&LOCK_thread_count);

  if (log_warnings > 1)
    sql_print_information("Start binlog_dump to master_thread_id(%lu) "
                          "slave_server(%u), pos(%s, %llu)",
                          thd->thread_id, thd->server_id,
                          m_start_file, m_start_pos);

  if (RUN_HOOK(binlog_transmit, transmit_start, (thd, 0/*flags*/,
                                                 m_start_file, m_start_pos)))
  {
    set_unknow_error("Failed to run hook 'transmit_start'");
    DBUG_VOID_RETURN;
  }

  if (!mysql_bin_log.is_open())
  {
    set_fatal_error("Binary log is not open");
    DBUG_VOID_RETURN;
  }

  if (DBUG_EVALUATE_IF("simulate_no_server_id", true, !server_id_supplied))
  {
    set_fatal_error("Misconfigured master - server_id was not set");
    DBUG_VOID_RETURN;
  }

  if (m_using_gtid_protocol && gtid_mode != GTID_MODE_ON)
  {
    set_fatal_error("Request to dump GTID when @@GLOBAL_.GTID_MODE <> ON.");
    DBUG_VOID_RETURN;
  }

  if (check_start_file())
    DBUG_VOID_RETURN;

  init_checksum_alg();
  /* mysqlbinlog's server_id is 0 */
  m_wait_new_events= (thd->server_id != 0);
  /* Binary event can be vary large. So set it to max allowed packet. */
  thd->variables.max_allowed_packet= MAX_MAX_ALLOWED_PACKET;

#ifndef DBUG_OFF
  if (opt_sporadic_binlog_dump_fail && (binlog_dump_count++ % 2))
    set_unknow_error("Master fails in COM_BINLOG_DUMP because of "
                     "--sporadic-binlog-dump-fail");
  m_event_count= 0;
#endif
  DBUG_VOID_RETURN;
}

void Binlog_sender::cleanup()
{
  DBUG_ENTER("Binlog_sender::cleanup");

  THD *thd= m_thd;

  (void) RUN_HOOK(binlog_transmit, transmit_stop, (thd, 0/*flags*/));

  mysql_mutex_lock(&LOCK_thread_count);
  thd->current_linfo= NULL;
  mysql_mutex_unlock(&LOCK_thread_count);

  thd->variables.max_allowed_packet= global_system_variables.max_allowed_packet;

  thd->pop_diagnostics_area();
  if (has_error())
    my_message(m_errno, m_errmsg, MYF(0));
  else
    my_eof(thd);

  DBUG_VOID_RETURN;
}

void Binlog_sender::run()
{
  DBUG_ENTER("Binlog_sender::run");
  File file= -1;
  IO_CACHE log_cache;
  my_off_t start_pos= m_start_pos;
  const char *log_file= m_linfo.log_file_name;

  init();

  while (!has_error() && !m_thd->killed)
  {
    /*
      Faked rotate event is only required in a few cases(see comment of the
      function). But even so, a faked rotate event is always sent before sending
      event log file, even if a rotate log event exists in last binlog and
      was already sent. The slave then gets an extra rotation and records
      two Rotate_log_events.

      The main issue here are some dependencies on mysqlbinlog, that should be
      solved in the future.
    */
    if (unlikely(fake_rotate_event(&m_thd->packet, log_file, start_pos)))
      break;

    file= open_binlog_file(&log_cache, log_file, &m_errmsg);
    if (unlikely(file < 0))
    {
      set_fatal_error(m_errmsg);
      break;
    }

    THD_STAGE_INFO(m_thd, stage_sending_binlog_event_to_slave);
    if (send_binlog(&log_cache, start_pos))
      break;

    /* Will go to next file, need to copy log file name */
    set_last_file(log_file);

    THD_STAGE_INFO(m_thd,
                   stage_finished_reading_one_binlog_switching_to_next_binlog);
    int error= mysql_bin_log.find_next_log(&m_linfo, 1);
    if (unlikely(error))
    {
      set_fatal_error("could not find next log");
      break;
    }

    start_pos= BIN_LOG_HEADER_SIZE;
    end_io_cache(&log_cache);
    mysql_file_close(file, MYF(MY_WME));
    file= -1;
  }

  THD_STAGE_INFO(m_thd, stage_waiting_to_finalize_termination);
  char error_text[MAX_SLAVE_ERRMSG];
  if (file > 0)
  {
    if (is_fatal_error())
    {
      /* output events range to error message */
      my_snprintf(error_text, sizeof(error_text),
                  "%s; the first event '%s' at %lld, "
                  "the last event read from '%s' at %lld, "
                  "the last byte read from '%s' at %lld.",
                  m_errmsg,
                  m_start_file, m_start_pos, m_last_file, m_last_pos,
                  log_file, my_b_tell(&log_cache));
      set_fatal_error(error_text);
    }

    end_io_cache(&log_cache);
    mysql_file_close(file, MYF(MY_WME));
  }

  cleanup();
  DBUG_VOID_RETURN;
}

int Binlog_sender::send_binlog(IO_CACHE *log_cache, my_off_t start_pos)
{
  if (unlikely(send_format_description_event(&m_thd->packet, log_cache,
                                             start_pos > BIN_LOG_HEADER_SIZE)))
    return 1;

  if (start_pos == BIN_LOG_HEADER_SIZE)
    start_pos= my_b_tell(log_cache);

  if (m_check_previous_gtid_event)
  {
    bool has_prev_gtid_ev;
    if (has_previous_gtid_log_event(log_cache, &has_prev_gtid_ev))
      return 1;

    if (!has_prev_gtid_ev)
      return 0;
  }

  /*
    Slave is requesting a position which is in the middle of a file,
    so seek to the correct position.
  */
  if (my_b_tell(log_cache) != start_pos)
    my_b_seek(log_cache, start_pos);

  while (!m_thd->killed)
  {
    my_off_t end_pos;

    end_pos= get_binlog_end_pos(log_cache);
    if (end_pos <= 1)
      return end_pos;

    if (send_events(log_cache, end_pos))
      return 1;

    m_thd->killed= DBUG_EVALUATE_IF("simulate_kill_dump", THD::KILL_CONNECTION,
                                    m_thd->killed);

    DBUG_EXECUTE_IF("wait_after_binlog_EOF",
                    {
                      const char act[]= "now wait_for signal.rotate_finished no_clear_event";
                      DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                         STRING_WITH_LEN(act)));
                    };);
  }
  return 1;
}

inline my_off_t Binlog_sender::get_binlog_end_pos(IO_CACHE *log_cache)
{
  DBUG_ENTER("Binlog_sender::get_binlog_end_pos()");
  my_off_t log_pos= my_b_tell(log_cache);
  my_off_t end_pos= 0;

  do
  {
    mysql_bin_log.lock_binlog_end_pos();
    end_pos= mysql_bin_log.get_binlog_end_pos();
    mysql_bin_log.unlock_binlog_end_pos();

    if (unlikely(!mysql_bin_log.is_active(m_linfo.log_file_name)))
    {
      end_pos= my_b_filelength(log_cache);
      if (log_pos == end_pos)
        DBUG_RETURN(0); // Arrived the end of inactive file
      else
        DBUG_RETURN(end_pos);
    }

    DBUG_PRINT("info", ("Reading file %s, seek pos %llu, end_pos is %llu",
                        m_linfo.log_file_name, log_pos, end_pos));
    DBUG_PRINT("info", ("Active file is %s", mysql_bin_log.get_log_fname()));

    if (log_pos < end_pos)
      DBUG_RETURN(end_pos);

    /* Some data may be in net buffer, it should be flushed before waiting */
    if (!m_wait_new_events || flush_net())
      DBUG_RETURN(1);

    if (unlikely(wait_new_events(log_pos)))
      DBUG_RETURN(1);
  } while (unlikely(!m_thd->killed));

  DBUG_RETURN(1);
}

int Binlog_sender::send_events(IO_CACHE *log_cache, my_off_t end_pos)
{
  DBUG_ENTER("Binlog_sender::send_events");

  THD *thd= m_thd;
  const char *log_file= m_linfo.log_file_name;
  my_off_t log_pos= my_b_tell(log_cache);
  my_off_t exclude_group_end_pos= 0;
  bool in_exclude_group= false;

  while (likely(log_pos < end_pos))
  {
    uchar* event_ptr;
    uint32 event_len;

    if (unlikely(thd->killed))
        DBUG_RETURN(1);

    if (unlikely(read_event(log_cache, m_event_checksum_alg,
                            &event_ptr, &event_len)))
      DBUG_RETURN(1);

    DBUG_EXECUTE_IF("dump_thread_wait_before_send_xid",
                    {
                      if ((Log_event_type) event_ptr[LOG_EVENT_OFFSET] == XID_EVENT)
                      {
                        net_flush(&thd->net);
                        const char act[]=
                          "now "
                          "wait_for signal.continue";
                        DBUG_ASSERT(opt_debug_sync_timeout > 0);
                        DBUG_ASSERT(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
                      }
                    });

    log_pos= my_b_tell(log_cache);

    if (before_send_hook(&thd->packet, log_file, log_pos))
      DBUG_RETURN(1);
    /*
      TODO: Set m_exclude_gtid to NULL if all gtids in m_exclude_gtid has
      be skipped. and maybe removing the gtid from m_exclude_gtid will make
      skip_event has better performance.
    */
    if (m_exclude_gtid && (in_exclude_group= skip_event(event_ptr, event_len,
                                                        in_exclude_group)))
    {
      exclude_group_end_pos= log_pos;
      DBUG_PRINT("info", ("Event is skipped\n"));
    }
    else
    {
      /*
        A heartbeat is required before sending a event, If some events are
        skipped. It notifies the slave to increase master_log_pos for
        excluded events.
      */
      if (exclude_group_end_pos)
      {
        if (unlikely(send_heartbeat_event(NULL, exclude_group_end_pos)))
          DBUG_RETURN(1);
        exclude_group_end_pos= 0;
      }

      if (unlikely(send_packet(&thd->packet)))
        DBUG_RETURN(1);
    }

    if (unlikely(after_send_hook(&thd->packet,
                                 log_file, in_exclude_group ? log_pos : 0)))
      DBUG_RETURN(1);
  }

  if (unlikely(in_exclude_group))
  {
    if (send_heartbeat_event(&thd->packet, log_pos))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


inline bool Binlog_sender::skip_event(const uchar *event_ptr, uint32 event_len,
                                      bool in_exclude_group)
{
  DBUG_ENTER("Binlog_sender::skip_event");

  uint8 event_type= (Log_event_type) event_ptr[LOG_EVENT_OFFSET];
  switch (event_type)
  {
  case GTID_LOG_EVENT:
    {
      Format_description_log_event fd_ev(BINLOG_VERSION);
      fd_ev.checksum_alg= m_event_checksum_alg;

      Gtid_log_event gtid_ev((const char *)event_ptr, event_checksum_on() ?
                             event_len - BINLOG_CHECKSUM_LEN : event_len,
                             &fd_ev);
      Gtid gtid;
      gtid.sidno= gtid_ev.get_sidno(m_exclude_gtid->get_sid_map());
      gtid.gno= gtid_ev.get_gno();

      DBUG_RETURN(m_exclude_gtid->contains_gtid(gtid));
    }
  case ROTATE_EVENT:
    DBUG_RETURN(false);
  }
  DBUG_RETURN(in_exclude_group);
}

int Binlog_sender::wait_new_events(my_off_t log_pos)
{
  int ret= 0;
  PSI_stage_info old_stage;

  mysql_bin_log.lock_binlog_end_pos();
  m_thd->ENTER_COND(mysql_bin_log.get_log_cond(),
                    mysql_bin_log.get_binlog_end_pos_lock(),
                    &stage_master_has_sent_all_binlog_to_slave,
                    &old_stage);

  if (mysql_bin_log.get_binlog_end_pos() <= log_pos &&
      mysql_bin_log.is_active(m_linfo.log_file_name))
  {
    if (m_heartbeat_period)
      ret= wait_with_heartbeat(log_pos);
    else
      ret= wait_without_heartbeat();
  }

  /* it releases the lock set in ENTER_COND */
  m_thd->EXIT_COND(&old_stage);
  return ret;
}

inline int Binlog_sender::wait_with_heartbeat(my_off_t log_pos)
{
#ifndef DBUG_OFF
  ulong hb_info_counter= 0;
#endif
  struct timespec ts;
  int ret;

  do
  {
    set_timespec_nsec(ts, m_heartbeat_period);
    ret= mysql_bin_log.wait_for_update_bin_log(m_thd, &ts);
    if (ret != ETIMEDOUT && ret != ETIME)
      break;

#ifndef DBUG_OFF
      if (hb_info_counter < 3)
      {
        sql_print_information("master sends heartbeat message");
        hb_info_counter++;
        if (hb_info_counter == 3)
          sql_print_information("the rest of heartbeat info skipped ...");
      }
#endif
      if (send_heartbeat_event(&m_thd->packet, log_pos))
        return 1;
  } while (!m_thd->killed);

  return ret ? 1 : 0;
}

inline int Binlog_sender::wait_without_heartbeat()
{
  return mysql_bin_log.wait_for_update_bin_log(m_thd, NULL);
}

void Binlog_sender::init_heartbeat_period()
{
  my_bool null_value;
  LEX_STRING name=  { C_STRING_WITH_LEN("master_heartbeat_period")};
  user_var_entry *entry=
    (user_var_entry*) my_hash_search(&m_thd->user_vars, (uchar*) name.str,
                                     name.length);
  m_heartbeat_period= entry ? entry->val_int(&null_value) : 0;
}

int Binlog_sender::check_start_file()
{
  char index_entry_name[FN_REFLEN];
  char *name_ptr= NULL;
  File file;
  IO_CACHE cache;
  const char *errmsg;
  my_off_t size;

  if (m_start_file[0] != '\0')
  {
    mysql_bin_log.make_log_name(index_entry_name, m_start_file);
    name_ptr= index_entry_name;
  }
  else if (m_using_gtid_protocol)
  {
    if (mysql_bin_log.find_first_log_not_in_gtid_set(index_entry_name,
                                                     m_exclude_gtid,
                                                     &errmsg))
    {
      set_fatal_error(errmsg);
      return 1;
    }
    name_ptr= index_entry_name;
    /*
      find_first_log_not_in_gtid_set() guarantees the file it found has
      Previous_gtids_log_event as all following binlogs. So the variable is
      set to false which tells not to check the event again when starting to
      dump binglogs.
    */
    m_check_previous_gtid_event= false;
  }

  /*
    Index entry name is saved into m_linfo. If name_ptr is NULL,
    then starts from the first file in index file.
  */

  if (mysql_bin_log.find_log_pos(&m_linfo, name_ptr, true))
  {
    set_fatal_error("Could not find first log file name in binary log "
                    "index file");
    return 1;
  }

  if (m_start_pos < BIN_LOG_HEADER_SIZE)
  {
    set_fatal_error("Client requested master to start replication "
                    "from position < 4");
    return 1;
  }

  if ((file= open_binlog_file(&cache, m_linfo.log_file_name, &errmsg)) < 0)
  {
    set_fatal_error(errmsg);
    return 1;
  }

  size= my_b_filelength(&cache);
  end_io_cache(&cache);
  mysql_file_close(file, MYF(MY_WME));

  if (m_start_pos > size)
  {
    set_fatal_error("Client requested master to start replication from "
                    "position > file size");
    return 1;
  }
  return 0;
}

extern TYPELIB binlog_checksum_typelib;

void Binlog_sender::init_checksum_alg()
{
  DBUG_ENTER("init_binlog_checksum");

  LEX_STRING name= {C_STRING_WITH_LEN("master_binlog_checksum")};
  user_var_entry *entry;

  m_slave_checksum_alg= BINLOG_CHECKSUM_ALG_UNDEF;
  entry= (user_var_entry*) my_hash_search(&m_thd->user_vars,
                                          (uchar*) name.str, name.length);
  if (entry)
  {
    m_slave_checksum_alg=
      find_type((char*) entry->ptr(), &binlog_checksum_typelib, 1) - 1;
    DBUG_ASSERT(m_slave_checksum_alg < BINLOG_CHECKSUM_ALG_ENUM_END);
  }

  /*
    m_event_checksum_alg should be set to the checksum algorithm in
    Format_description_log_event. But it is used by fake_rotate_event() which
    will be called before reading any Format_description_log_event. In that case,
    m_slave_checksum_alg is set as the value of m_event_checksum_alg.
  */
  m_event_checksum_alg= m_slave_checksum_alg;
  DBUG_VOID_RETURN;
}

int Binlog_sender::fake_rotate_event(String *packet, const char *next_log_file,
                                     my_off_t log_pos)
{
  DBUG_ENTER("fake_rotate_event");
  DBUG_ASSERT(packet != NULL);

  const char* p = next_log_file + dirname_length(next_log_file);
  ulong ident_len = strlen(p);
  ulong event_len = ident_len + LOG_EVENT_HEADER_LEN + ROTATE_HEADER_LEN +
    (event_checksum_on() ? BINLOG_CHECKSUM_LEN : 0);

  /* reset transmit packet for the fake rotate event below */
  if (reset_transmit_packet(packet, 0))
    DBUG_RETURN(1);

  uint32 event_offset= packet->length();

  packet->realloc(event_len + event_offset);
  packet->length(event_len + event_offset);

  uchar *header= (uchar *)packet->ptr() + event_offset;
  uchar *rotate_header= header + LOG_EVENT_HEADER_LEN;
  /*
    'when' (the timestamp) is set to 0 so that slave could distinguish between
    real and fake Rotate events (if necessary)
  */
  int4store(header, 0);
  header[EVENT_TYPE_OFFSET] = ROTATE_EVENT;
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int4store(header + LOG_POS_OFFSET, 0);
  int2store(header + FLAGS_OFFSET, LOG_EVENT_ARTIFICIAL_F);

  int8store(rotate_header, log_pos);
  memcpy(rotate_header + ROTATE_HEADER_LEN, p, ident_len);

  if (event_checksum_on())
    calc_event_checksum(header, event_len);

  DBUG_RETURN(send_packet(packet));
}

inline void Binlog_sender::calc_event_checksum(uchar *event_ptr, uint32 event_len)
{
  ha_checksum crc= my_checksum(0L, NULL, 0);
  crc= my_checksum(crc, event_ptr, event_len - BINLOG_CHECKSUM_LEN);
  int4store(event_ptr + event_len - BINLOG_CHECKSUM_LEN, crc);
}

inline int Binlog_sender::reset_transmit_packet(String *packet, ushort flags)
{
  /* reserve and set default header */
  packet->length(0);
  /*
    set() will free the original memory. It causes dump thread to free and
    reallocate memory for each sending event. It consumes a little bit more CPU
    resource. TODO: Use a shared send buffer to eliminate memory reallocating.
  */
  packet->set("\0", 1, &my_charset_bin);

  if (RUN_HOOK(binlog_transmit, reserve_header, (m_thd, flags, packet)))
  {
    set_unknow_error("Failed to run hook 'reserve_header'");
    return 1;
  }
  return 0;
}

int Binlog_sender::send_format_description_event(String *packet,
                                                 IO_CACHE *log_cache,
                                                 bool clear_log_pos)
{
  DBUG_ENTER("Binlog_sender::send_format_description_event");
  uchar* event_ptr;
  uint32 event_len;

  if (read_event(log_cache, BINLOG_CHECKSUM_ALG_OFF, &event_ptr, &event_len))
    DBUG_RETURN(1);

  DBUG_PRINT("info",
             ("Looked for a Format_description_log_event, found event type %s",
              Log_event::get_type_str((Log_event_type) event_ptr[EVENT_TYPE_OFFSET])));

  if (event_ptr[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT)
  {
    set_fatal_error("Could not find format_description_event in binlog file");
    DBUG_RETURN(1);
  }

  m_event_checksum_alg= get_checksum_alg((const char *)event_ptr, event_len);

  DBUG_ASSERT(m_event_checksum_alg < BINLOG_CHECKSUM_ALG_ENUM_END ||
              m_event_checksum_alg == BINLOG_CHECKSUM_ALG_UNDEF);

  /* Slave does not support checksum, but binary events include checksum */
  if (m_slave_checksum_alg == BINLOG_CHECKSUM_ALG_UNDEF &&
      event_checksum_on())
  {
    set_fatal_error("Slave can not handle replication events with the "
                    "checksum that master is configured to log");

    sql_print_warning("Master is configured to log replication events "
                      "with checksum, but will not send such events to "
                      "slaves that cannot process them");
    DBUG_RETURN(1);
  }

  event_ptr[FLAGS_OFFSET] &= ~LOG_EVENT_BINLOG_IN_USE_F;

  /*
    Mark that this event with "log_pos=0", so the slave should not increment
    master's binlog position (rli->group_master_log_pos).
  */
  if (clear_log_pos)
  {
    int4store(event_ptr + LOG_POS_OFFSET, 0);
    int4store(event_ptr + LOG_EVENT_MINIMAL_HEADER_LEN + ST_CREATED_OFFSET, 0);
    /* fix the checksum due to latest changes in header */
    if (event_checksum_on())
      calc_event_checksum(event_ptr, event_len);
  }

  DBUG_RETURN(send_packet(packet));
}

int Binlog_sender::has_previous_gtid_log_event(IO_CACHE *log_cache,
                                               bool *found)
{
  uchar buf[LOG_EVENT_HEADER_LEN];
  *found= false;

  /* It is possible there is only format_description_log_event in the file. */
  if (my_b_tell(log_cache) < my_b_filelength(log_cache))
  {
    if (my_b_read(log_cache, buf, LOG_EVENT_HEADER_LEN) != 0)
    {
      set_fatal_error(log_read_error_msg(LOG_READ_IO));
      return 1;
    }
    *found= (buf[EVENT_TYPE_OFFSET] == PREVIOUS_GTIDS_LOG_EVENT);
  }
  return 0;
}

const char* Binlog_sender::log_read_error_msg(int error)
{
  switch (error) {
  case LOG_READ_BOGUS:
    return "bogus data in log event";
  case LOG_READ_TOO_LARGE:
    return "log event entry exceeded max_allowed_packet; Increase "
      "max_allowed_packet on master";
  case LOG_READ_IO:
    return "I/O error reading log event";
  case LOG_READ_MEM:
    return "memory allocation failed reading log event";
  case LOG_READ_TRUNC:
    return "binlog truncated in the middle of event; consider out of disk space on master";
  case LOG_READ_CHECKSUM_FAILURE:
    return "event read from binlog did not pass crc check";
  default:
    return "unknown error reading log event on the master";
  }
}

inline int Binlog_sender::read_event(IO_CACHE *log_cache, uint8 checksum_alg,
                                     uchar **event_ptr, uint32 *event_len)
{
  DBUG_ENTER("Binlog_sender::read_event");

  uint32 event_offset;
  String *packet= &m_thd->packet;

  if (reset_transmit_packet(packet, 0))
    DBUG_RETURN(1);

  DBUG_EXECUTE_IF("dump_thread_before_read_event",
                  {
                    const char act[]= "now wait_for signal.continue no_clear_event";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  event_offset= packet->length();

  int error= Log_event::read_log_event(log_cache, packet, NULL, checksum_alg);
  if (error != 0)
  {
    /*
      In theory, it should never happen. But RESET MASTER deletes binlog file
      directly without checking if there is any dump thread working.
    */
    error= (error == LOG_READ_EOF) ? LOG_READ_IO : error;
    set_fatal_error(log_read_error_msg(error));
    DBUG_RETURN(1);
  }

  set_last_pos(my_b_tell(log_cache));
  /*
    The pointer must be set after read_log_event(), cause packet->ptr() may has
    been reallocated when reading the event.
  */
  *event_ptr= (uchar *)packet->ptr() + event_offset;
  *event_len= packet->length() - event_offset;

  DBUG_PRINT("info",
             ("Read event %s",
              Log_event::get_type_str(Log_event_type
                                      ((*event_ptr)[EVENT_TYPE_OFFSET]))));
#ifndef DBUG_OFF
  if (check_event_count())
    DBUG_RETURN(1);
#endif
  DBUG_RETURN(0);
}

int Binlog_sender::send_heartbeat_event(String* packet, my_off_t log_pos)
{
  DBUG_ENTER("send_heartbeat_event");
  String tmp;
  const char* filename= m_linfo.log_file_name;

  if (packet == NULL)
    packet= &tmp;

  const char* p= filename + dirname_length(filename);
  uint32 ident_len= (uint32) strlen(p);
  uint32 event_len= ident_len + LOG_EVENT_HEADER_LEN +
    (event_checksum_on() ? BINLOG_CHECKSUM_LEN : 0);

  DBUG_PRINT("info", ("log_file_name %s, log_pos %llu", p, log_pos));

  if (reset_transmit_packet(packet, 0))
    DBUG_RETURN(1);

  uint32 event_offset= packet->length();

  packet->realloc(event_len + event_offset);
  packet->length(event_len + event_offset);

  uchar *header= (uchar *)packet->ptr() + event_offset;

  /* Timestamp field */
  int4store(header, 0);
  header[EVENT_TYPE_OFFSET] = HEARTBEAT_LOG_EVENT;
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int4store(header + LOG_POS_OFFSET, log_pos);
  int2store(header + FLAGS_OFFSET, 0);
  memcpy(header + LOG_EVENT_HEADER_LEN, p, ident_len);

  if (event_checksum_on())
    calc_event_checksum(header, event_len);

  DBUG_RETURN(send_packet_and_flush(packet));
}

inline int Binlog_sender::flush_net()
{
  if (DBUG_EVALUATE_IF("simulate_flush_error", 1, net_flush(&m_thd->net)))
  {
    set_unknow_error("failed on flush_net()");
    return 1;
  }
  return 0;
}

inline int Binlog_sender::send_packet(String *packet)
{
  if (DBUG_EVALUATE_IF("simulate_send_error", true,
                       my_net_write(&m_thd->net, (uchar*) packet->ptr(),
                                    packet->length())))
  {
    set_unknow_error("Failed on my_net_write()");
    return 1;
  }
  return 0;
}

inline int Binlog_sender::send_packet_and_flush(String *packet)
{
  return (send_packet(packet) || flush_net());
}

inline int Binlog_sender::before_send_hook(String *packet,
                                           const char *log_file,
                                           my_off_t log_pos)
{
  if (RUN_HOOK(binlog_transmit, before_send_event,
               (m_thd, 0/*flags*/, packet, log_file, log_pos)))
  {
    set_unknow_error("run 'before_send_event' hook failed");
    return 1;
  }
  return 0;
}

inline int Binlog_sender::after_send_hook(String *packet,
                                          const char *log_file,
                                          my_off_t log_pos)
{
  if (RUN_HOOK(binlog_transmit, after_send_event,
               (m_thd, 0/*flags*/, packet, log_file, log_pos)))
  {
    set_unknow_error("Failed to run hook 'after_send_event'");
    return 1;
  }

  /*
    semisync after_send_event hook doesn't return and error when net error
    happens.
  */
  if (m_thd->net.error != 0)
  {
    set_unknow_error("Found net error");
    return 1;
  }
  return 0;
}

#ifndef DBUG_OFF
extern int max_binlog_dump_events;

inline int Binlog_sender::check_event_count()
{
  if (max_binlog_dump_events != 0 &&
      (++m_event_count > max_binlog_dump_events))
  {
    set_unknow_error("Debugging binlog dump abort");
    return 1;
  }
  return 0;
}
#endif

#endif // HAVE_REPLICATION
