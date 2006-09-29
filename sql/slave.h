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

#ifndef SLAVE_H
#define SLAVE_H

#ifdef HAVE_REPLICATION

#include "log.h"
#include "my_list.h"
#include "rpl_filter.h"
#include "rpl_tblmap.h"
#include "rpl_rli.h"

#define SLAVE_NET_TIMEOUT  3600

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
  position) and relay log (MYSQL_BIN_LOG object).

  In RELAY_LOG_INFO: run_lock, data_lock
  see MASTER_INFO
  
  Order of acquisition: if you want to have LOCK_active_mi and a run_lock, you
  must acquire LOCK_active_mi first.

  In MYSQL_BIN_LOG: LOCK_log, LOCK_index of the binlog and the relay log
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

/*
  3 possible values for MASTER_INFO::slave_running and
  RELAY_LOG_INFO::slave_running.
  The values 0,1,2 are very important: to keep the diff small, I didn't
  substitute places where we use 0/1 with the newly defined symbols. So don't change
  these values.
  The same way, code is assuming that in RELAY_LOG_INFO we use only values
  0/1.
  I started with using an enum, but
  enum_variable=1; is not legal so would have required many line changes.
*/
#define MYSQL_SLAVE_NOT_RUN         0
#define MYSQL_SLAVE_RUN_NOT_CONNECT 1
#define MYSQL_SLAVE_RUN_CONNECT     2

static Log_event* next_event(RELAY_LOG_INFO* rli);

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

  st_master_info()
    :ssl(0), fd(-1),  io_thd(0), inited(0),
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

#define RPL_LOG_NAME (rli->group_master_log_name[0] ? rli->group_master_log_name :\
 "FIRST")
#define IO_RPL_LOG_NAME (mi->master_log_name[0] ? mi->master_log_name :\
 "FIRST")

/*
  If the following is set, if first gives an error, second will be
  tried. Otherwise, if first fails, we fail.
*/
#define SLAVE_FORCE_ALL 4

int init_slave();
void init_slave_skip_errors(const char* arg);
int flush_master_info(MASTER_INFO* mi, bool flush_relay_log_cache);
bool flush_relay_log_info(RELAY_LOG_INFO* rli);
int register_slave_on_master(MYSQL* mysql);
int terminate_slave_threads(MASTER_INFO* mi, int thread_mask,
			     bool skip_lock = 0);
int terminate_slave_thread(THD* thd, pthread_mutex_t* term_mutex,
			   pthread_mutex_t* cond_lock,
			   pthread_cond_t* term_cond,
			   volatile uint* slave_running);
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
		       volatile uint *slave_running,
		       volatile ulong *slave_run_id,
		       MASTER_INFO* mi,
                       bool high_priority);

/* If fd is -1, dump to NET */
int mysql_table_dump(THD* thd, const char* db,
		     const char* tbl_name, int fd = -1);

/* retrieve table from master and copy to slave*/
int fetch_master_table(THD* thd, const char* db_name, const char* table_name,
		       MASTER_INFO* mi, MYSQL* mysql, bool overwrite);

bool show_master_info(THD* thd, MASTER_INFO* mi);
bool show_binlog_info(THD* thd);

const char *print_slave_db_safe(const char *db);
int check_expected_error(THD* thd, RELAY_LOG_INFO* rli, int error_code);
void skip_load_data_infile(NET* net);
void slave_print_msg(enum loglevel level, RELAY_LOG_INFO* rli,
                     int err_code, const char* msg, ...)
  ATTRIBUTE_FORMAT(printf, 4, 5);

void end_slave(); /* clean up */
void init_master_info_with_options(MASTER_INFO* mi);
void clear_until_condition(RELAY_LOG_INFO* rli);
void clear_slave_error(RELAY_LOG_INFO* rli);
int init_master_info(MASTER_INFO* mi, const char* master_info_fname,
		     const char* slave_info_fname,
		     bool abort_if_no_master_info_file,
		     int thread_mask);
void end_master_info(MASTER_INFO* mi);
void end_relay_log_info(RELAY_LOG_INFO* rli);
void lock_slave_threads(MASTER_INFO* mi);
void unlock_slave_threads(MASTER_INFO* mi);
void init_thread_mask(int* mask,MASTER_INFO* mi,bool inverse);
int init_relay_log_pos(RELAY_LOG_INFO* rli,const char* log,ulonglong pos,
		       bool need_data_lock, const char** errmsg,
                       bool look_for_description_event);

int purge_relay_logs(RELAY_LOG_INFO* rli, THD *thd, bool just_reset,
		     const char** errmsg);
void set_slave_thread_options(THD* thd);
void set_slave_thread_default_charset(THD* thd, RELAY_LOG_INFO *rli);
void rotate_relay_log(MASTER_INFO* mi);

pthread_handler_t handle_slave_io(void *arg);
pthread_handler_t handle_slave_sql(void *arg);
extern bool volatile abort_loop;
extern MASTER_INFO main_mi, *active_mi; /* active_mi for multi-master */
extern LIST master_list;
extern my_bool replicate_same_server_id;

extern int disconnect_slave_event_count, abort_slave_event_count ;

/* the master variables are defaults read from my.cnf or command line */
extern uint master_port, master_connect_retry, report_port;
extern my_string master_user, master_password, master_host,
       master_info_file, relay_log_info_file, report_user, report_host,
       report_password;

extern my_bool master_ssl;
extern my_string master_ssl_ca, master_ssl_capath, master_ssl_cert,
       master_ssl_cipher, master_ssl_key;
       
extern I_List<THD> threads;

#endif /* HAVE_REPLICATION */

/* masks for start/stop operations on io and sql slave threads */
#define SLAVE_IO  1
#define SLAVE_SQL 2

#endif


