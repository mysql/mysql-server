/* Copyright (C) 2000-2003 MySQL AB
   
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

#ifdef HAVE_REPLICATION

#ifndef SLAVE_H
#define SLAVE_H

#include "mysql.h"
#include "my_list.h"
#define SLAVE_NET_TIMEOUT  3600
#define MAX_SLAVE_ERRMSG   1024
#define MAX_SLAVE_ERROR    2000

/*****************************************************************************

  MySQL Replication

  Replication is implemented via two types of threads:

    I/O Thread - One of these threads is started for each master server.
                 They maintain a connection to their master server, read log
                 events from the master as they arrive, and queues them into
                 a single, shared relay log file.  A MASTER_INFO struct
                 represents each of these threads.

    SQL Thread - One of these threads is started and reads from the relay log
                 file, executing each event.  A RELAY_LOG_INFO struct
                 represents this thread.

  Buffering in the relay log file makes it unnecessary to reread events from
  a master server across a slave restart.  It also decouples the slave from
  the master where long-running updates and event logging are concerned--ie
  it can continue to log new events while a slow query executes on the slave.

*****************************************************************************/

/*
  MUTEXES in replication:

  LOCK_active_mi: [note: this was originally meant for multimaster, to switch
  from a master to another, to protect active_mi] It is used to SERIALIZE ALL
  administrative commands of replication: START SLAVE, STOP SLAVE, CHANGE
  MASTER, RESET SLAVE, end_slave() (when mysqld stops) [init_slave() does not
  need it it's called early]. Any of these commands holds the mutex from the
  start till the end. This thus protects us against a handful of deadlocks
  (consider start_slave_thread() which, when starting the I/O thread, releases
  mi->run_lock, keeps rli->run_lock, and tries to re-acquire mi->run_lock).

  Currently active_mi never moves (it's created at startup and deleted at
  shutdown, and not changed: it always points to the same MASTER_INFO struct),
  because we don't have multimaster. So for the moment, mi does not move, and
  mi->rli does not either.

  In MASTER_INFO: run_lock, data_lock
  run_lock protects all information about the run state: slave_running, and the
  existence of the I/O thread (to stop/start it, you need this mutex).
  data_lock protects some moving members of the struct: counters (log name,
  position) and relay log (MYSQL_LOG object).

  In RELAY_LOG_INFO: run_lock, data_lock
  see MASTER_INFO
  
  Order of acquisition: if you want to have LOCK_active_mi and a run_lock, you
  must acquire LOCK_active_mi first.

  In MYSQL_LOG: LOCK_log, LOCK_index of the binlog and the relay log
  LOCK_log: when you write to it. LOCK_index: when you create/delete a binlog
  (so that you have to update the .index file).
*/

extern ulong master_retry_count;
extern MY_BITMAP slave_error_mask;
extern bool use_slave_mask;
extern char* slave_load_tmpdir;
extern my_string master_info_file,relay_log_info_file;
extern my_string opt_relay_logname, opt_relaylog_index_name;
extern my_bool opt_skip_slave_start, opt_reckless_slave;
extern my_bool opt_log_slave_updates;
extern ulonglong relay_log_space_limit;
struct st_master_info;

enum enum_binlog_formats {
  BINLOG_FORMAT_CURRENT=0, /* 0 is important for easy 'if (mi->old_format)' */
  BINLOG_FORMAT_323_LESS_57, 
  BINLOG_FORMAT_323_GEQ_57 };

/****************************************************************************

  Replication SQL Thread

  st_relay_log_info contains:
    - the current relay log
    - the current relay log offset
    - master log name
    - master log sequence corresponding to the last update
    - misc information specific to the SQL thread

  st_relay_log_info is initialized from the slave.info file if such exists.
  Otherwise, data members are intialized with defaults. The initialization is
  done with init_relay_log_info() call.

  The format of slave.info file:

  relay_log_name
  relay_log_pos
  master_log_name
  master_log_pos

  To clean up, call end_relay_log_info()

*****************************************************************************/

