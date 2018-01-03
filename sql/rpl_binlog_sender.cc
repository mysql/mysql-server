/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_binlog_sender.h"

#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <utility>

#include "lex_string.h"
#include "m_string.h"
#include "map_helpers.h"
#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "my_systime.h"
#include "my_thread.h"
#include "mysql/components/services/psi_stage_bits.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_my_snprintf.h"
#include "sql/debug_sync.h"          // debug_sync_set_action
#include "sql/derror.h"              // ER_THD
#include "sql/item_func.h"           // user_var_entry
#include "sql/log.h"
#include "sql/log_event.h"           // MAX_MAX_ALLOWED_PACKET
#include "sql/mdl.h"
#include "sql/mysqld.h"              // global_system_variables ...
#include "sql/protocol.h"
#include "sql/protocol_classic.h"
#include "sql/rpl_constants.h"       // BINLOG_DUMP_NON_BLOCK
#include "sql/rpl_gtid.h"
#include "sql/rpl_handler.h"         // RUN_HOOK
#include "sql/rpl_master.h"          // opt_sporadic_binlog_dump_fail
#include "sql/rpl_reporting.h"       // MAX_SLAVE_ERRMSG
#include "sql/sql_class.h"           // THD
#include "sql/system_variables.h"
#include "typelib.h"

#ifndef DBUG_OFF
  static uint binlog_dump_count= 0;
#endif
using binary_log::checksum_crc32;

const uint32 Binlog_sender::PACKET_MIN_SIZE= 4096;
const uint32 Binlog_sender::PACKET_MAX_SIZE= UINT_MAX32;
const ushort Binlog_sender::PACKET_SHRINK_COUNTER_THRESHOLD= 100;
const float Binlog_sender::PACKET_GROW_FACTOR= 2.0;
const float Binlog_sender::PACKET_SHRINK_FACTOR= 0.5;

Binlog_sender::Binlog_sender(THD *thd, const char *start_file,
                             my_off_t start_pos,
                             Gtid_set *exclude_gtids, uint32 flag)
  : m_thd(thd),
    m_packet(*thd->get_protocol_classic()->get_output_packet()),
    m_start_file(start_file),
    m_start_pos(start_pos), m_exclude_gtid(exclude_gtids),
    m_using_gtid_protocol(exclude_gtids != NULL),
    m_check_previous_gtid_event(exclude_gtids != NULL),
    m_gtid_clear_fd_created_flag(exclude_gtids == NULL),
    m_diag_area(false),
    m_errmsg(NULL), m_errno(0), m_last_file(NULL), m_last_pos(0),
    m_half_buffer_size_req_counter(0), m_new_shrink_size(PACKET_MIN_SIZE),
    m_flag(flag), m_observe_transmission(false), m_transmit_started(false)
  {}

void Binlog_sender::init()
{
  DBUG_ENTER("Binlog_sender::init");
  THD *thd= m_thd;

  thd->push_diagnostics_area(&m_diag_area);
  init_heartbeat_period();
  m_last_event_sent_ts= time(0);

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->current_linfo= &m_linfo;
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  /* Initialize the buffer only once. */
  m_packet.mem_realloc(PACKET_MIN_SIZE); // size of the buffer
  m_new_shrink_size= PACKET_MIN_SIZE;
  DBUG_PRINT("info", ("Initial packet->alloced_length: %zu",
                      m_packet.alloced_length()));

  if (!mysql_bin_log.is_open())
  {
    set_fatal_error("Binary log is not open");
    DBUG_VOID_RETURN;
  }

  if (DBUG_EVALUATE_IF("simulate_no_server_id", true, server_id == 0))
  {
    set_fatal_error("Misconfigured master - master server_id is 0");
    DBUG_VOID_RETURN;
  }

  if (m_using_gtid_protocol)
  {
    enum_gtid_mode gtid_mode= get_gtid_mode_from_copy(GTID_MODE_LOCK_NONE);
    if (gtid_mode != GTID_MODE_ON)
    {
      char buf[MYSQL_ERRMSG_SIZE];
      sprintf(buf, "The replication sender thread cannot start in "
              "AUTO_POSITION mode: this server has GTID_MODE = %.192s "
              "instead of ON.", get_gtid_mode_string(gtid_mode));
      set_fatal_error(buf);
      DBUG_VOID_RETURN;
    }
  }

  if (check_start_file())
    DBUG_VOID_RETURN;

  LogErr(INFORMATION_LEVEL, ER_RPL_BINLOG_STARTING_DUMP,
         thd->thread_id(), thd->server_id,
         m_start_file, m_start_pos);

  if (RUN_HOOK(binlog_transmit, transmit_start,
               (thd, m_flag, m_start_file, m_start_pos,
                &m_observe_transmission)))
  {
    set_unknown_error("Failed to run hook 'transmit_start'");
    DBUG_VOID_RETURN;
  }
  m_transmit_started=true;

  init_checksum_alg();
  /*
    There are two ways to tell the server to not block:

    - Set the BINLOG_DUMP_NON_BLOCK flag.
      This is official, documented, not used by any mysql
      client, but used by some external users.

    - Set server_id=0.
      This is unofficial, undocumented, and used by
      mysqlbinlog -R since the beginning of time.

    When mysqlbinlog --stop-never is used, it sets a 'fake'
    server_id that defaults to 1 but can be set to anything
    else using stop-never-slave-server-id. This has the
    drawback that if the server_id conflicts with any other
    running slave, or with any other instance of mysqlbinlog
    --stop-never, then that other instance will be killed.  It
    is also an unnecessary burden on the user to have to
    specify a server_id different from all other server_ids
    just to avoid conflicts.

    As of MySQL 5.6.20 and 5.7.5, mysqlbinlog redundantly sets
    the BINLOG_DUMP_NONBLOCK flag when one or both of the
    following holds:
    - the --stop-never option is *not* specified

    In a far future, this means we can remove the unofficial
    functionality that server_id=0 implies nonblocking
    behavior. That will allow mysqlbinlog to use server_id=0
    always. That has the advantage that mysqlbinlog
    --stop-never cannot cause any running dump threads to be
    killed.
  */
  m_wait_new_events= !((thd->server_id == 0) ||
                       ((m_flag & BINLOG_DUMP_NON_BLOCK) != 0));
  /* Binary event can be vary large. So set it to max allowed packet. */
  thd->variables.max_allowed_packet= MAX_MAX_ALLOWED_PACKET;

#ifndef DBUG_OFF
  if (opt_sporadic_binlog_dump_fail && (binlog_dump_count++ % 2))
    set_unknown_error("Master fails in COM_BINLOG_DUMP because of "
                     "--sporadic-binlog-dump-fail");
  m_event_count= 0;
#endif
  DBUG_VOID_RETURN;
}

