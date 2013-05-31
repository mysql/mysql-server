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

#ifndef DEFINED_RPL_BINLOG_SENDER
#define DEFINED_RPL_BINLOG_SENDER

#include "my_global.h"

#ifdef HAVE_REPLICATION
#include "rpl_gtid.h"
#include "my_sys.h"
#include "sql_class.h"
#include "sql_error.h"
#include "binlog.h"
#include "log_event.h"

/**
  The major logic of dump thread is implemented in this class. It sends
  required binlog events to clients according to their requests.
*/
class Binlog_sender
{
public:
  Binlog_sender(THD *thd, const char *start_file, my_off_t start_pos,
              Gtid_set *exclude_gtids)
    : m_thd(thd), m_start_file(start_file), m_start_pos(start_pos),
    m_exclude_gtid(exclude_gtids),
    m_using_gtid_protocol(exclude_gtids != NULL),
    m_check_previous_gtid_event(exclude_gtids != NULL),
    m_diag_area(false),
    m_errmsg(NULL), m_errno(0), m_last_file(NULL), m_last_pos(0)
  {}

  ~Binlog_sender() {}

  /**
    It checks the dump reqest and sends events to the client until it finish
    all events(for mysqlbinlog) or encounters an error.
  */
  void run();
private:
  THD *m_thd;

  /* Requested start binlog file and position */
  const char *m_start_file;
  my_off_t m_start_pos;

  /*
    For COM_BINLOG_DUMP_GTID, It may include a GTID set. All events in the set
    should not be sent to the client.
  */
  Gtid_set *m_exclude_gtid;
  bool m_using_gtid_protocol;
  bool m_check_previous_gtid_event;

  /* The binlog file it is reading */
  LOG_INFO m_linfo;

  uint8 m_event_checksum_alg;
  uint8 m_slave_checksum_alg;
  ulonglong m_heartbeat_period;
  /*
    For mysqlbinlog(server_id is 0), it will stop immediately without waiting
    if it already reads all events.
  */
  bool m_wait_new_events;

  Diagnostics_area m_diag_area;
  const char *m_errmsg;
  int m_errno;
  /*
    The position of the event it reads most recently is stored. So it can report
    the exact position after where an error happens.

    m_last_file will point to m_info.log_file_name, if it is same to
    m_info.log_file_name. Otherwise the file name is copied to m_last_file_buf
    and m_last_file will point to it.
  */
  char m_last_file_buf[FN_REFLEN];
  const char *m_last_file;
  my_off_t m_last_pos;

  /*
    It initializes the context, checks if the dump request is valid and
    if binlog status is correct.
  */
  void init();
  void cleanup();
  void init_heartbeat_period();
  void init_checksum_alg();
  /** Check if the requested binlog file and position are valid */
  int check_start_file();
  /** Transform read error numbers to error messages. */
  const char* log_read_error_msg(int error);

  /**
    It dumps a binlog file. Events are read and sent one by one. If it need
    to wait for new events, it will wait after already reading all events in
    the active log file.

    @param[in] log_cache  IO_CACHE of the binlog will be sent
    @param[in] start_pos  Only the events after start_pos are sent

    @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  int send_binlog(IO_CACHE *log_cache, my_off_t start_pos);

  /**
    It sends some events in a binlog file to the client.


     @param[in] log_cache  IO_CACHE of the binlog will be sent
     @param[in] end_pos    Only the events before end_pos are sent

     @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  int send_events(IO_CACHE *log_cache, my_off_t end_pos);

  /**
    It gets the end position of the binlog file.

    @param[in] log_cache  IO_CACHE of the binlog will be checked

    @return
      @retval 0  It already arrives the end of the binlog.
      @retval 1  Failed to get binlog position
      @retval >1 Succeeded to get binlog end position
  */
  my_off_t get_binlog_end_pos(IO_CACHE *log_cache);

