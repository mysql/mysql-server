/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

#ifndef RPL_REPLICA_H
#define RPL_REPLICA_H

#include <limits.h>
#include <sys/types.h>
#include <atomic>

#include "m_string.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "my_thread.h"  // my_start_routine
#include "mysql.h"      // MYSQL
#include "mysql/components/services/bits/psi_thread_bits.h"
#include "mysql_com.h"
#include "sql/changestreams/apply/constants.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"

class Master_info;
class Relay_log_info;
class THD;
struct LEX_MASTER_INFO;
struct mysql_cond_t;
struct mysql_mutex_t;
class Rpl_channel_filters;

typedef struct struct_slave_connection LEX_SLAVE_CONNECTION;

typedef enum {
  SLAVE_THD_IO,
  SLAVE_THD_SQL,
  SLAVE_THD_WORKER,
  SLAVE_THD_MONITOR
} SLAVE_THD_TYPE;

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

#define REPLICA_NET_TIMEOUT 60

#define MAX_SLAVE_ERROR 14000

#define MTS_WORKER_UNDEF ((ulong)-1)
#define MTS_MAX_WORKERS 1024
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
                 a single, shared relay log file.  A Master_info represents
                 each of these threads.

    SQL Thread - One of these threads is started and reads from the relay log
                 file, executing each event. A Relay_log_info represents this
                 thread.

  Buffering in the relay log file makes it unnecessary to reread events from
  a master server across a slave restart.  It also decouples the slave from
  the master where long-running updates and event logging are concerned--ie
  it can continue to log new events while a slow query executes on the slave.

*****************************************************************************/

