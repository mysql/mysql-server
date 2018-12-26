/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_SLAVE_H
#define RPL_SLAVE_H

#include "my_global.h"
#include "my_thread.h"                     // my_start_routine
#include "mysql/psi/mysql_thread.h"        // mysql_mutex_t
#include "rpl_channel_service_interface.h" // enum_channel_type

class Log_event;
class Master_info;
class Relay_log_info;
class THD;
typedef struct st_bitmap MY_BITMAP;
typedef struct st_lex_master_info LEX_MASTER_INFO;
typedef struct st_list LIST;
typedef struct st_mysql MYSQL;
typedef struct st_net NET;
typedef struct struct_slave_connection LEX_SLAVE_CONNECTION;

typedef enum { SLAVE_THD_IO, SLAVE_THD_SQL, SLAVE_THD_WORKER } SLAVE_THD_TYPE;

/**
  MASTER_DELAY can be at most (1 << 31) - 1.
*/
#define MASTER_DELAY_MAX (0x7FFFFFFF)
#if INT_MAX < 0x7FFFFFFF
#error "don't support platforms where INT_MAX < 0x7FFFFFFF"
#endif

/**
  @defgroup Replication Replication
  @{

  @file
*/

/** 
   Some of defines are need in parser even though replication is not 
   compiled in (embedded).
*/

/**
   The maximum is defined as (ULONG_MAX/1000) with 4 bytes ulong
*/
#define SLAVE_MAX_HEARTBEAT_PERIOD 4294967

#ifdef HAVE_REPLICATION

#define SLAVE_NET_TIMEOUT  60

#define MAX_SLAVE_ERROR    10000

#define MTS_WORKER_UNDEF ((ulong) -1)
#define MTS_MAX_WORKERS  1024
#define MAX_SLAVE_RETRY_PAUSE 5

/* 
   When using tables to store the slave workers bitmaps,
   we use a BLOB field. The maximum size of a BLOB is:

   2^16-1 = 65535 bytes => (2^16-1) * 8 = 524280 bits
*/
#define MTS_MAX_BITS_IN_GROUP ((1L << 19) - 8) /* 524280 */

extern bool server_id_supplied;

/*****************************************************************************

  MySQL Replication

  Replication is implemented via two types of threads:

    I/O Thread - One of these threads is started for each master server.
                 They maintain a connection to their master server, read log
                 events from the master as they arrive, and queues them into
                 a single, shared relay log file.  A Master_info 
                 represents each of these threads.

    SQL Thread - One of these threads is started and reads from the relay log
                 file, executing each event.  A Relay_log_info 
                 represents this thread.

  Buffering in the relay log file makes it unnecessary to reread events from
  a master server across a slave restart.  It also decouples the slave from
  the master where long-running updates and event logging are concerned--ie
  it can continue to log new events while a slow query executes on the slave.

*****************************************************************************/