  /**
     It checks if a binlog file has Previous_gtid_log_event

     @param[in]  log_cache  IO_CACHE of the binlog will be checked
     @param[out] found      Found Previous_gtid_log_event or not

     @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  int has_previous_gtid_log_event(IO_CACHE *log_cache, bool *found);

  /**
    It sends a faked rotate event which does not exist physically in any
    binlog to the slave. It contains the name of the binlog we are going to
    send to the slave.

    Faked rotate event is required in a few cases, so slave can know which
    binlog the following events are from.

  - The binlog file slave requested is Empty. E.g.
    "CHANGE MASTER TO MASTER_LOG_FILE='', MASTER_LOG_POS=4", etc.

  - The position slave requested is exactly the end of a binlog file.

  - Previous binlog file does not include a rotate event.
    It happens when server is shutdown and restarted.

  - The previous binary log was GTID-free (does not contain a
    Previous_gtids_log_event) and the slave is connecting using
    the GTID protocol.

    @param[in] packet         The buffer used to store the faked event.
    @param[in] next_log_file  The name of the binlog file will be sent after
                              the rotate event.
    @param[in] log_pos        The start position of the binlog file.

    @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  int fake_rotate_event(String *packet, const char *next_log_file,
                        my_off_t log_pos);

  /**
     When starting to dump a binlog file, Format_description_log_event
     is read and sent first. If the requested position is after
     Format_description_log_event, log_pos field in the first
     Format_description_log_event has to be set to 0. So the slave
     will not increment its master's binlog position.

     @param[in] packet         The buffer used to store the event.
     @param[in] log_cache      IO_CACHE of the binlog will be dumpped
     @param[in] clear_log_pos  If clears end_pos field in the event.

     @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  int send_format_description_event(String *packet, IO_CACHE *log,
                                    bool clear_log_pos);
  /**
     It sends a heartbeat to the client.

     @param[in] packet   The buffer used to store the event.
     @param[in] log_pos  The log position that events before it are sent.

     @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  int send_heartbeat_event(String* packet, my_off_t log_pos);

  /**
     It reads a event from binlog file.

     @param[in] log_cache     IO_CACHE of the binlog file.
     @param[in] checksum_alg  Checksum algorithm used to check the event.
     @param[out] event_ptr    The buffer used to store the event.
     @param[out] event_len    Length of the event.

     @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  inline int read_event(IO_CACHE *log_cache, uint8 checksum_alg,
                        uchar **event_ptr, uint32 *event_len);
  /**
    It checks if the event is in m_exclude_gtid.

    Clients may request to exclude some GTIDs. The events include in the GTID
    groups will be skipped. We call below events sequence as a goup,
    Gtid_log_event
    BEGIN
    ...
    COMMIT or ROLLBACK

    or
    Gtid_log_event
    DDL statement

    @param[in] event_ptr  Buffer of the event
    @param[in] event_len  Length of the event
    @param[in] in_exclude_group  If it is in a execude group

    @return It returns true if it should be skipped, otherwise false is turned.
  */
  inline bool skip_event(const uchar *event_ptr, uint32 event_len,
                         bool in_exclude_group);

  inline void calc_event_checksum(uchar *event_ptr, uint32 event_len);
  inline int flush_net();
  inline int send_packet(String *packet);
  inline int send_packet_and_flush(String *packet);
  inline int before_send_hook(String *packet, const char *log_file,
                              my_off_t log_pos);
  inline int after_send_hook(String *packet, const char *log_file,
                             my_off_t log_pos);
  /*
    Reset thread transmit packet buffer for event sending

    This function reserves header bytes for event transmission, and
    should be called before store the event data to the packet buffer.

    @param[inout] packet  The buffer where a event will be stored.
    @param[in] flags      The flag used in reset_transmit hook.
  */
  inline int reset_transmit_packet(String *packet, ushort flags);

  /**
    It waits until receiving an update_cond signal. It will send heartbeat
    periodically if m_heartbeat_period is set.

    @param[in] log_pos  The end position of the last event it already sent.
    It is required by heartbeat events.

    @return It returns 0 if succeeds, otherwise 1 is returned.
  */
  inline int wait_new_events(my_off_t log_pos);
  inline int wait_with_heartbeat(my_off_t log_pos);
  inline int wait_without_heartbeat();

#ifndef DBUG_OFF
  /* It is used to count the events that have been sent. */
  int m_event_count;
  /*
    It aborts dump thread with an error if m_event_count exceeds
    max_binlog_dump_events.
  */
  inline int check_event_count();
#endif

  bool has_error() { return m_errno != 0; }
  void set_error(int errorno, const char *errmsg)
  {
    m_errmsg= errmsg;
    m_errno= errorno;
  }

  void set_unknow_error(const char *errmsg)
  {
    set_error(ER_UNKNOWN_ERROR, errmsg);
  }

  void set_fatal_error(const char *errmsg)
  {
    set_error(ER_MASTER_FATAL_ERROR_READING_BINLOG, errmsg);
  }

  bool is_fatal_error()
  {
    return m_errno == ER_MASTER_FATAL_ERROR_READING_BINLOG;
  }

  bool event_checksum_on()
  {
    return m_event_checksum_alg > BINLOG_CHECKSUM_ALG_OFF &&
      m_event_checksum_alg < BINLOG_CHECKSUM_ALG_ENUM_END;
  }

  void set_last_pos(my_off_t log_pos)
  {
    m_last_file= m_linfo.log_file_name;
    m_last_pos= log_pos;
  }

  void set_last_file(const char *log_file)
  {
    strcpy(m_last_file_buf, log_file);
    m_last_file= m_last_file_buf;
  }
};

#endif // HAVE_REPLICATION
#endif // DEFINED_RPL_BINLOG_SENDER