/*
  # MUTEXES in replication #

  JAG: TODO: This guide needs to be updated after pushing WL#10406!

  ## In Multisource_info (channel_map) ##

  ### m_channel_map_lock ###

  This rwlock is used to protect the multi source replication data structure
  (channel_map). Any operation reading contents from the channel_map should
  hold the rdlock during the operation. Any operation changing the
  channel_map (either adding/removing channels to/from the channel_map)
  should hold the wrlock during the operation.

  [init_replica() does not need it it's called early].

  ## In Master_info (mi) ##

  ### m_channel_lock ###

  It is used to SERIALIZE ALL administrative commands of replication: START
  SLAVE, STOP SLAVE, CHANGE MASTER, RESET SLAVE, delete_slave_info_objects
  (when mysqld stops)

  This thus protects us against a handful of deadlocks, being the know ones
  around lock_slave_threads and the mixed order they are acquired in some
  operations:

   + consider start_slave_thread() which, when starting the I/O thread,
     releases mi->run_lock, keeps rli->run_lock, and tries to re-acquire
     mi->run_lock.

   + Same applies to stop_slave() where a stop of the I/O thread will
     mi->run_lock, keeps rli->run_lock, and tries to re-acquire mi->run_lock.
     For the SQL thread, the order is the opposite.

  ### run_lock ###

  Protects all information about the running state: slave_running, thd
  and the existence of the I/O thread itself (to stop/start it, you need
  this mutex).
  Check the above m_channel_lock about locking order.

  ### data_lock ###

  Protects some moving members of the struct: counters (log name,
  position).

  ### sid_lock ###

  Protects the retrieved GTID set and it's SID map from updates.

  ## In Relay_log_info (rli) ##

  ### run_lock ###

  Same as Master_info's one. However, note that run_lock does not protect
  Relay_log_info.run_state. That is protected by data_lock.
  Check the above m_channel_lock about locking order.

  ### data_lock ###

  Protects some moving members of the struct: counters (log name,
  position).

  ## In MYSQL_BIN_LOG (mysql_bin_log,relay_log) ##

  ### LOCK_log ###

  This mutex should be taken when going to write to a log file. Notice that it
  does not prevent other threads from reading from the file being written (the
  "hot" file) or any other older file.

  ### LOCK_index ###

  This mutex should be taken when going to create/delete a log file (as those
  operations will update the .index file).

  ### LOCK_binlog_end_pos ###

  This mutex protects the access to the binlog_end_pos variable. The variable
  it set with the position that other threads reading from the currently active
  log file (the "hot" one) should not cross.

  ## Gtid_state (gtid_state, global_sid_map) ##

  ### global_sid_lock ###

  Protects all Gtid_state GTID sets (lost_gtids, executed_gtids,
  gtids_only_in_table, previous_gtids_logged, owned_gtids) and the global SID
  map from updates.

  The global_sid_lock must not be taken after LOCK_reset_gtid_table.

  ## Gtid_mode (gtid_mode) ##

  ### Gtid_mode::lock ###

  Used to arbitrate changes on server Gtid_mode.

  # Order of acquisition #

  Here, we list most major functions that acquire multiple locks.

  Notation: For each function, we list the locks it takes, in the
  order it takes them.  If a function holds lock A while taking lock
  B, then we write "A, B".  If a function locks A, unlocks A, then
  locks B, then we write "A | B".  If function F1 invokes function F2,
  then we write F2's name in parentheses in the list of locks for F1.

    Sys_var_gtid_mode::global_update:
      Gtid_mode::lock.wrlock, channel_map->wrlock, binlog.LOCK_log,
  global_sid_lock->wrlock

    change_master_cmd:
      channel_map.wrlock, (change_master)

    change_master:
      mi.channel_wrlock, mi.run_lock, rli.run_lock, (global_init_info),
  (purge_relay_logs), (init_relay_log_pos), rli.err_lock

    global_init_info:
      mi.data_lock, rli.data_lock

    purge_relay_logs:
      rli.data_lock, (relay_log.reset_logs)

    relay_log.reset_logs:
      .LOCK_log, .LOCK_index, .sid_lock->wrlock

    init_relay_log_pos:
      rli.data_lock

    queue_event:
      rli.LOCK_log, relay_log.sid_lock->rdlock, mi.data_lock

    stop_slave:
      channel_map rdlock,
      ( mi.channel_wrlock, mi.run_lock, thd.LOCK_thd_data
      | rli.run_lock, thd.LOCK_thd_data
      | relay.LOCK_log
      )

    start_slave:
      mi.channel_wrlock, mi.run_lock, rli.run_lock, rli.data_lock,
      global_sid_lock->wrlock

    mysql_bin_log.reset_logs:
      .LOCK_log, .LOCK_index, global_sid_lock->wrlock

    purge_relay_logs:
      rli.data_lock, (relay.reset_logs) THD::LOCK_thd_data,
      relay.LOCK_log, relay.LOCK_index, global_sid_lock->wrlock

    reset_master:
      (binlog.reset_logs) THD::LOCK_thd_data, binlog.LOCK_log,
      binlog.LOCK_index, global_sid_lock->wrlock, LOCK_reset_gtid_table

    reset_slave:
      mi.channel_wrlock, mi.run_lock, rli.run_lock, (purge_relay_logs)
      rli.data_lock, THD::LOCK_thd_data, relay.LOCK_log, relay.LOCK_index,
      global_sid_lock->wrlock

    purge_logs:
      .LOCK_index, LOCK_thd_list, thd.linfo.lock

      [Note: purge_logs contains a known bug: LOCK_index should not be
      taken before LOCK_thd_list.  This implies that, e.g.,
      purge_source_logs_to_file can deadlock with reset_master.  However,
      although purge_first_log and reset_slave take locks in reverse
      order, they cannot deadlock because they both first acquire
      rli.data_lock.]

    purge_source_logs_to_file, purge_source_logs_before_date, purge:
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
      (relay.new_file_impl) relay.LOCK_log, relay.LOCK_index

    kill_zombie_dump_threads:
      LOCK_thd_list, thd.LOCK_thd_data

    rli_init_info:
      rli.data_lock,
      ( relay.log_lock
      | global_sid_lock->wrlock
      | (relay.open_binlog)
      | (init_relay_log_pos) rli.data_lock, relay.log_lock
      )

  So the DAG of lock acquisition order (not counting the buggy
  purge_logs) is, empirically:

    Gtid_mode::lock, channel_map lock, mi.run_lock, rli.run_lock,
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
extern char *replica_load_tmpdir;
extern const char *master_info_file;
extern const char *relay_log_info_file;
extern char *opt_relay_logname, *opt_relaylog_index_name;
extern bool opt_relaylog_index_name_supplied;
extern bool opt_relay_logname_supplied;
extern char *opt_binlog_index_name;
extern bool opt_skip_replica_start;
extern bool opt_log_replica_updates;
extern char *opt_replica_skip_errors;
extern ulonglong relay_log_space_limit;

extern const char *relay_log_index;
extern const char *relay_log_basename;

/// @brief Helper class used to initialize the replica (includes init_replica())
/// @details init_replica is called once during the mysqld start-up
class ReplicaInitializer {
 public:
  /// @brief Constructor, calls init_replica()
  /// @param[in] opt_initialize Server option used to indicate whether mysqld
  /// has been started with --initialize
  /// @param[in] opt_skip_replica_start When true, skips the start of
  /// replication threads
  /// @param[in] filters Replication filters
  /// @param[in] replica_skip_erors
  ReplicaInitializer(bool opt_initialize, bool opt_skip_replica_start,
                     Rpl_channel_filters &filters, char **replica_skip_erors);

  /// @brief Gets initialization code set-up at replica initialization
  /// @return Error code obtained during the replica initialization
  int get_initialization_code() const;

 private:
  /// @brief This function starts replication threads
  /// @param[in] skip_replica_start When true, skips the start of replication
  /// threads threads
  void start_replication_threads(bool skip_replica_start = true);

  /// @brief Initializes replica PSI keys in case PSI interface is available
  static void init_replica_psi_keys();

  /// @brief Performs replica initialization, creates default replication
  /// channel and sets channel filters
  /// @returns Error code
  int init_replica();

  /// @brief In case debug mode is on, prints channel information
  void print_channel_info() const;

  /// @brief This function starts replication threads
  void start_threads();

  bool m_opt_initialize_replica =
      false;  ///< Indicates whether to initialize replica
  bool m_opt_skip_replica_start =
      false;  ///< Indicates whether replica threads should be started or not
  int m_init_code = 0;    ///< Replica initialization error code
  int m_thread_mask = 0;  ///< Thread mask indicating type of the thread
};

/*
  3 possible values for Master_info::slave_running and
  Relay_log_info::slave_running.
  The values 0,1,2 are very important: to keep the diff small, I didn't
  substitute places where we use 0/1 with the newly defined symbols. So don't
  change these values. The same way, code is assuming that in Relay_log_info we
  use only values 0/1. I started with using an enum, but enum_variable=1; is not
  legal so would have required many line changes.
*/
#define MYSQL_SLAVE_NOT_RUN 0
#define MYSQL_SLAVE_RUN_NOT_CONNECT 1
#define MYSQL_SLAVE_RUN_CONNECT 2