/*
  MUTEXES in replication:

  channel_map lock: This is to lock the Multisource datastructure (channel_map).
  Generally it used to retrieve an mi from channel_map.It is used to SERIALIZE ALL
  administrative commands of replication: START SLAVE, STOP SLAVE, CHANGE
  MASTER, RESET SLAVE, end_slave() (when mysqld stops) [init_slave() does not
  need it it's called early]. Any of these commands holds the mutex from the
  start till the end. This thus protects us against a handful of deadlocks
  (consider start_slave_thread() which, when starting the I/O thread, releases
  mi->run_lock, keeps rli->run_lock, and tries to re-acquire mi->run_lock).

  In Master_info: run_lock, data_lock
  run_lock protects all information about the run state: slave_running, thd
  and the existence of the I/O thread (to stop/start it, you need this mutex).
  data_lock protects some moving members of the struct: counters (log name,
  position) and relay log (MYSQL_BIN_LOG object).

  In Relay_log_info: run_lock, data_lock
  see Master_info
  However, note that run_lock does not protect
  Relay_log_info.run_state; that is protected by data_lock.

  In MYSQL_BIN_LOG: LOCK_log, LOCK_index of the binlog and the relay log
  LOCK_log: when you write to it. LOCK_index: when you create/delete a binlog
  (so that you have to update the .index file).

  The global_sid_lock must not be taken after LOCK_reset_gtid_table.

  ==== Order of acquisition ====

  Here, we list most major functions that acquire multiple locks.

  Notation: For each function, we list the locks it takes, in the
  order it takes them.  If a function holds lock A while taking lock
  B, then we write "A, B".  If a function locks A, unlocks A, then
  locks B, then we write "A | B".  If function F1 invokes function F2,
  then we write F2's name in parentheses in the list of locks for F1.

    show_master_info:
      mi.data_lock, rli.data_lock, mi.err_lock, rli.err_lock

    stop_slave:
      channel_map lock,
      ( mi.run_lock, thd.LOCK_thd_data
      | rli.run_lock, thd.LOCK_thd_data
      | relay.LOCK_log
      )

    start_slave:
      mi.run_lock, rli.run_lock, rli.data_lock, global_sid_lock->wrlock

    reset_logs:
      THD::LOCK_thd_data, .LOCK_log, .LOCK_index, global_sid_lock->wrlock

    purge_relay_logs:
      rli.data_lock, (relay.reset_logs) THD::LOCK_thd_data,
      relay.LOCK_log, relay.LOCK_index, global_sid_lock->wrlock

    reset_master:
      (binlog.reset_logs) THD::LOCK_thd_data, binlog.LOCK_log,
      binlog.LOCK_index, global_sid_lock->wrlock, LOCK_reset_gtid_table

    reset_slave:
      mi.run_lock, rli.run_lock, (purge_relay_logs) rli.data_lock,
      THD::LOCK_thd_data, relay.LOCK_log, relay.LOCK_index,
      global_sid_lock->wrlock

    purge_logs:
      .LOCK_index, LOCK_thd_list, thd.linfo.lock

      [Note: purge_logs contains a known bug: LOCK_index should not be
      taken before LOCK_thd_list.  This implies that, e.g.,
      purge_master_logs can deadlock with reset_master.  However,
      although purge_first_log and reset_slave take locks in reverse
      order, they cannot deadlock because they both first acquire
      rli.data_lock.]

    purge_master_logs, purge_master_logs_before_date, purge:
      (binlog.purge_logs) binlog.LOCK_index, LOCK_thd_list, thd.linfo.lock

    purge_first_log:
      rli.data_lock, relay.LOCK_index, rli.log_space_lock,
      (relay.purge_logs) LOCK_thd_list, thd.linfo.lock

    MYSQL_BIN_LOG::new_file_impl:
      .LOCK_log, .LOCK_index,
      ( [ if binlog: LOCK_prep_xids ]
      | global_sid_lock->wrlock
      )

    rotate_relay_log:
      (relay.new_file_impl) relay.LOCK_log, relay.LOCK_index,
      global_sid_lock->wrlock

    kill_zombie_dump_threads:
      LOCK_thd_list, thd.LOCK_thd_data

    init_relay_log_pos:
      rli.data_lock, relay.log_lock

    rli_init_info:
      rli.data_lock,
      ( relay.log_lock
      | global_sid_lock->wrlock
      | (relay.open_binlog)
      | (init_relay_log_pos) rli.data_lock, relay.log_lock
      )

    change_master:
      mi.run_lock, rli.run_lock, (init_relay_log_pos) rli.data_lock,
      relay.log_lock

    Sys_var_gtid_mode::global_update:
      gtid_mode_lock, channel_map lock, binlog.LOCK_log, global_sid_lock

  So the DAG of lock acquisition order (not counting the buggy
  purge_logs) is, empirically:

    gtid_mode_lock, channel_map lock, mi.run_lock, rli.run_lock,
      ( rli.data_lock,
        ( LOCK_thd_list,
          (
            ( binlog.LOCK_log, binlog.LOCK_index
            | relay.LOCK_log, relay.LOCK_index
            ),
            ( rli.log_space_lock | global_sid_lock->wrlock )
          | binlog.LOCK_log, binlog.LOCK_index, LOCK_prep_xids
          | thd.LOCK_data
          )
        | mi.err_lock, rli.err_lock
        )
      )
    )
    | mi.data_lock, rli.data_lock
*/