typedef struct st_relay_log_info
{
  /*** The following variables can only be read when protect by data lock ****/

  /*
    info_fd - file descriptor of the info file. set only during
    initialization or clean up - safe to read anytime
    cur_log_fd - file descriptor of the current read  relay log
  */
  File info_fd,cur_log_fd;

  /*
    Protected with internal locks.
    Must get data_lock when resetting the logs.
  */
  MYSQL_LOG relay_log;
  LOG_INFO linfo;
  IO_CACHE cache_buf,*cur_log;

  /* The following variables are safe to read any time */

  /* IO_CACHE of the info file - set only during init or end */
  IO_CACHE info_file;

  /*
    When we restart slave thread we need to have access to the previously
    created temporary tables. Modified only on init/end and by the SQL
    thread, read only by SQL thread.
  */
  TABLE *save_temporary_tables;

  /*
    standard lock acquistion order to avoid deadlocks:
    run_lock, data_lock, relay_log.LOCK_log, relay_log.LOCK_index
  */
  pthread_mutex_t data_lock,run_lock;

  /*
    start_cond is broadcast when SQL thread is started
    stop_cond - when stopped
    data_cond - when data protected by data_lock changes
  */
  pthread_cond_t start_cond, stop_cond, data_cond;

  /* parent master info structure */
  struct st_master_info *mi;

  /*
    Needed to deal properly with cur_log getting closed and re-opened with
    a different log under our feet
  */
  uint32 cur_log_old_open_count;
  
  /*
    Let's call a group (of events) :
      - a transaction
      or
      - an autocommiting query + its associated events (INSERT_ID,
    TIMESTAMP...)
    We need these rli coordinates :
    - relay log name and position of the beginning of the group we currently are
    executing. Needed to know where we have to restart when replication has
    stopped in the middle of a group (which has been rolled back by the slave).
    - relay log name and position just after the event we have just
    executed. This event is part of the current group.
    Formerly we only had the immediately above coordinates, plus a 'pending'
    variable, but this dealt wrong with the case of a transaction starting on a
    relay log and finishing (commiting) on another relay log. Case which can
    happen when, for example, the relay log gets rotated because of
    max_binlog_size.
  */
  char group_relay_log_name[FN_REFLEN];
  ulonglong group_relay_log_pos;
  char event_relay_log_name[FN_REFLEN];
  ulonglong event_relay_log_pos;
  /* 
     Original log name and position of the group we're currently executing
     (whose coordinates are group_relay_log_name/pos in the relay log)
     in the master's binlog. These concern the *group*, because in the master's
     binlog the log_pos that comes with each event is the position of the
     beginning of the group.
  */
  char group_master_log_name[FN_REFLEN];
  volatile my_off_t group_master_log_pos;

  /*
    Handling of the relay_log_space_limit optional constraint.
    ignore_log_space_limit is used to resolve a deadlock between I/O and SQL
    threads, the SQL thread sets it to unblock the I/O thread and make it
    temporarily forget about the constraint.
  */
  ulonglong log_space_limit,log_space_total;
  bool ignore_log_space_limit;

  /*
    When it commits, InnoDB internally stores the master log position it has
    processed so far; the position to store is the one of the end of the
    committing event (the COMMIT query event, or the event if in autocommit
    mode).
  */
#if MYSQL_VERSION_ID < 40100
  ulonglong future_master_log_pos;
#else
  ulonglong future_group_master_log_pos;
#endif

  time_t last_master_timestamp; 

  /*
    Needed for problems when slave stops and we want to restart it
    skipping one or more events in the master log that have caused
    errors, and have been manually applied by DBA already.
  */
  volatile uint32 slave_skip_counter;
  volatile ulong abort_pos_wait;	/* Incremented on change master */
  volatile ulong slave_run_id;		/* Incremented on slave start */
  pthread_mutex_t log_space_lock;
  pthread_cond_t log_space_cond;
  THD * sql_thd;
  int last_slave_errno;
#ifndef DBUG_OFF
  int events_till_abort;
#endif  
  char last_slave_error[MAX_SLAVE_ERRMSG];

  /* if not set, the value of other members of the structure are undefined */
  bool inited;
  volatile bool abort_slave, slave_running;

  /* 
     Condition and its parameters from START SLAVE UNTIL clause.
     
     UNTIL condition is tested with is_until_satisfied() method that is 
     called by exec_relay_log_event(). is_until_satisfied() caches the result
     of the comparison of log names because log names don't change very often;
     this cache is invalidated by parts of code which change log names with
     notify_*_log_name_updated() methods. (They need to be called only if SQL
     thread is running).
   */
  
  enum {UNTIL_NONE= 0, UNTIL_MASTER_POS, UNTIL_RELAY_POS} until_condition;
  char until_log_name[FN_REFLEN];
  ulonglong until_log_pos;
  /* extension extracted from log_name and converted to int */
  ulong until_log_name_extension;   
  /* 
     Cached result of comparison of until_log_name and current log name
     -2 means unitialised, -1,0,1 are comarison results 
  */
  enum 
  { 
    UNTIL_LOG_NAMES_CMP_UNKNOWN= -2, UNTIL_LOG_NAMES_CMP_LESS= -1,
    UNTIL_LOG_NAMES_CMP_EQUAL= 0, UNTIL_LOG_NAMES_CMP_GREATER= 1
  } until_log_names_cmp_result;
  
  st_relay_log_info();
  ~st_relay_log_info();

  /*
    Invalidate cached until_log_name and group_relay_log_name comparison 
    result. Should be called after any update of group_realy_log_name if
    there chances that sql_thread is running.
  */
  inline void notify_group_relay_log_name_update()
  {
    if (until_condition==UNTIL_RELAY_POS)
      until_log_names_cmp_result= UNTIL_LOG_NAMES_CMP_UNKNOWN;
  }

  /*
    The same as previous but for group_master_log_name. 
  */
  inline void notify_group_master_log_name_update()
  {
    if (until_condition==UNTIL_MASTER_POS)
      until_log_names_cmp_result= UNTIL_LOG_NAMES_CMP_UNKNOWN;
  }
  
  inline void inc_event_relay_log_pos(ulonglong val)
  {
    event_relay_log_pos+= val;
  }

  void inc_group_relay_log_pos(ulonglong val, ulonglong log_pos, bool skip_lock=0);
  int wait_for_pos(THD* thd, String* log_name, longlong log_pos, 
		   longlong timeout);
  void close_temporary_tables();

  /* Check if UNTIL condition is satisfied. See slave.cc for more. */
  bool is_until_satisfied();
  inline ulonglong until_pos()
  {
    return ((until_condition == UNTIL_MASTER_POS) ? group_master_log_pos :
	    group_relay_log_pos);
  }
} RELAY_LOG_INFO;