/*
  If the following is set, if first gives an error, second will be
  tried. Otherwise, if first fails, we fail.
*/
#define SLAVE_FORCE_ALL 4

/* @todo: see if you can change to int */
bool start_slave_cmd(THD *thd);
bool stop_slave_cmd(THD *thd);
bool change_master_cmd(THD *thd);
int change_master(THD *thd, Master_info *mi, LEX_MASTER_INFO *lex_mi,
                  bool preserve_logs = false);
bool reset_slave_cmd(THD *thd);
bool show_slave_status_cmd(THD *thd);
bool flush_relay_logs_cmd(THD *thd);
/**
  Re-encrypt previous relay logs with current master key for all slave channels.

  @retval false Success.
  @retval true Error.
*/
bool reencrypt_relay_logs();
int flush_relay_logs(Master_info *mi, THD *thd);
int reset_slave(THD *thd, Master_info *mi, bool reset_all);
int reset_slave(THD *thd);
int init_recovery(Master_info *mi);
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

  @param force_load repositories will only read information if they
  are not yet initialized. When true this flag forces the repositories
  to load information from table or file.

  @param skip_received_gtid_set_recovery When true, skips the received GTID
                                         set recovery.

  @retval 0 Success
  @retval nonzero Error
*/
int load_mi_and_rli_from_repositories(
    Master_info *mi, bool ignore_if_no_info, int thread_mask,
    bool skip_received_gtid_set_recovery = false, bool force_load = false);