extern ulong master_retry_count;
extern MY_BITMAP slave_error_mask;
extern char slave_skip_error_names[];
extern bool use_slave_mask;
extern char *slave_load_tmpdir;
extern char *master_info_file, *relay_log_info_file;
extern char *opt_relay_logname, *opt_relaylog_index_name;
extern char *opt_binlog_index_name;
extern my_bool opt_skip_slave_start, opt_reckless_slave;
extern my_bool opt_log_slave_updates;
extern char *opt_slave_skip_errors;
extern ulonglong relay_log_space_limit;

extern const char *relay_log_index;
extern const char *relay_log_basename;

/*
  3 possible values for Master_info::slave_running and
  Relay_log_info::slave_running.
  The values 0,1,2 are very important: to keep the diff small, I didn't
  substitute places where we use 0/1 with the newly defined symbols. So don't change
  these values.
  The same way, code is assuming that in Relay_log_info we use only values
  0/1.
  I started with using an enum, but
  enum_variable=1; is not legal so would have required many line changes.
*/
#define MYSQL_SLAVE_NOT_RUN         0
#define MYSQL_SLAVE_RUN_NOT_CONNECT 1
#define MYSQL_SLAVE_RUN_CONNECT     2

/*
  If the following is set, if first gives an error, second will be
  tried. Otherwise, if first fails, we fail.
*/
#define SLAVE_FORCE_ALL 4

/* @todo: see if you can change to int */
bool start_slave_cmd(THD* thd);
bool stop_slave_cmd(THD* thd);
bool change_master_cmd(THD *thd);
int change_master(THD* thd, Master_info* mi, LEX_MASTER_INFO* lex_mi,
                  bool preserve_logs= false);
bool reset_slave_cmd(THD *thd);
bool show_slave_status_cmd(THD *thd);
bool flush_relay_logs_cmd(THD *thd);
bool is_any_slave_channel_running(int thread_mask,
                                  Master_info* already_locked_mi=NULL);

bool flush_relay_logs(Master_info *mi);
int reset_slave(THD *thd, Master_info* mi, bool reset_all);
int reset_slave(THD *thd);
int init_slave();
int init_recovery(Master_info* mi, const char** errmsg);
/**
  Call mi->init_info() and/or mi->rli->init_info(), which will read
  the replication configuration from repositories.

  This takes care of creating a transaction context in case table
  repository is needed.

  @param mi The Master_info object to use.

  @param ignore_if_no_info If this is false, and the repository does
  not exist, it will be created. If this is true, and the repository
  does not exist, nothing is done.

  @param thread_mask Indicate which repositories will be initialized:
  if (thread_mask&SLAVE_IO)!=0, then mi->init_info is called; if
  (thread_mask&SLAVE_SQL)!=0, then mi->rli->init_info is called.

  @retval 0 Success
  @retval nonzero Error
*/
int load_mi_and_rli_from_repositories(Master_info* mi,
                                      bool ignore_if_no_info,
                                      int thread_mask);
void end_info(Master_info* mi);
int remove_info(Master_info* mi);
int flush_master_info(Master_info* mi, bool force);
void add_slave_skip_errors(const char* arg);
void set_slave_skip_errors(char** slave_skip_errors_ptr);
int register_slave_on_master(MYSQL* mysql);
int add_new_channel(Master_info** mi, const char* channel);
/**
  Terminates the slave threads according to the given mask.

  @param mi                the master info repository
  @param thread_mask       the mask identifying which thread(s) to terminate
  @param stop_wait_timeout the timeout after which the method returns and error
  @param need_lock_term
          If @c false the lock will not be acquired before waiting on
          the condition. In this case, it is assumed that the calling
          function acquires the lock before calling this function.

  @return the operation status
    @retval 0    OK
    @retval ER_SLAVE_NOT_RUNNING
      The slave is already stopped
    @retval ER_STOP_SLAVE_SQL_THREAD_TIMEOUT
      There was a timeout when stopping the SQL thread
    @retval ER_STOP_SLAVE_IO_THREAD_TIMEOUT
      There was a timeout when stopping the IO thread
    @retval ER_ERROR_DURING_FLUSH_LOGS
      There was an error while flushing the log/repositories
*/
int terminate_slave_threads(Master_info* mi, int thread_mask,
                            ulong stop_wait_timeout,
                            bool need_lock_term= true);