Log_event* next_event(RELAY_LOG_INFO* rli);

/*****************************************************************************

  Replication IO Thread

  st_master_info contains:
    - information about how to connect to a master
    - current master log name
    - current master log offset
    - misc control variables

  st_master_info is initialized once from the master.info file if such
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

typedef struct st_master_info
{
  /* the variables below are needed because we can change masters on the fly */
  char master_log_name[FN_REFLEN];
  char host[HOSTNAME_LENGTH+1];
  char user[USERNAME_LENGTH+1];
  char password[MAX_PASSWORD_LENGTH+1];
  my_bool ssl; // enables use of SSL connection if true
  char ssl_ca[FN_REFLEN], ssl_capath[FN_REFLEN], ssl_cert[FN_REFLEN];
  char ssl_cipher[FN_REFLEN], ssl_key[FN_REFLEN];
  
  my_off_t master_log_pos;
  File fd; // we keep the file open, so we need to remember the file pointer
  IO_CACHE file;
  
  pthread_mutex_t data_lock,run_lock;
  pthread_cond_t data_cond,start_cond,stop_cond;
  THD *io_thd;
  MYSQL* mysql;
  uint32 file_id;				/* for 3.23 load data infile */
  RELAY_LOG_INFO rli;
  uint port;
  uint connect_retry;