void end_info(Master_info *mi);
/**
  Clear the information regarding the `Master_info` and `Relay_log_info` objects
  represented by the parameter, meaning, setting to `NULL` all attributes that
  are not meant to be kept between slave resets.

  @param mi the `Master_info` reference that holds both `Master_info` and
                `Relay_log_info` data.
 */
void clear_info(Master_info *mi);
int remove_info(Master_info *mi);
/**
  Resets the information regarding the `Master_info` and `Relay_log_info`
  objects represented by the parameter, meaning, setting to `NULL` all
  attributes that are not meant to be kept between slave resets and persisting
  all other attribute values in the repository.

  @param mi the `Master_info` reference that holds both `Master_info` and
                `Relay_log_info` data.

  @returns true if an error occurred and false otherwiser.
 */
bool reset_info(Master_info *mi);

/**
  This method flushes the current configuration for the channel into the
  connection metadata repository. It will also flush the current contents
  of the relay log file if instructed to.

  @param mi the `Master_info` reference that holds both `Master_info` and
            `Relay_log_info` data.

  @param force shall the method ignore the server settings that limit flushes
               to this repository

  @param need_lock shall the method take the associated data lock and log lock
                   if false ownership is asserted

  @param flush_relay_log should the method also flush the relay log file

  @param skip_repo_persistence if this method shall skip the repository flush
                               This won't skip the relay log flush if
                               flush_relay_log = true

  @returns 0 if no error occurred, !=0 if an error occurred
*/
int flush_master_info(Master_info *mi, bool force, bool need_lock = true,
                      bool flush_relay_log = true,
                      bool skip_repo_persistence = false);
void add_replica_skip_errors(const char *arg);
void set_replica_skip_errors(char **replica_skip_errors_ptr);
int add_new_channel(Master_info **mi, const char *channel);
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
    @retval ER_REPLICA_NOT_RUNNING
      The slave is already stopped
    @retval ER_STOP_REPLICA_SQL_THREAD_TIMEOUT
      There was a timeout when stopping the SQL thread
    @retval ER_STOP_REPLICA_IO_THREAD_TIMEOUT
      There was a timeout when stopping the IO thread
    @retval ER_ERROR_DURING_FLUSH_LOGS
      There was an error while flushing the log/repositories
*/
int terminate_slave_threads(Master_info *mi, int thread_mask,
                            ulong stop_wait_timeout,
                            bool need_lock_term = true);
bool start_slave_threads(bool need_lock_slave, bool wait_for_start,
                         Master_info *mi, int thread_mask);
bool start_slave(THD *thd);
int stop_slave(THD *thd);
bool start_slave(THD *thd, LEX_SLAVE_CONNECTION *connection_param,
                 LEX_MASTER_INFO *master_param, int thread_mask_input,
                 Master_info *mi, bool set_mts_settings);
int stop_slave(THD *thd, Master_info *mi, bool net_report, bool for_one_channel,
               bool *push_temp_table_warning);