void Binlog_sender::cleanup()
{
  DBUG_ENTER("Binlog_sender::cleanup");

  THD *thd= m_thd;

  if (m_transmit_started)
    (void) RUN_HOOK(binlog_transmit, transmit_stop, (thd, m_flag));

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->current_linfo= NULL;
  mysql_mutex_unlock(&thd->LOCK_thd_data);

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
  bool is_index_file_reopened_on_binlog_disable= false;

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
    if (unlikely(fake_rotate_event(log_file, start_pos)))
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
    DBUG_EXECUTE_IF("waiting_for_disable_binlog",
		    {
		    const char act[]= "now "
		    "signal dump_thread_reached_wait_point "
		    "wait_for continue_dump_thread no_clear_event";
		    DBUG_ASSERT(!debug_sync_set_action(m_thd,
						       STRING_WITH_LEN(act)));
		    };);
    mysql_bin_log.lock_index();
    if (!mysql_bin_log.is_open())
    {
      if (mysql_bin_log.open_index_file(mysql_bin_log.get_index_fname(),
					log_file, FALSE))
      {
        set_fatal_error("Binary log is not open and failed to open index file "
                        "to retrieve next file.");
        mysql_bin_log.unlock_index();
        break;
      }
      is_index_file_reopened_on_binlog_disable= true;
    }
    int error= mysql_bin_log.find_next_log(&m_linfo, 0);
    mysql_bin_log.unlock_index();
    if (unlikely(error))
    {
      DBUG_EXECUTE_IF("waiting_for_disable_binlog",
		      {
		      const char act[]= "now signal consumed_binlog";
		      DBUG_ASSERT(!debug_sync_set_action(m_thd,
							 STRING_WITH_LEN(act)));
		      };);
      if (is_index_file_reopened_on_binlog_disable)
        mysql_bin_log.close(LOG_CLOSE_INDEX, true/*need_lock_log=true*/,
                            true/*need_lock_index=true*/);
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

  /*
    If the dump thread was killed because of a duplicate slave UUID we
    will fail throwing an error to the slave so it will not try to
    reconnect anymore.
  */
  mysql_mutex_lock(&m_thd->LOCK_thd_data);
  bool was_killed_by_duplicate_slave_id= m_thd->duplicate_slave_id;
  mysql_mutex_unlock(&m_thd->LOCK_thd_data);
  if (was_killed_by_duplicate_slave_id)
    set_fatal_error("A slave with the same server_uuid/server_id as this slave "
                    "has connected to the master");

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

my_off_t Binlog_sender::send_binlog(IO_CACHE *log_cache, my_off_t start_pos)
{
  if (unlikely(send_format_description_event(log_cache, start_pos)))
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

    m_thd->killed.store(DBUG_EVALUATE_IF("simulate_kill_dump",
                                         THD::KILL_CONNECTION,
                                         m_thd->killed.load()));

    DBUG_EXECUTE_IF("wait_after_binlog_EOF",
                    {
                      const char act[]= "now wait_for signal.rotate_finished no_clear_event";
                      DBUG_ASSERT(!debug_sync_set_action(m_thd,
                                                         STRING_WITH_LEN(act)));
                    };);
  }
  return 1;
}

inline my_off_t Binlog_sender::get_binlog_end_pos(IO_CACHE *log_cache)
{
  DBUG_ENTER("Binlog_sender::get_binlog_end_pos()");
  my_off_t read_pos= my_b_tell(log_cache);
  my_off_t end_pos= 0;

  do
  {
    /*
      MYSQL_BIN_LOG::binlog_end_pos is atomic. We should only acquire the
      LOCK_binlog_end_pos if we reached the end of the hot log and are going
      to wait for updates on the binary log (Binlog_sender::wait_new_event()).
    */
    end_pos= mysql_bin_log.get_binlog_end_pos();

    /* If this is a cold binlog file, we are done getting the end pos */
    if (unlikely(!mysql_bin_log.is_active(m_linfo.log_file_name)))
    {
      end_pos= my_b_filelength(log_cache);
      if (read_pos == end_pos)
        DBUG_RETURN(0); // Arrived the end of inactive file
      else
        DBUG_RETURN(end_pos);
    }

    DBUG_PRINT("info", ("Reading file %s, seek pos %llu, end_pos is %llu",
                        m_linfo.log_file_name, read_pos, end_pos));
    DBUG_PRINT("info", ("Active file is %s", mysql_bin_log.get_log_fname()));

    if (read_pos < end_pos)
      DBUG_RETURN(end_pos);

    /* Some data may be in net buffer, it should be flushed before waiting */
    if (!m_wait_new_events || flush_net())
      DBUG_RETURN(1);

    if (unlikely(wait_new_events(read_pos)))
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
    uchar* event_ptr= nullptr;
    uint32 event_len= 0;

    if (unlikely(thd->killed))
        DBUG_RETURN(1);

    if (unlikely(read_event(log_cache, m_event_checksum_alg,
                            &event_ptr, &event_len)))
      DBUG_RETURN(1);

    Log_event_type event_type= (Log_event_type)event_ptr[EVENT_TYPE_OFFSET];
    if (unlikely(check_event_type(event_type, log_file, log_pos)))
      DBUG_RETURN(1);

    DBUG_EXECUTE_IF("dump_thread_wait_before_send_xid",
                    {
                      if (event_type == binary_log::XID_EVENT)
                      {
                        thd->get_protocol()->flush();
                        const char act[]=
                          "now "
                          "wait_for signal.continue";
                        DBUG_ASSERT(opt_debug_sync_timeout > 0);
                        DBUG_ASSERT(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
                      }
                    });

    log_pos= my_b_tell(log_cache);

    if (before_send_hook(log_file, log_pos))
      DBUG_RETURN(1);
    /*
      TODO: Set m_exclude_gtid to NULL if all gtids in m_exclude_gtid has
      be skipped. and maybe removing the gtid from m_exclude_gtid will make
      skip_event has better performance.
    */
    if (m_exclude_gtid && (in_exclude_group= skip_event(event_ptr, event_len,
                                                        in_exclude_group)))
    {
      /*
        If we have not send any event from past 'heartbeat_period' time
        period, then it is time to send a packet before skipping this group.
       */
      DBUG_EXECUTE_IF("inject_2sec_sleep_when_skipping_an_event",
                      {
                      my_sleep(2000000);
                      });
      time_t now= time(0);
      DBUG_ASSERT(now >= m_last_event_sent_ts);
      bool time_for_hb_event= ((ulonglong)(now - m_last_event_sent_ts)
                          >= (ulonglong)(m_heartbeat_period/1000000000UL));
      if (time_for_hb_event)
      {
        if (unlikely(send_heartbeat_event(log_pos)))
          DBUG_RETURN(1);
        exclude_group_end_pos= 0;
      }
      else
      {
        exclude_group_end_pos= log_pos;
      }
      DBUG_PRINT("info", ("Event of type %s is skipped",
                          Log_event::get_type_str(event_type)));
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
        /* Save a copy of the buffer content. */
        String tmp;
        tmp.copy(m_packet);
        tmp.length(m_packet.length());

        if (unlikely(send_heartbeat_event(exclude_group_end_pos)))
          DBUG_RETURN(1);
        exclude_group_end_pos= 0;

        /* Restore the copy back. */
        m_packet.copy(tmp);
        m_packet.length(tmp.length());
      }

      if (unlikely(send_packet()))
        DBUG_RETURN(1);
    }

    if (unlikely(after_send_hook(log_file, in_exclude_group ? log_pos : 0)))
      DBUG_RETURN(1);
  }

  /*
    A heartbeat is needed before waiting for more events, if some
    events are skipped. This is needed so that the slave can increase
    master_log_pos correctly.
  */
  if (unlikely(in_exclude_group))
  {
    if (send_heartbeat_event(log_pos))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


bool Binlog_sender::check_event_type(Log_event_type type,
                                     const char *log_file, my_off_t log_pos)
{
  if (type == binary_log::ANONYMOUS_GTID_LOG_EVENT)
  {
    /*
      Normally, there will not be any anonymous events when
      auto_position is enabled, since both the master and the slave
      refuse to connect if the master is not using GTID_MODE=ON.
      However, if the master changes GTID_MODE after the connection
      was initialized, or if the slave requests to replicate
      transactions that appear before the last anonymous event, then
      this can happen. Then we generate this error to prevent sending
      anonymous transactions to the slave.
    */
    if (m_using_gtid_protocol)
    {
      DBUG_EXECUTE_IF("skip_sender_anon_autoposition_error",
                      {
                        return false;
                      };);
      char buf[MYSQL_ERRMSG_SIZE];
      sprintf(buf,
              ER_THD(m_thd, ER_CANT_REPLICATE_ANONYMOUS_WITH_AUTO_POSITION),
              log_file, log_pos);
      set_fatal_error(buf);
      return true;
    }
    /*
      Normally, there will not be any anonymous events when master has
      GTID_MODE=ON, since anonymous events are not generated when
      GTID_MODE=ON.  However, this can happen if the master changes
      GTID_MODE to ON when the slave has not yet replicated all
      anonymous transactions.
    */
    else if (get_gtid_mode_from_copy(GTID_MODE_LOCK_NONE) == GTID_MODE_ON)
    {
      char buf[MYSQL_ERRMSG_SIZE];
      sprintf(buf,
              ER_THD(m_thd, ER_CANT_REPLICATE_ANONYMOUS_WITH_GTID_MODE_ON),
              log_file, log_pos);
      set_fatal_error(buf);
      return true;
    }
  }
  else if (type == binary_log::GTID_LOG_EVENT)
  {
    /*
      Normally, there will not be any GTID events when master has
      GTID_MODE=OFF, since GTID events are not generated when
      GTID_MODE=OFF.  However, this can happen if the master changes
      GTID_MODE to OFF when the slave has not yet replicated all GTID
      transactions.
    */
    if (get_gtid_mode_from_copy(GTID_MODE_LOCK_NONE) == GTID_MODE_OFF)
    {
      char buf[MYSQL_ERRMSG_SIZE];
      sprintf(buf,
              ER_THD(m_thd, ER_CANT_REPLICATE_GTID_WITH_GTID_MODE_OFF),
              log_file, log_pos);
      set_fatal_error(buf);
      return true;
    }
  }
  return false;
}


inline bool Binlog_sender::skip_event(const uchar *event_ptr, uint32 event_len,
                                      bool in_exclude_group)
{
  DBUG_ENTER("Binlog_sender::skip_event");

  uint8 event_type= (Log_event_type) event_ptr[LOG_EVENT_OFFSET];
  switch (event_type)
  {
  case binary_log::GTID_LOG_EVENT:
    {
      Format_description_log_event fd_ev;
      fd_ev.common_footer->checksum_alg= m_event_checksum_alg;
      Gtid_log_event gtid_ev((const char *)event_ptr, event_checksum_on() ?
                             event_len - BINLOG_CHECKSUM_LEN : event_len,
                             &fd_ev);
      Gtid gtid;
      gtid.sidno= gtid_ev.get_sidno(m_exclude_gtid->get_sid_map());
      gtid.gno= gtid_ev.get_gno();
      DBUG_RETURN(m_exclude_gtid->contains_gtid(gtid));
    }
  case binary_log::ROTATE_EVENT:
    DBUG_RETURN(false);
  }
  DBUG_RETURN(in_exclude_group);
}

int Binlog_sender::wait_new_events(my_off_t log_pos)
{
  int ret= 0;
  PSI_stage_info old_stage;

  mysql_bin_log.lock_binlog_end_pos();
  /*
    If the binary log was updated before reaching this waiting point,
    there is no need to wait.
  */
  if (mysql_bin_log.get_binlog_end_pos() > log_pos ||
      !mysql_bin_log.is_active(m_linfo.log_file_name))
  {
    mysql_bin_log.unlock_binlog_end_pos();
    return ret;
  }

  m_thd->ENTER_COND(mysql_bin_log.get_log_cond(),
                    mysql_bin_log.get_binlog_end_pos_lock(),
                    &stage_master_has_sent_all_binlog_to_slave,
                    &old_stage);

  if (m_heartbeat_period)
    ret= wait_with_heartbeat(log_pos);
  else
    ret= wait_without_heartbeat();

  mysql_bin_log.unlock_binlog_end_pos();
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
    set_timespec_nsec(&ts, m_heartbeat_period);
    ret= mysql_bin_log.wait_for_update(&ts);
    if (!is_timeout(ret))
      break;

#ifndef DBUG_OFF
      if (hb_info_counter < 3)
      {
        LogErr(INFORMATION_LEVEL, ER_RPL_BINLOG_MASTER_SENDS_HEARTBEAT);
        hb_info_counter++;
        if (hb_info_counter == 3)
          LogErr(INFORMATION_LEVEL,
                 ER_RPL_BINLOG_SKIPPING_REMAINING_HEARTBEAT_INFO);
      }
#endif
      if (send_heartbeat_event(log_pos))
        return 1;
  } while (!m_thd->killed);

  return ret ? 1 : 0;
}

inline int Binlog_sender::wait_without_heartbeat()
{
  return mysql_bin_log.wait_for_update(NULL);
}

void Binlog_sender::init_heartbeat_period()
{
  bool null_value;
  LEX_STRING name=  { C_STRING_WITH_LEN("master_heartbeat_period")};

  /* Protects m_thd->user_vars. */
  mysql_mutex_lock(&m_thd->LOCK_thd_data);

  const auto it= m_thd->user_vars.find(to_string(name));
  if (it == m_thd->user_vars.end())
     m_heartbeat_period= 0;
  else
     m_heartbeat_period= it->second->val_int(&null_value);

  mysql_mutex_unlock(&m_thd->LOCK_thd_data);
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
    /*
      In normal scenarios, it is not possible that Slave will
      contain more gtids than Master with resepctive to Master's
      UUID. But it could be possible case if Master's binary log
      is truncated(due to raid failure) or Master's binary log is
      deleted but GTID_PURGED was not set properly. That scenario
      needs to be validated, i.e., it should *always* be the case that
      Slave's gtid executed set (+retrieved set) is a subset of
      Master's gtid executed set with respective to Master's UUID.
      If it happens, dump thread will be stopped during the handshake
      with Slave (thus the Slave's I/O thread will be stopped with the
      error. Otherwise, it can lead to data inconsistency between Master
      and Slave.
    */
    Sid_map* slave_sid_map= m_exclude_gtid->get_sid_map();
    DBUG_ASSERT(slave_sid_map);
    global_sid_lock->wrlock();
    const rpl_sid &server_sid= gtid_state->get_server_sid();
    rpl_sidno subset_sidno= slave_sid_map->sid_to_sidno(server_sid);
    Gtid_set
      gtid_executed_and_owned(gtid_state->get_executed_gtids()->get_sid_map());

    // gtids = executed_gtids & owned_gtids
    if (gtid_executed_and_owned.add_gtid_set(gtid_state->get_executed_gtids())
        != RETURN_STATUS_OK)
    {
      DBUG_ASSERT(0);
    }
    gtid_state->get_owned_gtids()->get_gtids(gtid_executed_and_owned);

    if (!m_exclude_gtid->is_subset_for_sid(&gtid_executed_and_owned,
                                           gtid_state->get_server_sidno(),
                                           subset_sidno))
    {
      errmsg= ER_THD(m_thd, ER_SLAVE_HAS_MORE_GTIDS_THAN_MASTER);
      global_sid_lock->unlock();
      set_fatal_error(errmsg);
      return 1;
    }
    /*
      Setting GTID_PURGED (when GTID_EXECUTED set is empty i.e., when
      previous_gtids are also empty) will make binlog rotate. That
      leaves first binary log with empty previous_gtids and second
      binary log's previous_gtids with the value of gtid_purged.
      In find_first_log_not_in_gtid_set() while we search for a binary
      log whose previous_gtid_set is subset of slave_gtid_executed,
      in this particular case, server will always find the first binary
      log with empty previous_gtids which is subset of any given
      slave_gtid_executed. Thus Master thinks that it found the first
      binary log which is actually not correct and unable to catch
      this error situation. Hence adding below extra if condition
      to check the situation. Slave should know about Master's purged GTIDs.
      If Slave's GTID executed + retrieved set does not contain Master's
      complete purged GTID list, that means Slave is requesting(expecting)
      GTIDs which were purged by Master. We should let Slave know about the
      situation. i.e., throw error if slave's GTID executed set is not
      a superset of Master's purged GTID set.
      The other case, where user deleted binary logs manually
      (without using 'PURGE BINARY LOGS' command) but gtid_purged
      is not set by the user, the following if condition cannot catch it.
      But that is not a problem because in find_first_log_not_in_gtid_set()
      while checking for subset previous_gtids binary log, the logic
      will not find one and an error ER_MASTER_HAS_PURGED_REQUIRED_GTIDS
      is thrown from there.
    */
    if (!gtid_state->get_lost_gtids()->is_subset(m_exclude_gtid))
    {
      errmsg= ER_THD(m_thd, ER_MASTER_HAS_PURGED_REQUIRED_GTIDS);
      global_sid_lock->unlock();
      set_fatal_error(errmsg);
      return 1;
    }
    global_sid_lock->unlock();
    Gtid first_gtid= {0, 0};
    if (mysql_bin_log.find_first_log_not_in_gtid_set(index_entry_name,
                                                     m_exclude_gtid,
                                                     &first_gtid,
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
    /*
      If we are skipping at least the first transaction of the binlog,
      we must clear the "created" field of the FD event (set it to 0)
      to avoid cleaning up temp tables on slave.
    */
    m_gtid_clear_fd_created_flag= (first_gtid.sidno >= 1 &&
                                   first_gtid.gno >= 1 &&
                                   m_exclude_gtid->contains_gtid(first_gtid));
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

  m_slave_checksum_alg= binary_log::BINLOG_CHECKSUM_ALG_UNDEF;

  /* Protects m_thd->user_vars. */
  mysql_mutex_lock(&m_thd->LOCK_thd_data);

  const auto it= m_thd->user_vars.find("master_binlog_checksum");
  if (it != m_thd->user_vars.end())
  {
    m_slave_checksum_alg=
      static_cast<enum_binlog_checksum_alg>(
        find_type((char*) it->second->ptr(), &binlog_checksum_typelib, 1) - 1);
    DBUG_ASSERT(m_slave_checksum_alg < binary_log::BINLOG_CHECKSUM_ALG_ENUM_END);
  }

  mysql_mutex_unlock(&m_thd->LOCK_thd_data);

  /*
    m_event_checksum_alg should be set to the checksum algorithm in
    Format_description_log_event. But it is used by fake_rotate_event() which
    will be called before reading any Format_description_log_event. In that case,
    m_slave_checksum_alg is set as the value of m_event_checksum_alg.
  */
  m_event_checksum_alg= m_slave_checksum_alg;
  DBUG_VOID_RETURN;
}

int Binlog_sender::fake_rotate_event(const char *next_log_file,
                                     my_off_t log_pos)
{
  DBUG_ENTER("fake_rotate_event");
  const char* p = next_log_file + dirname_length(next_log_file);
  size_t ident_len = strlen(p);
  size_t event_len = ident_len + LOG_EVENT_HEADER_LEN + Binary_log_event::ROTATE_HEADER_LEN +
    (event_checksum_on() ? BINLOG_CHECKSUM_LEN : 0);

  /* reset transmit packet for the fake rotate event below */
  if (reset_transmit_packet(0, event_len))
    DBUG_RETURN(1);

  size_t event_offset= m_packet.length();
  m_packet.length(event_len + event_offset);
  uchar *header= (uchar *)m_packet.ptr() + event_offset;
  uchar *rotate_header= header + LOG_EVENT_HEADER_LEN;
  /*
    'when' (the timestamp) is set to 0 so that slave could distinguish between
    real and fake Rotate events (if necessary)
  */
  int4store(header, 0);
  header[EVENT_TYPE_OFFSET] = binary_log::ROTATE_EVENT;
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, static_cast<uint32>(event_len));
  int4store(header + LOG_POS_OFFSET, 0);
  int2store(header + FLAGS_OFFSET, LOG_EVENT_ARTIFICIAL_F);

  int8store(rotate_header, log_pos);
  memcpy(rotate_header + Binary_log_event::ROTATE_HEADER_LEN, p, ident_len);

  if (event_checksum_on())
    calc_event_checksum(header, event_len);

  DBUG_RETURN(send_packet());
}

inline void Binlog_sender::calc_event_checksum(uchar *event_ptr, size_t event_len)
{
  ha_checksum crc= checksum_crc32(0L, NULL, 0);
  crc= checksum_crc32(crc, event_ptr, event_len - BINLOG_CHECKSUM_LEN);
  int4store(event_ptr + event_len - BINLOG_CHECKSUM_LEN, crc);
}

inline int Binlog_sender::reset_transmit_packet(ushort flags, size_t event_len)
{
  DBUG_ENTER("Binlog_sender::reset_transmit_packet");
  DBUG_PRINT("info", ("event_len: %zu, m_packet->alloced_length: %zu",
                      event_len, m_packet.alloced_length()));
  DBUG_ASSERT(m_packet.alloced_length() >= PACKET_MIN_SIZE);

  m_packet.length(0);  // size of the content
  m_packet.qs_append('\0'); // Set this as an OK packet

  /* reserve and set default header */
  if (m_observe_transmission &&
      RUN_HOOK(binlog_transmit, reserve_header, (m_thd, flags, &m_packet)))
  {
    set_unknown_error("Failed to run hook 'reserve_header'");
    DBUG_RETURN(1);
  }

  /* Resizes the buffer if needed. */
  if (grow_packet(event_len))
    DBUG_RETURN(1);

  DBUG_PRINT("info", ("m_packet.alloced_length: %zu (after potential "
                      "reallocation)", m_packet.alloced_length()));

  DBUG_RETURN(0);
}

int Binlog_sender::send_format_description_event(IO_CACHE *log_cache,
                                                 my_off_t start_pos)
{
  DBUG_ENTER("Binlog_sender::send_format_description_event");
  uchar* event_ptr= nullptr;
  uint32 event_len= 0;

  if (read_event(log_cache, binary_log::BINLOG_CHECKSUM_ALG_OFF, &event_ptr,
                 &event_len))
    DBUG_RETURN(1);

  DBUG_PRINT("info",
             ("Looked for a Format_description_log_event, found event type %s",
              Log_event::get_type_str((Log_event_type) event_ptr[EVENT_TYPE_OFFSET])));

  if (event_ptr[EVENT_TYPE_OFFSET] != binary_log::FORMAT_DESCRIPTION_EVENT)
  {
    set_fatal_error("Could not find format_description_event in binlog file");
    DBUG_RETURN(1);
  }

  DBUG_ASSERT(event_ptr[LOG_POS_OFFSET] > 0);
  m_event_checksum_alg=
    Log_event_footer::get_checksum_alg((const char *)event_ptr, event_len);

  DBUG_ASSERT(m_event_checksum_alg < binary_log::BINLOG_CHECKSUM_ALG_ENUM_END ||
              m_event_checksum_alg == binary_log::BINLOG_CHECKSUM_ALG_UNDEF);

  /* Slave does not support checksum, but binary events include checksum */
  if (m_slave_checksum_alg == binary_log::BINLOG_CHECKSUM_ALG_UNDEF &&
      event_checksum_on())
  {
    set_fatal_error("Slave can not handle replication events with the "
                    "checksum that master is configured to log");

    LogErr(WARNING_LEVEL, ER_RPL_BINLOG_MASTER_USES_CHECKSUM_AND_SLAVE_CANT);
    DBUG_RETURN(1);
  }

  event_ptr[FLAGS_OFFSET] &= ~LOG_EVENT_BINLOG_IN_USE_F;

  bool event_updated= false;
  if (m_using_gtid_protocol)
  {
    if (m_gtid_clear_fd_created_flag)
    {
      /*
        As we are skipping at least the first transaction of the binlog,
        we must clear the "created" field of the FD event (set it to 0)
        to avoid destroying temp tables on slave.
      */
      int4store(event_ptr + LOG_EVENT_MINIMAL_HEADER_LEN + ST_CREATED_OFFSET,
                0);
      event_updated= true;
    }
  }
  else if (start_pos > BIN_LOG_HEADER_SIZE)
  {
    /*
      If we are skipping the beginning of the binlog file based on the position
      asked by the slave, we must clear the log_pos and the created flag of the
      Format_description_log_event to be sent. Mark that this event with
      "log_pos=0", so the slave should not increment master's binlog position
      (rli->group_master_log_pos)
    */
    int4store(event_ptr + LOG_POS_OFFSET, 0);
    /*
      Set the 'created' field to 0 to avoid destroying
      temp tables on slave.
    */
    int4store(event_ptr + LOG_EVENT_MINIMAL_HEADER_LEN + ST_CREATED_OFFSET, 0);
    event_updated= true;
  }

  /* fix the checksum due to latest changes in header */
  if (event_checksum_on() && event_updated)
    calc_event_checksum(event_ptr, event_len);

  DBUG_RETURN(send_packet());
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
    *found= (buf[EVENT_TYPE_OFFSET] == binary_log::PREVIOUS_GTIDS_LOG_EVENT);
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

inline int Binlog_sender::read_event(IO_CACHE *log_cache, enum_binlog_checksum_alg checksum_alg,
                                     uchar **event_ptr, uint32 *event_len)
{
  DBUG_ENTER("Binlog_sender::read_event");

  size_t event_offset;
  char header[LOG_EVENT_MINIMAL_HEADER_LEN];
  int error= 0;
#ifndef DBUG_OFF
  const char *packet_buffer= NULL;
#endif

  if ((error= Log_event::peek_event_length(event_len, log_cache, header)))
    goto read_error;

  if (reset_transmit_packet(0, *event_len))
    DBUG_RETURN(1);

  event_offset= m_packet.length();
#ifndef DBUG_OFF
  packet_buffer= m_packet.ptr();
#endif

  DBUG_EXECUTE_IF("dump_thread_before_read_event",
                  {
                    const char act[]= "now wait_for signal.continue no_clear_event";
                    DBUG_ASSERT(!debug_sync_set_action(m_thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  /*
    packet is big enough to read the event, since we have reallocated based
    on the length stated in the event header.
  */
  if ((error= Log_event::read_log_event(log_cache, &m_packet, NULL, checksum_alg,
                                        NULL, NULL, header)))
    goto read_error;

  set_last_pos(my_b_tell(log_cache));

  /*
    As we pre-allocate the buffer to store the event at reset_transmit_packet,
    the buffer should not be changed while calling read_log_event, even knowing
    that it might call functions to replace the buffer by one with the size to
    fit the event.
  */
  DBUG_ASSERT(packet_buffer == m_packet.ptr());
  *event_ptr= (uchar *)m_packet.ptr() + event_offset;

  DBUG_PRINT("info",
             ("Read event %s",
              Log_event::get_type_str(Log_event_type
                                      ((*event_ptr)[EVENT_TYPE_OFFSET]))));
#ifndef DBUG_OFF
  if (check_event_count())
    DBUG_RETURN(1);
#endif
  DBUG_RETURN(0);
read_error:
  /*
    In theory, it should never happen. But RESET MASTER deletes binlog file
    directly without checking if there is any dump thread working.
  */
  error= (error == LOG_READ_EOF) ? LOG_READ_IO : error;
  set_fatal_error(log_read_error_msg(error));
  DBUG_RETURN(1);
}

int Binlog_sender::send_heartbeat_event(my_off_t log_pos)
{
  DBUG_ENTER("send_heartbeat_event");
  const char* filename= m_linfo.log_file_name;
  const char* p= filename + dirname_length(filename);
  size_t ident_len= strlen(p);
  size_t event_len= ident_len + LOG_EVENT_HEADER_LEN +
    (event_checksum_on() ? BINLOG_CHECKSUM_LEN : 0);

  DBUG_PRINT("info", ("log_file_name %s, log_pos %llu", p, log_pos));

  if (reset_transmit_packet(0, event_len))
    DBUG_RETURN(1);

  size_t event_offset= m_packet.length();
  m_packet.length(event_len + event_offset);
  uchar *header= (uchar *)m_packet.ptr() + event_offset;

  /* Timestamp field */
  int4store(header, 0);
  header[EVENT_TYPE_OFFSET] = binary_log::HEARTBEAT_LOG_EVENT;
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int4store(header + LOG_POS_OFFSET, static_cast<uint32>(log_pos));
  int2store(header + FLAGS_OFFSET, 0);
  memcpy(header + LOG_EVENT_HEADER_LEN, p, ident_len);

  if (event_checksum_on())
    calc_event_checksum(header, event_len);

  DBUG_RETURN(send_packet_and_flush());
}

inline int Binlog_sender::flush_net()
{
  if (DBUG_EVALUATE_IF("simulate_flush_error", 1,
      m_thd->get_protocol()->flush()))
  {
    set_unknown_error("failed on flush_net()");
    return 1;
  }
  return 0;
}

inline int Binlog_sender::send_packet()
{
  DBUG_ENTER("Binlog_sender::send_packet");
  DBUG_PRINT("info",
             ("Sending event of type %s", Log_event::get_type_str(
                (Log_event_type)m_packet.ptr()[1 + EVENT_TYPE_OFFSET])));
  // We should always use the same buffer to guarantee that the reallocation
  // logic is not broken.
  if (DBUG_EVALUATE_IF("simulate_send_error", true,
                       my_net_write(
                         m_thd->get_protocol_classic()->get_net(),
                         (uchar*) m_packet.ptr(), m_packet.length())))
  {
    set_unknown_error("Failed on my_net_write()");
    DBUG_RETURN(1);
  }

  /* Shrink the packet if needed. */
  int ret= shrink_packet() ? 1 : 0;
  m_last_event_sent_ts= time(0);
  DBUG_RETURN(ret);
}

inline int Binlog_sender::send_packet_and_flush()
{
  return (send_packet() || flush_net());
}

inline int Binlog_sender::before_send_hook(const char *log_file,
                                           my_off_t log_pos)
{
  if (m_observe_transmission &&
      RUN_HOOK(binlog_transmit, before_send_event,
               (m_thd, m_flag, &m_packet, log_file, log_pos)))
  {
    set_unknown_error("run 'before_send_event' hook failed");
    return 1;
  }
  return 0;
}

inline int Binlog_sender::after_send_hook(const char *log_file,
                                          my_off_t log_pos)
{
  if (m_observe_transmission &&
      RUN_HOOK(binlog_transmit, after_send_event,
               (m_thd, m_flag, &m_packet, log_file, log_pos)))
  {
    set_unknown_error("Failed to run hook 'after_send_event'");
    return 1;
  }

  /*
    semisync after_send_event hook doesn't return and error when net error
    happens.
  */
  if (m_thd->get_protocol_classic()->get_net()->last_errno != 0)
  {
    set_unknown_error("Found net error");
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
    set_unknown_error("Debugging binlog dump abort");
    return 1;
  }
  return 0;
}
#endif


inline bool Binlog_sender::grow_packet(size_t extra_size)
{
  DBUG_ENTER("Binlog_sender::grow_packet");
  size_t cur_buffer_size= m_packet.alloced_length();
  size_t cur_buffer_used= m_packet.length();
  size_t needed_buffer_size= cur_buffer_used + extra_size;

  if (extra_size > (PACKET_MAX_SIZE - cur_buffer_used))
    /*
       Not enough memory: requesting packet to be bigger than the max
       allowed - PACKET_MAX_SIZE.
    */
    DBUG_RETURN(true);

  /* Grow the buffer if needed. */
  if (needed_buffer_size > cur_buffer_size)
  {
    size_t new_buffer_size;
    new_buffer_size= calc_grow_buffer_size(cur_buffer_size, needed_buffer_size);

    if (!new_buffer_size)
      DBUG_RETURN(true);

    if (m_packet.mem_realloc(new_buffer_size))
      DBUG_RETURN(true);

    /*
     Calculates the new, smaller buffer, size to use the next time
     one wants to shrink the buffer.
    */
    calc_shrink_buffer_size(new_buffer_size);
  }

  DBUG_RETURN(false);
}

inline bool Binlog_sender::shrink_packet()
{
  DBUG_ENTER("Binlog_sender::shrink_packet");
  bool res= false;
  size_t cur_buffer_size= m_packet.alloced_length();
  size_t buffer_used= m_packet.length();

  DBUG_ASSERT(!(cur_buffer_size < PACKET_MIN_SIZE));

  /*
     If the packet is already at the minimum size, just
     do nothing. Otherwise, check if we should shrink.
   */
  if (cur_buffer_size > PACKET_MIN_SIZE)
  {
    /* increment the counter if we used less than the new shrink size. */
    if (buffer_used < m_new_shrink_size)
    {
      m_half_buffer_size_req_counter++;

      /* Check if we should shrink the buffer. */
      if (m_half_buffer_size_req_counter == PACKET_SHRINK_COUNTER_THRESHOLD)
      {
        /*
         The last PACKET_SHRINK_COUNTER_THRESHOLD consecutive packets
         required less than half of the current buffer size. Lets shrink
         it to not hold more memory than we potentially need.
        */
        m_packet.shrink(m_new_shrink_size);

        /*
           Calculates the new, smaller buffer, size to use the next time
           one wants to shrink the buffer.
         */
        calc_shrink_buffer_size(m_new_shrink_size);

        /* Reset the counter. */
        m_half_buffer_size_req_counter= 0;
      }
    }
    else
      m_half_buffer_size_req_counter= 0;
  }
#ifndef DBUG_OFF
  if (res == false)
  {
    DBUG_ASSERT(m_new_shrink_size <= cur_buffer_size);
    DBUG_ASSERT(m_packet.alloced_length() >= PACKET_MIN_SIZE);
  }
#endif
  DBUG_RETURN(res);
}

inline size_t Binlog_sender::calc_grow_buffer_size(size_t current_size,
                                                   size_t min_size)
{
  /* Check that a sane minimum buffer size was requested.  */
  DBUG_ASSERT(min_size > PACKET_MIN_SIZE);
  if (min_size > PACKET_MAX_SIZE)
    return 0;

  /*
     Even if this overflows (PACKET_MAX_SIZE == UINT_MAX32) and
     new_size wraps around, the min_size will always be returned,
     i.e., it is a safety net.

     Also, cap new_size to PACKET_MAX_SIZE (in case
     PACKET_MAX_SIZE < UINT_MAX32).
   */
  size_t new_size= static_cast<size_t>(
    std::min(static_cast<double>(PACKET_MAX_SIZE),
             static_cast<double>(current_size * PACKET_GROW_FACTOR)));

  new_size= ALIGN_SIZE(std::max(new_size, min_size));

  return new_size;
}

void Binlog_sender::calc_shrink_buffer_size(size_t current_size)
{
  size_t new_size= static_cast<size_t>(
      std::max(static_cast<double>(PACKET_MIN_SIZE),
               static_cast<double>(current_size * PACKET_SHRINK_FACTOR)));

  m_new_shrink_size= ALIGN_SIZE(new_size);
}