#ifndef DBUG_OFF
  int events_till_abort;
#endif
  bool inited;
  enum enum_binlog_formats old_format;
  volatile bool abort_slave, slave_running;
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
  
  st_master_info()
    :ssl(0), fd(-1),  io_thd(0), inited(0), old_format(BINLOG_FORMAT_CURRENT),
     abort_slave(0),slave_running(0), slave_run_id(0)
  {
    host[0] = 0; user[0] = 0; password[0] = 0;
    ssl_ca[0]= 0; ssl_capath[0]= 0; ssl_cert[0]= 0;
    ssl_cipher[0]= 0; ssl_key[0]= 0;
    
    bzero((char*) &file, sizeof(file));
    pthread_mutex_init(&run_lock, MY_MUTEX_INIT_FAST);
    pthread_mutex_init(&data_lock, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&data_cond, NULL);
    pthread_cond_init(&start_cond, NULL);
    pthread_cond_init(&stop_cond, NULL);
  }

  ~st_master_info()
  {
    pthread_mutex_destroy(&run_lock);
    pthread_mutex_destroy(&data_lock);
    pthread_cond_destroy(&data_cond);
    pthread_cond_destroy(&start_cond);
    pthread_cond_destroy(&stop_cond);
  }

} MASTER_INFO;


int queue_event(MASTER_INFO* mi,const char* buf,ulong event_len);

typedef struct st_table_rule_ent
{
  char* db;
  char* tbl_name;
  uint key_len;
} TABLE_RULE_ENT;

#define TABLE_RULE_HASH_SIZE   16
#define TABLE_RULE_ARR_SIZE   16
#define MAX_SLAVE_ERRMSG      1024

#define RPL_LOG_NAME (rli->group_master_log_name[0] ? rli->group_master_log_name :\
 "FIRST")
#define IO_RPL_LOG_NAME (mi->master_log_name[0] ? mi->master_log_name :\
 "FIRST")

/* masks for start/stop operations on io and sql slave threads */
#define SLAVE_IO  1
#define SLAVE_SQL 2

/*
  If the following is set, if first gives an error, second will be
  tried. Otherwise, if first fails, we fail.
*/
#define SLAVE_FORCE_ALL 4

int init_slave();
void init_slave_skip_errors(const char* arg);
bool flush_master_info(MASTER_INFO* mi, bool flush_relay_log_cache);
bool flush_relay_log_info(RELAY_LOG_INFO* rli);
int register_slave_on_master(MYSQL* mysql);
int terminate_slave_threads(MASTER_INFO* mi, int thread_mask,
			     bool skip_lock = 0);
int terminate_slave_thread(THD* thd, pthread_mutex_t* term_mutex,
			   pthread_mutex_t* cond_lock,
			   pthread_cond_t* term_cond,
			   volatile bool* slave_running);
int start_slave_threads(bool need_slave_mutex, bool wait_for_start,
			MASTER_INFO* mi, const char* master_info_fname,
			const char* slave_info_fname, int thread_mask);
/*
  cond_lock is usually same as start_lock. It is needed for the case when
  start_lock is 0 which happens if start_slave_thread() is called already
  inside the start_lock section, but at the same time we want a
  pthread_cond_wait() on start_cond,start_lock
*/
int start_slave_thread(pthread_handler h_func, pthread_mutex_t* start_lock,
		       pthread_mutex_t *cond_lock,
		       pthread_cond_t* start_cond,
		       volatile bool *slave_running,
		       volatile ulong *slave_run_id,
		       MASTER_INFO* mi,
                       bool high_priority);

/* If fd is -1, dump to NET */
int mysql_table_dump(THD* thd, const char* db,
		     const char* tbl_name, int fd = -1);