/*
  cond_lock is usually same as start_lock. It is needed for the case when
  start_lock is 0 which happens if start_slave_thread() is called already
  inside the start_lock section, but at the same time we want a
  mysql_cond_wait() on start_cond, start_lock
*/
bool start_slave_thread(PSI_thread_key thread_key, my_start_routine h_func,
                        mysql_mutex_t *start_lock, mysql_mutex_t *cond_lock,
                        mysql_cond_t *start_cond,
                        std::atomic<uint> *slave_running,
                        std::atomic<ulong> *slave_run_id, Master_info *mi);

bool show_slave_status(THD *thd, Master_info *mi);
bool show_slave_status(THD *thd);

const char *print_slave_db_safe(const char *db);

void end_slave();                 /* release slave threads */
void delete_slave_info_objects(); /* clean up slave threads data */
void set_slave_thread_options(THD *thd);
void set_slave_thread_default_charset(THD *thd, Relay_log_info const *rli);
int rotate_relay_log(Master_info *mi, bool log_master_fd = true,
                     bool need_lock = true, bool need_log_space_lock = true);
typedef enum {
  QUEUE_EVENT_OK = 0,
  QUEUE_EVENT_ERROR_QUEUING,
  QUEUE_EVENT_ERROR_FLUSHING_INFO
} QUEUE_EVENT_RESULT;
QUEUE_EVENT_RESULT queue_event(Master_info *mi, const char *buf,
                               ulong event_len, bool flush_mi = true);

int heartbeat_queue_event(bool is_valid, Master_info *&mi,
                          std::string binlog_name, uint64_t position,
                          unsigned long &inc_pos, bool &do_flush_mi);

extern "C" void *handle_slave_io(void *arg);
extern "C" void *handle_slave_sql(void *arg);

/*
  SYNPOSIS
    connect_to_master()

  IMPLEMENTATION
    Try to connect until successful or replica killed or we have retried

    @param[in] thd   The thread.
    @param[in] mysql MySQL connection handle
    @param[in] mi    The Master_info object of the failed connection which
                     needs to be reconnected to the new source.
    @param[in] reconnect  If its need to reconnect to existing source.
    @param[in] host       The Host name or ip address of the source to which
                          connection need to be made.
    @param[in] port       The Port of the source to which connection need to
                          be made.
    @param[in] is_io_thread  To determine if its IO or Monitor IO thread.

    @retval 0    Success connecting to the source.
    @retval #    Error connecting to the source.
*/
int connect_to_master(THD *thd, MYSQL *mysql, Master_info *mi, bool reconnect,
                      bool suppress_warnings,
                      const std::string &host = std::string(),
                      const uint port = 0, bool is_io_thread = true);

bool net_request_file(NET *net, const char *fname);

extern bool replicate_same_server_id;

extern int disconnect_slave_event_count, abort_slave_event_count;

/* the master variables are defaults read from my.cnf or command line */
extern uint report_port;
extern const char *master_info_file;
extern const char *relay_log_info_file;
extern char *report_user;
extern char *report_host, *report_password;

bool mts_recovery_groups(Relay_log_info *rli);
/**
   Processing rli->gaq to find out the low-water-mark (lwm) coordinates
   which is stored into the central recovery table. rli->data_lock will be
   required, so the caller should not hold rli->data_lock.

   @param     rli      pointer to Relay-log-info of Coordinator
   @param     force    if true then hang in a loop till some progress
   @retval    false    Success
   @retval    true     Error
*/
bool mta_checkpoint_routine(Relay_log_info *rli, bool force);
bool sql_slave_killed(THD *thd, Relay_log_info *rli);

/*
  Check if the error is caused by network.
  @param[in]   errorno   Number of the error.
  RETURNS:
  true         network error
  false        not network error
*/
bool is_network_error(uint errorno);

int init_replica_thread(THD *thd, SLAVE_THD_TYPE thd_type);

/**
  @} (end of group Replication)
*/
#endif