bool start_slave_threads(bool need_lock_slave, bool wait_for_start,
                         Master_info* mi, int thread_mask);
bool start_slave(THD *thd);
int stop_slave(THD *thd);
bool start_slave(THD* thd,
                 LEX_SLAVE_CONNECTION* connection_param,
                 LEX_MASTER_INFO* master_param,
                 int thread_mask_input,
                 Master_info* mi,
                 bool set_mts_settings);
int stop_slave(THD* thd, Master_info* mi, bool net_report,
               bool for_one_channel,
               bool* push_temp_table_warning);
/*
  cond_lock is usually same as start_lock. It is needed for the case when
  start_lock is 0 which happens if start_slave_thread() is called already
  inside the start_lock section, but at the same time we want a
  mysql_cond_wait() on start_cond, start_lock
*/
bool start_slave_thread(
#ifdef HAVE_PSI_INTERFACE
                        PSI_thread_key thread_key,
#endif
                        my_start_routine h_func,
                        mysql_mutex_t *start_lock,
                        mysql_mutex_t *cond_lock,
                        mysql_cond_t *start_cond,
                        volatile uint *slave_running,
                        volatile ulong *slave_run_id,
                        Master_info *mi);

/* retrieve table from master and copy to slave*/
int fetch_master_table(THD* thd, const char* db_name, const char* table_name,
		       Master_info* mi, MYSQL* mysql, bool overwrite);

bool show_slave_status(THD* thd, Master_info* mi);
bool show_slave_status(THD* thd);
bool rpl_master_has_bug(const Relay_log_info *rli, uint bug_id, bool report,
                        bool (*pred)(const void *), const void *param);
bool rpl_master_erroneous_autoinc(THD* thd);

const char *print_slave_db_safe(const char *db);
void skip_load_data_infile(NET* net);

void end_slave(); /* release slave threads */
void delete_slave_info_objects(); /* clean up slave threads data */
void clear_until_condition(Relay_log_info* rli);
void clear_slave_error(Relay_log_info* rli);
void lock_slave_threads(Master_info* mi);
void unlock_slave_threads(Master_info* mi);
void init_thread_mask(int* mask,Master_info* mi,bool inverse);
void set_slave_thread_options(THD* thd);
void set_slave_thread_default_charset(THD *thd, Relay_log_info const *rli);
int apply_event_and_update_pos(Log_event* ev, THD* thd, Relay_log_info* rli);
int rotate_relay_log(Master_info* mi);
bool queue_event(Master_info* mi,const char* buf, ulong event_len);

extern "C" void *handle_slave_io(void *arg);
extern "C" void *handle_slave_sql(void *arg);
bool net_request_file(NET* net, const char* fname);

extern bool volatile abort_loop;
extern LIST master_list;
extern my_bool replicate_same_server_id;

extern int disconnect_slave_event_count, abort_slave_event_count ;

/* the master variables are defaults read from my.cnf or command line */
extern uint master_port, master_connect_retry, report_port;
extern char * master_user, *master_password, *master_host;
extern char *master_info_file, *relay_log_info_file, *report_user;
extern char *report_host, *report_password;

extern my_bool master_ssl;
extern char *master_ssl_ca, *master_ssl_capath, *master_ssl_cert, *master_tls_version;
extern char *master_ssl_cipher, *master_ssl_key;
       
bool mts_recovery_groups(Relay_log_info *rli);
bool mts_checkpoint_routine(Relay_log_info *rli, ulonglong period,
                            bool force, bool need_data_lock);
bool sql_slave_killed(THD* thd, Relay_log_info* rli);
#endif /* HAVE_REPLICATION */

/* masks for start/stop operations on io and sql slave threads */
#define SLAVE_IO  1
#define SLAVE_SQL 2

/**
  @} (end of group Replication)
*/
#endif