/* retrieve table from master and copy to slave*/
int fetch_master_table(THD* thd, const char* db_name, const char* table_name,
		       MASTER_INFO* mi, MYSQL* mysql, bool overwrite);

void table_rule_ent_hash_to_str(String* s, HASH* h);
void table_rule_ent_dynamic_array_to_str(String* s, DYNAMIC_ARRAY* a);
int show_master_info(THD* thd, MASTER_INFO* mi);
int show_binlog_info(THD* thd);

/* See if the query uses any tables that should not be replicated */
int tables_ok(THD* thd, TABLE_LIST* tables);

/*
  Check to see if the database is ok to operate on with respect to the
  do and ignore lists - used in replication
*/
int db_ok(const char* db, I_List<i_string> &do_list,
	  I_List<i_string> &ignore_list );
int db_ok_with_wild_table(const char *db);

int add_table_rule(HASH* h, const char* table_spec);
int add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec);
void init_table_rule_hash(HASH* h, bool* h_inited);
void init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited);
const char *rewrite_db(const char* db, uint *new_db_len);
const char *print_slave_db_safe(const char *db);
int check_expected_error(THD* thd, RELAY_LOG_INFO* rli, int error_code);
void skip_load_data_infile(NET* net);
void slave_print_error(RELAY_LOG_INFO* rli, int err_code, const char* msg, ...);

void end_slave(); /* clean up */
void init_master_info_with_options(MASTER_INFO* mi);
void clear_until_condition(RELAY_LOG_INFO* rli);
void clear_slave_error_timestamp(RELAY_LOG_INFO* rli);
int init_master_info(MASTER_INFO* mi, const char* master_info_fname,
		     const char* slave_info_fname,
		     bool abort_if_no_master_info_file);
void end_master_info(MASTER_INFO* mi);
int init_relay_log_info(RELAY_LOG_INFO* rli, const char* info_fname);
void end_relay_log_info(RELAY_LOG_INFO* rli);
void lock_slave_threads(MASTER_INFO* mi);
void unlock_slave_threads(MASTER_INFO* mi);
void init_thread_mask(int* mask,MASTER_INFO* mi,bool inverse);
int init_relay_log_pos(RELAY_LOG_INFO* rli,const char* log,ulonglong pos,
		       bool need_data_lock, const char** errmsg);

int purge_relay_logs(RELAY_LOG_INFO* rli, THD *thd, bool just_reset,
		     const char** errmsg);
void rotate_relay_log(MASTER_INFO* mi);

extern "C" pthread_handler_decl(handle_slave_io,arg);
extern "C" pthread_handler_decl(handle_slave_sql,arg);
extern bool volatile abort_loop;
extern MASTER_INFO main_mi, *active_mi; /* active_mi for multi-master */
extern LIST master_list;
extern HASH replicate_do_table, replicate_ignore_table;
extern DYNAMIC_ARRAY  replicate_wild_do_table, replicate_wild_ignore_table;
extern bool do_table_inited, ignore_table_inited,
	    wild_do_table_inited, wild_ignore_table_inited;
extern bool table_rules_on, replicate_same_server_id;

extern int disconnect_slave_event_count, abort_slave_event_count ;

/* the master variables are defaults read from my.cnf or command line */
extern uint master_port, master_connect_retry, report_port;
extern my_string master_user, master_password, master_host,
       master_info_file, relay_log_info_file, report_user, report_host,
       report_password;

extern my_bool master_ssl;
extern my_string master_ssl_ca, master_ssl_capath, master_ssl_cert,
       master_ssl_cipher, master_ssl_key;
       
extern I_List<i_string> replicate_do_db, replicate_ignore_db;
extern I_List<i_string_pair> replicate_rewrite_db;
extern I_List<THD> threads;

#endif
#else
#define SLAVE_IO  1
#define SLAVE_SQL 2
#endif /* HAVE_REPLICATION */
