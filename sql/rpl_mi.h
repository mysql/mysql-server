/* Copyright (c) 2006, 2012, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_MI_H
#define RPL_MI_H

#ifdef HAVE_REPLICATION

#include "rpl_rli.h"
#include "rpl_reporting.h"
#include "my_sys.h"

typedef struct st_mysql MYSQL;

/*****************************************************************************
  Replication IO Thread

  Master_info contains:
    - information about how to connect to a master
    - current master log name
    - current master log offset
    - misc control variables

  Master_info is initialized once from the master.info file if such
  exists. Otherwise, data members corresponding to master.info fields
  are initialized with defaults specified by master-* options. The
  initialization is done through init_master_info() call.

  The format of master.info file:

  log_name
  log_pos
  master_host
  master_user
  master_pass
  master_port
  master_connect_retry

  To write out the contents of master.info file to disk ( needed every
  time we read and queue data from the master ), a call to
  flush_master_info() is required.

  To clean up, call end_master_info()

*****************************************************************************/

class Master_info : public Slave_reporting_capability
{
 public:
  Master_info(bool is_slave_recovery);
  ~Master_info();
  bool shall_ignore_server_id(ulong s_id);
  void clear_in_memory_info(bool all);

  /* the variables below are needed because we can change masters on the fly */
  char master_log_name[FN_REFLEN];
  char host[HOSTNAME_LENGTH*SYSTEM_CHARSET_MBMAXLEN+1];
  char user[USERNAME_LENGTH+1];
  char password[MAX_PASSWORD_LENGTH*SYSTEM_CHARSET_MBMAXLEN+1];
  bool ssl; // enables use of SSL connection if true
  char ssl_ca[FN_REFLEN], ssl_capath[FN_REFLEN], ssl_cert[FN_REFLEN];
  char ssl_cipher[FN_REFLEN], ssl_key[FN_REFLEN];
  bool ssl_verify_server_cert;

  my_off_t master_log_pos;
  File fd; // we keep the file open, so we need to remember the file pointer
  IO_CACHE file;

  mysql_mutex_t data_lock, run_lock, sleep_lock;
  mysql_cond_t data_cond, start_cond, stop_cond, sleep_cond;
  THD *io_thd;
  MYSQL* mysql;
  uint32 file_id;				/* for 3.23 load data infile */
  Relay_log_info rli;
  uint port;
  /*
    to hold checksum alg in use until IO thread has received FD.
    Initialized to novalue, then set to the queried from master
    @@global.binlog_checksum and deactivated once FD has been received.
  */
  uint8 checksum_alg_before_fd;
  uint connect_retry;
#ifndef DBUG_OFF
  int events_till_disconnect;
#endif
  bool inited;
  volatile bool abort_slave;
  volatile uint slave_running;
  volatile ulong slave_run_id;
  /*
     The difference in seconds between the clock of the master and the clock of
     the slave (second - first). It must be signed as it may be <0 or >0.
     clock_diff_with_master is computed when the I/O thread starts; for this the
     I/O thread does a SELECT UNIX_TIMESTAMP() on the master.
     "how late the slave is compared to the master" is computed like this:
     clock_of_slave - last_timestamp_executed_by_SQL_thread - clock_diff_with_master

  */
  long clock_diff_with_master;
  /*
    Keeps track of the number of events before fsyncing.
    The option --sync-master-info determines how many
    events should happen before fsyncing.
  */
  uint sync_counter;
  float heartbeat_period;         // interface with CHANGE MASTER or master.info
  ulonglong received_heartbeats;  // counter of received heartbeat events
  DYNAMIC_ARRAY ignore_server_ids;
  ulong master_id;
};
int init_master_info(Master_info* mi, const char* master_info_fname,
		     const char* slave_info_fname,
		     bool abort_if_no_master_info_file,
		     int thread_mask);
void end_master_info(Master_info* mi);
int flush_master_info(Master_info* mi, 
                      bool flush_relay_log_cache, 
                      bool need_lock_relay_log);
int change_master_server_id_cmp(ulong *id1, ulong *id2);

#endif /* HAVE_REPLICATION */
#endif /* RPL_MI_H */
