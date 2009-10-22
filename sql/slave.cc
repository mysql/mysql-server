/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/**
  @addtogroup Replication
  @{

  @file

  @brief Code to run the io thread and the sql thread on the
  replication slave.
*/

#include "mysql_priv.h"

#include <mysql.h>
#include <myisam.h>
#include "slave.h"
#include "rpl_mi.h"
#include "rpl_rli.h"
#include "sql_repl.h"
#include "rpl_filter.h"
#include "repl_failsafe.h"
#include <thr_alarm.h>
#include <my_dir.h>
#include <sql_common.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include <mysys_err.h>

#ifdef HAVE_REPLICATION

#include "rpl_tblmap.h"

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")

#define MAX_SLAVE_RETRY_PAUSE 5
bool use_slave_mask = 0;
MY_BITMAP slave_error_mask;
char slave_skip_error_names[SHOW_VAR_FUNC_BUFF_SIZE];

typedef bool (*CHECK_KILLED_FUNC)(THD*,void*);

char* slave_load_tmpdir = 0;
Master_info *active_mi= 0;
my_bool replicate_same_server_id;
ulonglong relay_log_space_limit = 0;

/*
  When slave thread exits, we need to remember the temporary tables so we
  can re-use them on slave start.

  TODO: move the vars below under Master_info
*/

int disconnect_slave_event_count = 0, abort_slave_event_count = 0;
int events_till_abort = -1;

enum enum_slave_reconnect_actions
{
  SLAVE_RECON_ACT_REG= 0,
  SLAVE_RECON_ACT_DUMP= 1,
  SLAVE_RECON_ACT_EVENT= 2,
  SLAVE_RECON_ACT_MAX
};

enum enum_slave_reconnect_messages
{
  SLAVE_RECON_MSG_WAIT= 0,
  SLAVE_RECON_MSG_KILLED_WAITING= 1,
  SLAVE_RECON_MSG_AFTER= 2,
  SLAVE_RECON_MSG_FAILED= 3,
  SLAVE_RECON_MSG_COMMAND= 4,
  SLAVE_RECON_MSG_KILLED_AFTER= 5,
  SLAVE_RECON_MSG_MAX
};

static const char *reconnect_messages[SLAVE_RECON_ACT_MAX][SLAVE_RECON_MSG_MAX]=
{
  {
    "Waiting to reconnect after a failed registration on master",
    "Slave I/O thread killed while waitnig to reconnect after a failed \
registration on master",
    "Reconnecting after a failed registration on master",
    "failed registering on master, reconnecting to try again, \
log '%s' at postion %s",
    "COM_REGISTER_SLAVE",
    "Slave I/O thread killed during or after reconnect"
  },
  {
    "Waiting to reconnect after a failed binlog dump request",
    "Slave I/O thread killed while retrying master dump",
    "Reconnecting after a failed binlog dump request",
    "failed dump request, reconnecting to try again, log '%s' at postion %s",
    "COM_BINLOG_DUMP",
    "Slave I/O thread killed during or after reconnect"
  },
  {
    "Waiting to reconnect after a failed master event read",
    "Slave I/O thread killed while waiting to reconnect after a failed read",
    "Reconnecting after a failed master event read",
    "Slave I/O thread: Failed reading log event, reconnecting to retry, \
log '%s' at postion %s",
    "",
    "Slave I/O thread killed during or after a reconnect done to recover from \
failed read"
  }
};
 

typedef enum { SLAVE_THD_IO, SLAVE_THD_SQL} SLAVE_THD_TYPE;

static int process_io_rotate(Master_info* mi, Rotate_log_event* rev);
static int process_io_create_file(Master_info* mi, Create_file_log_event* cev);
static bool wait_for_relay_log_space(Relay_log_info* rli);
static inline bool io_slave_killed(THD* thd,Master_info* mi);
static inline bool sql_slave_killed(THD* thd,Relay_log_info* rli);
static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type);
static void print_slave_skip_errors(void);
static int safe_connect(THD* thd, MYSQL* mysql, Master_info* mi);
static int safe_reconnect(THD* thd, MYSQL* mysql, Master_info* mi,
                          bool suppress_warnings);
static int connect_to_master(THD* thd, MYSQL* mysql, Master_info* mi,
                             bool reconnect, bool suppress_warnings);
static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
                      void* thread_killed_arg);
static int request_table_dump(MYSQL* mysql, const char* db, const char* table);
static int create_table_from_dump(THD* thd, MYSQL *mysql, const char* db,
                                  const char* table_name, bool overwrite);
static int get_master_version_and_clock(MYSQL* mysql, Master_info* mi);
static Log_event* next_event(Relay_log_info* rli);
static int queue_event(Master_info* mi,const char* buf,ulong event_len);
static int terminate_slave_thread(THD *thd,
                                  pthread_mutex_t *term_lock,
                                  pthread_cond_t *term_cond,
                                  volatile uint *slave_running,
                                  bool skip_lock);
static bool check_io_slave_killed(THD *thd, Master_info *mi, const char *info);

/*
  Find out which replications threads are running

  SYNOPSIS
    init_thread_mask()
    mask                Return value here
    mi                  master_info for slave
    inverse             If set, returns which threads are not running

  IMPLEMENTATION
    Get a bit mask for which threads are running so that we can later restart
    these threads.

  RETURN
    mask        If inverse == 0, running threads
                If inverse == 1, stopped threads
*/

void init_thread_mask(int* mask,Master_info* mi,bool inverse)
{
  bool set_io = mi->slave_running, set_sql = mi->rli.slave_running;
  register int tmp_mask=0;
  DBUG_ENTER("init_thread_mask");

  if (set_io)
    tmp_mask |= SLAVE_IO;
  if (set_sql)
    tmp_mask |= SLAVE_SQL;
  if (inverse)
    tmp_mask^= (SLAVE_IO | SLAVE_SQL);
  *mask = tmp_mask;
  DBUG_VOID_RETURN;
}


/*
  lock_slave_threads()
*/

void lock_slave_threads(Master_info* mi)
{
  DBUG_ENTER("lock_slave_threads");

  //TODO: see if we can do this without dual mutex
  pthread_mutex_lock(&mi->run_lock);
  pthread_mutex_lock(&mi->rli.run_lock);
  DBUG_VOID_RETURN;
}


/*
  unlock_slave_threads()
*/

void unlock_slave_threads(Master_info* mi)
{
  DBUG_ENTER("unlock_slave_threads");

  //TODO: see if we can do this without dual mutex
  pthread_mutex_unlock(&mi->rli.run_lock);
  pthread_mutex_unlock(&mi->run_lock);
  DBUG_VOID_RETURN;
}


/* Initialize slave structures */

int init_slave()
{
  DBUG_ENTER("init_slave");

  /*
    This is called when mysqld starts. Before client connections are
    accepted. However bootstrap may conflict with us if it does START SLAVE.
    So it's safer to take the lock.
  */
  pthread_mutex_lock(&LOCK_active_mi);
  /*
    TODO: re-write this to interate through the list of files
    for multi-master
  */
  active_mi= new Master_info;

  /*
    If --slave-skip-errors=... was not used, the string value for the
    system variable has not been set up yet. Do it now.
  */
  if (!use_slave_mask)
  {
    print_slave_skip_errors();
  }

  /*
    If master_host is not specified, try to read it from the master_info file.
    If master_host is specified, create the master_info file if it doesn't
    exists.
  */
  if (!active_mi)
  {
    sql_print_error("Failed to allocate memory for the master info structure");
    goto err;
  }

  if (init_master_info(active_mi,master_info_file,relay_log_info_file,
                       !master_host, (SLAVE_IO | SLAVE_SQL)))
  {
    sql_print_error("Failed to initialize the master info structure");
    goto err;
  }

  if (server_id && !master_host && active_mi->host[0])
    master_host= active_mi->host;

  /* If server id is not set, start_slave_thread() will say it */

  if (master_host && !opt_skip_slave_start)
  {
    if (start_slave_threads(1 /* need mutex */,
                            0 /* no wait for start*/,
                            active_mi,
                            master_info_file,
                            relay_log_info_file,
                            SLAVE_IO | SLAVE_SQL))
    {
      sql_print_error("Failed to create slave threads");
      goto err;
    }
  }
  pthread_mutex_unlock(&LOCK_active_mi);
  DBUG_RETURN(0);

err:
  pthread_mutex_unlock(&LOCK_active_mi);
  DBUG_RETURN(1);
}


/**
  Convert slave skip errors bitmap into a printable string.
*/

static void print_slave_skip_errors(void)
{
  /*
    To be safe, we want 10 characters of room in the buffer for a number
    plus terminators. Also, we need some space for constant strings.
    10 characters must be sufficient for a number plus {',' | '...'}
    plus a NUL terminator. That is a max 6 digit number.
  */
  const size_t MIN_ROOM= 10;
  DBUG_ENTER("print_slave_skip_errors");
  DBUG_ASSERT(sizeof(slave_skip_error_names) > MIN_ROOM);
  DBUG_ASSERT(MAX_SLAVE_ERROR <= 999999); // 6 digits

  if (!use_slave_mask || bitmap_is_clear_all(&slave_error_mask))
  {
    /* purecov: begin tested */
    memcpy(slave_skip_error_names, STRING_WITH_LEN("OFF"));
    /* purecov: end */
  }
  else if (bitmap_is_set_all(&slave_error_mask))
  {
    /* purecov: begin tested */
    memcpy(slave_skip_error_names, STRING_WITH_LEN("ALL"));
    /* purecov: end */
  }
  else
  {
    char *buff= slave_skip_error_names;
    char *bend= buff + sizeof(slave_skip_error_names);
    int  errnum;

    for (errnum= 0; errnum < MAX_SLAVE_ERROR; errnum++)
    {
      if (bitmap_is_set(&slave_error_mask, errnum))
      {
        if (buff + MIN_ROOM >= bend)
          break; /* purecov: tested */
        buff= int10_to_str(errnum, buff, 10);
        *buff++= ',';
      }
    }
    if (buff != slave_skip_error_names)
      buff--; // Remove last ','
    if (errnum < MAX_SLAVE_ERROR)
    {
      /* Couldn't show all errors */
      buff= strmov(buff, "..."); /* purecov: tested */
    }
    *buff=0;
  }
  DBUG_PRINT("init", ("error_names: '%s'", slave_skip_error_names));
  DBUG_VOID_RETURN;
}

/*
  Init function to set up array for errors that should be skipped for slave

  SYNOPSIS
    init_slave_skip_errors()
    arg         List of errors numbers to skip, separated with ','

  NOTES
    Called from get_options() in mysqld.cc on start-up
*/

void init_slave_skip_errors(const char* arg)
{
  const char *p;
  DBUG_ENTER("init_slave_skip_errors");

  if (bitmap_init(&slave_error_mask,0,MAX_SLAVE_ERROR,0))
  {
    fprintf(stderr, "Badly out of memory, please check your system status\n");
    exit(1);
  }
  use_slave_mask = 1;
  for (;my_isspace(system_charset_info,*arg);++arg)
    /* empty */;
  if (!my_strnncoll(system_charset_info,(uchar*)arg,4,(const uchar*)"all",4))
  {
    bitmap_set_all(&slave_error_mask);
    print_slave_skip_errors();
    DBUG_VOID_RETURN;
  }
  for (p= arg ; *p; )
  {
    long err_code;
    if (!(p= str2int(p, 10, 0, LONG_MAX, &err_code)))
      break;
    if (err_code < MAX_SLAVE_ERROR)
       bitmap_set_bit(&slave_error_mask,(uint)err_code);
    while (!my_isdigit(system_charset_info,*p) && *p)
      p++;
  }
  /* Convert slave skip errors bitmap into a printable string. */
  print_slave_skip_errors();
  DBUG_VOID_RETURN;
}

static void set_thd_in_use_temporary_tables(Relay_log_info *rli)
{
  TABLE *table;

  for (table= rli->save_temporary_tables ; table ; table= table->next)
    table->in_use= rli->sql_thd;
}

int terminate_slave_threads(Master_info* mi,int thread_mask,bool skip_lock)
{
  DBUG_ENTER("terminate_slave_threads");

  if (!mi->inited)
    DBUG_RETURN(0); /* successfully do nothing */
  int error,force_all = (thread_mask & SLAVE_FORCE_ALL);
  pthread_mutex_t *sql_lock = &mi->rli.run_lock, *io_lock = &mi->run_lock;

  if (thread_mask & (SLAVE_IO|SLAVE_FORCE_ALL))
  {
    DBUG_PRINT("info",("Terminating IO thread"));
    mi->abort_slave=1;
    if ((error=terminate_slave_thread(mi->io_thd, io_lock,
                                      &mi->stop_cond,
                                      &mi->slave_running,
                                      skip_lock)) &&
        !force_all)
      DBUG_RETURN(error);
  }
  if (thread_mask & (SLAVE_SQL|SLAVE_FORCE_ALL))
  {
    DBUG_PRINT("info",("Terminating SQL thread"));
    mi->rli.abort_slave=1;
    if ((error=terminate_slave_thread(mi->rli.sql_thd, sql_lock,
                                      &mi->rli.stop_cond,
                                      &mi->rli.slave_running,
                                      skip_lock)) &&
        !force_all)
      DBUG_RETURN(error);
  }
  DBUG_RETURN(0);
}


/**
   Wait for a slave thread to terminate.

   This function is called after requesting the thread to terminate
   (by setting @c abort_slave member of @c Relay_log_info or @c
   Master_info structure to 1). Termination of the thread is
   controlled with the the predicate <code>*slave_running</code>.

   Function will acquire @c term_lock before waiting on the condition
   unless @c skip_lock is true in which case the mutex should be owned
   by the caller of this function and will remain acquired after
   return from the function.

   @param term_lock
          Associated lock to use when waiting for @c term_cond

   @param term_cond
          Condition that is signalled when the thread has terminated

   @param slave_running
          Pointer to predicate to check for slave thread termination

   @param skip_lock
          If @c true the lock will not be acquired before waiting on
          the condition. In this case, it is assumed that the calling
          function acquires the lock before calling this function.

   @retval 0 All OK ER_SLAVE_NOT_RUNNING otherwise.

   @note  If the executing thread has to acquire term_lock (skip_lock
          is false), the negative running status does not represent
          any issue therefore no error is reported.

 */
static int
terminate_slave_thread(THD *thd,
                       pthread_mutex_t *term_lock,
                       pthread_cond_t *term_cond,
                       volatile uint *slave_running,
                       bool skip_lock)
{
  DBUG_ENTER("terminate_slave_thread");
  if (!skip_lock)
  {
    pthread_mutex_lock(term_lock);
  }
  else
  {
    safe_mutex_assert_owner(term_lock);
  }
  if (!*slave_running)
  {
    if (!skip_lock)
    {
      /*
        if run_lock (term_lock) is acquired locally then either
        slave_running status is fine
      */
      pthread_mutex_unlock(term_lock);
      DBUG_RETURN(0);
    }
    else
    {
      DBUG_RETURN(ER_SLAVE_NOT_RUNNING);
    }
  }
  DBUG_ASSERT(thd != 0);
  THD_CHECK_SENTRY(thd);

  /*
    Is is critical to test if the slave is running. Otherwise, we might
    be referening freed memory trying to kick it
  */

  while (*slave_running)                        // Should always be true
  {
    int error;
    DBUG_PRINT("loop", ("killing slave thread"));

    pthread_mutex_lock(&thd->LOCK_thd_data);
#ifndef DONT_USE_THR_ALARM
    /*
      Error codes from pthread_kill are:
      EINVAL: invalid signal number (can't happen)
      ESRCH: thread already killed (can happen, should be ignored)
    */
    IF_DBUG(int err= ) pthread_kill(thd->real_id, thr_client_alarm);
    DBUG_ASSERT(err != EINVAL);
#endif
    thd->awake(THD::NOT_KILLED);
    pthread_mutex_unlock(&thd->LOCK_thd_data);

    /*
      There is a small chance that slave thread might miss the first
      alarm. To protect againts it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(abstime,2);
    error= pthread_cond_timedwait(term_cond, term_lock, &abstime);
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(*slave_running == 0);

  if (!skip_lock)
    pthread_mutex_unlock(term_lock);
  DBUG_RETURN(0);
}


int start_slave_thread(pthread_handler h_func, pthread_mutex_t *start_lock,
                       pthread_mutex_t *cond_lock,
                       pthread_cond_t *start_cond,
                       volatile uint *slave_running,
                       volatile ulong *slave_run_id,
                       Master_info* mi,
                       bool high_priority)
{
  pthread_t th;
  ulong start_id;
  DBUG_ENTER("start_slave_thread");

  DBUG_ASSERT(mi->inited);

  if (start_lock)
    pthread_mutex_lock(start_lock);
  if (!server_id)
  {
    if (start_cond)
      pthread_cond_broadcast(start_cond);
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    sql_print_error("Server id not set, will not start slave");
    DBUG_RETURN(ER_BAD_SLAVE);
  }

  if (*slave_running)
  {
    if (start_cond)
      pthread_cond_broadcast(start_cond);
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    DBUG_RETURN(ER_SLAVE_MUST_STOP);
  }
  start_id= *slave_run_id;
  DBUG_PRINT("info",("Creating new slave thread"));
  if (high_priority)
    my_pthread_attr_setprio(&connection_attrib,CONNECT_PRIOR);
  if (pthread_create(&th, &connection_attrib, h_func, (void*)mi))
  {
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    DBUG_RETURN(ER_SLAVE_THREAD);
  }
  if (start_cond && cond_lock) // caller has cond_lock
  {
    THD* thd = current_thd;
    while (start_id == *slave_run_id)
    {
      DBUG_PRINT("sleep",("Waiting for slave thread to start"));
      const char* old_msg = thd->enter_cond(start_cond,cond_lock,
                                            "Waiting for slave thread to start");
      pthread_cond_wait(start_cond,cond_lock);
      thd->exit_cond(old_msg);
      pthread_mutex_lock(cond_lock); // re-acquire it as exit_cond() released
      if (thd->killed)
        DBUG_RETURN(thd->killed_errno());
    }
  }
  if (start_lock)
    pthread_mutex_unlock(start_lock);
  DBUG_RETURN(0);
}


/*
  start_slave_threads()

  NOTES
    SLAVE_FORCE_ALL is not implemented here on purpose since it does not make
    sense to do that for starting a slave--we always care if it actually
    started the threads that were not previously running
*/

int start_slave_threads(bool need_slave_mutex, bool wait_for_start,
                        Master_info* mi, const char* master_info_fname,
                        const char* slave_info_fname, int thread_mask)
{
  pthread_mutex_t *lock_io=0,*lock_sql=0,*lock_cond_io=0,*lock_cond_sql=0;
  pthread_cond_t* cond_io=0,*cond_sql=0;
  int error=0;
  DBUG_ENTER("start_slave_threads");

  if (need_slave_mutex)
  {
    lock_io = &mi->run_lock;
    lock_sql = &mi->rli.run_lock;
  }
  if (wait_for_start)
  {
    cond_io = &mi->start_cond;
    cond_sql = &mi->rli.start_cond;
    lock_cond_io = &mi->run_lock;
    lock_cond_sql = &mi->rli.run_lock;
  }

  if (thread_mask & SLAVE_IO)
    error=start_slave_thread(handle_slave_io,lock_io,lock_cond_io,
                             cond_io,
                             &mi->slave_running, &mi->slave_run_id,
                             mi, 1); //high priority, to read the most possible
  if (!error && (thread_mask & SLAVE_SQL))
  {
    error=start_slave_thread(handle_slave_sql,lock_sql,lock_cond_sql,
                             cond_sql,
                             &mi->rli.slave_running, &mi->rli.slave_run_id,
                             mi, 0);
    if (error)
      terminate_slave_threads(mi, thread_mask & SLAVE_IO, !need_slave_mutex);
  }
  DBUG_RETURN(error);
}


#ifdef NOT_USED_YET
static int end_slave_on_walk(Master_info* mi, uchar* /*unused*/)
{
  DBUG_ENTER("end_slave_on_walk");

  end_master_info(mi);
  DBUG_RETURN(0);
}
#endif


/*
  Release slave threads at time of executing shutdown.

  SYNOPSIS
    end_slave()
*/

void end_slave()
{
  DBUG_ENTER("end_slave");

  /*
    This is called when the server terminates, in close_connections().
    It terminates slave threads. However, some CHANGE MASTER etc may still be
    running presently. If a START SLAVE was in progress, the mutex lock below
    will make us wait until slave threads have started, and START SLAVE
    returns, then we terminate them here.
  */
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    /*
      TODO: replace the line below with
      list_walk(&master_list, (list_walk_action)end_slave_on_walk,0);
      once multi-master code is ready.
    */
    terminate_slave_threads(active_mi,SLAVE_FORCE_ALL);
  }
  pthread_mutex_unlock(&LOCK_active_mi);
  DBUG_VOID_RETURN;
}

/**
   Free all resources used by slave threads at time of executing shutdown.
   The routine must be called after all possible users of @c active_mi
   have left.

   SYNOPSIS
     close_active_mi()

*/
void close_active_mi()
{
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    end_master_info(active_mi);
    delete active_mi;
    active_mi= 0;
  }
  pthread_mutex_unlock(&LOCK_active_mi);
}

static bool io_slave_killed(THD* thd, Master_info* mi)
{
  DBUG_ENTER("io_slave_killed");

  DBUG_ASSERT(mi->io_thd == thd);
  DBUG_ASSERT(mi->slave_running); // tracking buffer overrun
  DBUG_RETURN(mi->abort_slave || abort_loop || thd->killed);
}


static bool sql_slave_killed(THD* thd, Relay_log_info* rli)
{
  DBUG_ENTER("sql_slave_killed");

  DBUG_ASSERT(rli->sql_thd == thd);
  DBUG_ASSERT(rli->slave_running == 1);// tracking buffer overrun
  if (abort_loop || thd->killed || rli->abort_slave)
  {
    if (rli->abort_slave && rli->is_in_group() &&
        thd->transaction.all.modified_non_trans_table)
      DBUG_RETURN(0);
    /*
      If we are in an unsafe situation (stopping could corrupt replication),
      we give one minute to the slave SQL thread of grace before really
      terminating, in the hope that it will be able to read more events and
      the unsafe situation will soon be left. Note that this one minute starts
      from the last time anything happened in the slave SQL thread. So it's
      really one minute of idleness, we don't timeout if the slave SQL thread
      is actively working.
    */
    if (rli->last_event_start_time == 0)
      DBUG_RETURN(1);
    DBUG_PRINT("info", ("Slave SQL thread is in an unsafe situation, giving "
                        "it some grace period"));
    if (difftime(time(0), rli->last_event_start_time) > 60)
    {
      rli->report(ERROR_LEVEL, 0,
                  "SQL thread had to stop in an unsafe situation, in "
                  "the middle of applying updates to a "
                  "non-transactional table without any primary key. "
                  "There is a risk of duplicate updates when the slave "
                  "SQL thread is restarted. Please check your tables' "
                  "contents after restart.");
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


/*
  skip_load_data_infile()

  NOTES
    This is used to tell a 3.23 master to break send_file()
*/

void skip_load_data_infile(NET *net)
{
  DBUG_ENTER("skip_load_data_infile");

  (void)net_request_file(net, "/dev/null");
  (void)my_net_read(net);                               // discard response
  (void)net_write_command(net, 0, (uchar*) "", 0, (uchar*) "", 0); // ok
  DBUG_VOID_RETURN;
}


bool net_request_file(NET* net, const char* fname)
{
  DBUG_ENTER("net_request_file");
  DBUG_RETURN(net_write_command(net, 251, (uchar*) fname, strlen(fname),
                                (uchar*) "", 0));
}

/*
  From other comments and tests in code, it looks like
  sometimes Query_log_event and Load_log_event can have db == 0
  (see rewrite_db() above for example)
  (cases where this happens are unclear; it may be when the master is 3.23).
*/

const char *print_slave_db_safe(const char* db)
{
  DBUG_ENTER("*print_slave_db_safe");

  DBUG_RETURN((db ? db : ""));
}

int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
                                 const char *default_val)
{
  uint length;
  DBUG_ENTER("init_strvar_from_file");

  if ((length=my_b_gets(f,var, max_size)))
  {
    char* last_p = var + length -1;
    if (*last_p == '\n')
      *last_p = 0; // if we stopped on newline, kill it
    else
    {
      /*
        If we truncated a line or stopped on last char, remove all chars
        up to and including newline.
      */
      int c;
      while (((c=my_b_get(f)) != '\n' && c != my_b_EOF)) ;
    }
    DBUG_RETURN(0);
  }
  else if (default_val)
  {
    strmake(var,  default_val, max_size-1);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


int init_intvar_from_file(int* var, IO_CACHE* f, int default_val)
{
  char buf[32];
  DBUG_ENTER("init_intvar_from_file");


  if (my_b_gets(f, buf, sizeof(buf)))
  {
    *var = atoi(buf);
    DBUG_RETURN(0);
  }
  else if (default_val)
  {
    *var = default_val;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


/*
  Check if the error is caused by network.
  @param[in]   errorno   Number of the error.
  RETURNS:
  TRUE         network error
  FALSE        not network error
*/

bool is_network_error(uint errorno)
{ 
  if (errorno == CR_CONNECTION_ERROR || 
      errorno == CR_CONN_HOST_ERROR ||
      errorno == CR_SERVER_GONE_ERROR ||
      errorno == CR_SERVER_LOST ||
      errorno == ER_CON_COUNT_ERROR ||
      errorno == ER_SERVER_SHUTDOWN)
    return TRUE;

  return FALSE;   
}


/*
  Note that we rely on the master's version (3.23, 4.0.14 etc) instead of
  relying on the binlog's version. This is not perfect: imagine an upgrade
  of the master without waiting that all slaves are in sync with the master;
  then a slave could be fooled about the binlog's format. This is what happens
  when people upgrade a 3.23 master to 4.0 without doing RESET MASTER: 4.0
  slaves are fooled. So we do this only to distinguish between 3.23 and more
  recent masters (it's too late to change things for 3.23).

  RETURNS
  0       ok
  1       error
  2       transient network problem, the caller should try to reconnect
*/

static int get_master_version_and_clock(MYSQL* mysql, Master_info* mi)
{
  char err_buff[MAX_SLAVE_ERRMSG];
  const char* errmsg= 0;
  int err_code= 0;
  MYSQL_RES *master_res= 0;
  MYSQL_ROW master_row;
  DBUG_ENTER("get_master_version_and_clock");

  /*
    Free old description_event_for_queue (that is needed if we are in
    a reconnection).
  */
  delete mi->rli.relay_log.description_event_for_queue;
  mi->rli.relay_log.description_event_for_queue= 0;

  if (!my_isdigit(&my_charset_bin,*mysql->server_version))
  {
    errmsg = "Master reported unrecognized MySQL version";
    err_code= ER_SLAVE_FATAL_ERROR;
    sprintf(err_buff, ER(err_code), errmsg);
  }
  else
  {
    /*
      Note the following switch will bug when we have MySQL branch 30 ;)
    */
    switch (*mysql->server_version)
    {
    case '0':
    case '1':
    case '2':
      errmsg = "Master reported unrecognized MySQL version";
      err_code= ER_SLAVE_FATAL_ERROR;
      sprintf(err_buff, ER(err_code), errmsg);
      break;
    case '3':
      mi->rli.relay_log.description_event_for_queue= new
        Format_description_log_event(1, mysql->server_version);
      break;
    case '4':
      mi->rli.relay_log.description_event_for_queue= new
        Format_description_log_event(3, mysql->server_version);
      break;
    default:
      /*
        Master is MySQL >=5.0. Give a default Format_desc event, so that we can
        take the early steps (like tests for "is this a 3.23 master") which we
        have to take before we receive the real master's Format_desc which will
        override this one. Note that the Format_desc we create below is garbage
        (it has the format of the *slave*); it's only good to help know if the
        master is 3.23, 4.0, etc.
      */
      mi->rli.relay_log.description_event_for_queue= new
        Format_description_log_event(4, mysql->server_version);
      break;
    }
  }

  /*
     This does not mean that a 5.0 slave will be able to read a 6.0 master; but
     as we don't know yet, we don't want to forbid this for now. If a 5.0 slave
     can't read a 6.0 master, this will show up when the slave can't read some
     events sent by the master, and there will be error messages.
  */

  if (errmsg)
    goto err;

  /* as we are here, we tried to allocate the event */
  if (!mi->rli.relay_log.description_event_for_queue)
  {
    errmsg= "default Format_description_log_event";
    err_code= ER_SLAVE_CREATE_EVENT_FAILURE;
    sprintf(err_buff, ER(err_code), errmsg);
    goto err;
  }

  /*
    Compare the master and slave's clock. Do not die if master's clock is
    unavailable (very old master not supporting UNIX_TIMESTAMP()?).
  */

  DBUG_SYNC_POINT("debug_lock.before_get_UNIX_TIMESTAMP", 10);
  master_res= NULL;
  if (!mysql_real_query(mysql, STRING_WITH_LEN("SELECT UNIX_TIMESTAMP()")) &&
      (master_res= mysql_store_result(mysql)) &&
      (master_row= mysql_fetch_row(master_res)))
  {
    mi->clock_diff_with_master=
      (long) (time((time_t*) 0) - strtoul(master_row[0], 0, 10));
  }
  else if (is_network_error(mysql_errno(mysql)))
  {
    mi->report(WARNING_LEVEL, mysql_errno(mysql),
               "Get master clock failed with error: %s", mysql_error(mysql));
    goto network_err;
  }
  else 
  {
    mi->clock_diff_with_master= 0; /* The "most sensible" value */
    sql_print_warning("\"SELECT UNIX_TIMESTAMP()\" failed on master, "
                      "do not trust column Seconds_Behind_Master of SHOW "
                      "SLAVE STATUS. Error: %s (%d)",
                      mysql_error(mysql), mysql_errno(mysql));
  }
  if (master_res)
  {
    mysql_free_result(master_res);
    master_res= NULL;
  }

  /*
    Check that the master's server id and ours are different. Because if they
    are equal (which can result from a simple copy of master's datadir to slave,
    thus copying some my.cnf), replication will work but all events will be
    skipped.
    Do not die if SHOW VARIABLES LIKE 'SERVER_ID' fails on master (very old
    master?).
    Note: we could have put a @@SERVER_ID in the previous SELECT
    UNIX_TIMESTAMP() instead, but this would not have worked on 3.23 masters.
  */
  DBUG_SYNC_POINT("debug_lock.before_get_SERVER_ID", 10);
  master_res= NULL;
  master_row= NULL;
  if (!mysql_real_query(mysql,
                        STRING_WITH_LEN("SHOW VARIABLES LIKE 'SERVER_ID'")) &&
      (master_res= mysql_store_result(mysql)) &&
      (master_row= mysql_fetch_row(master_res)))
  {
    if ((::server_id == strtoul(master_row[1], 0, 10)) &&
        !mi->rli.replicate_same_server_id)
    {
      errmsg= "The slave I/O thread stops because master and slave have equal \
MySQL server ids; these ids must be different for replication to work (or \
the --replicate-same-server-id option must be used on slave but this does \
not always make sense; please check the manual before using it).";
      err_code= ER_SLAVE_FATAL_ERROR;
      sprintf(err_buff, ER(err_code), errmsg);
      goto err;
    }
  }
  else if (mysql_errno(mysql))
  {
    if (is_network_error(mysql_errno(mysql)))
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get master SERVER_ID failed with error: %s", mysql_error(mysql));
      goto network_err;
    }
    /* Fatal error */
    errmsg= "The slave I/O thread stops because a fatal error is encountered \
when it try to get the value of SERVER_ID variable from master.";
    err_code= mysql_errno(mysql);
    sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
    goto err;
  }
  else if (!master_row && master_res)
  {
    mi->report(WARNING_LEVEL, ER_UNKNOWN_SYSTEM_VARIABLE,
               "Unknown system variable 'SERVER_ID' on master, \
maybe it is a *VERY OLD MASTER*.");
  }
  if (master_res)
  {
    mysql_free_result(master_res);
    master_res= NULL;
  }

  /*
    Check that the master's global character_set_server and ours are the same.
    Not fatal if query fails (old master?).
    Note that we don't check for equality of global character_set_client and
    collation_connection (neither do we prevent their setting in
    set_var.cc). That's because from what I (Guilhem) have tested, the global
    values of these 2 are never used (new connections don't use them).
    We don't test equality of global collation_database either as it's is
    going to be deprecated (made read-only) in 4.1 very soon.
    The test is only relevant if master < 5.0.3 (we'll test only if it's older
    than the 5 branch; < 5.0.3 was alpha...), as >= 5.0.3 master stores
    charset info in each binlog event.
    We don't do it for 3.23 because masters <3.23.50 hang on
    SELECT @@unknown_var (BUG#7965 - see changelog of 3.23.50). So finally we
    test only if master is 4.x.
  */

  /* redundant with rest of code but safer against later additions */
  if (*mysql->server_version == '3')
    goto err;

  if (*mysql->server_version == '4')
  {
    master_res= NULL;
    if (!mysql_real_query(mysql,
                          STRING_WITH_LEN("SELECT @@GLOBAL.COLLATION_SERVER")) &&
        (master_res= mysql_store_result(mysql)) &&
        (master_row= mysql_fetch_row(master_res)))
    {
      if (strcmp(master_row[0], global_system_variables.collation_server->name))
      {
        errmsg= "The slave I/O thread stops because master and slave have \
different values for the COLLATION_SERVER global variable. The values must \
be equal for the Statement-format replication to work";
        err_code= ER_SLAVE_FATAL_ERROR;
        sprintf(err_buff, ER(err_code), errmsg);
        goto err;
      }
    }
    else if (is_network_error(mysql_errno(mysql)))
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get master COLLATION_SERVER failed with error: %s", mysql_error(mysql));
      goto network_err;
    }
    else if (mysql_errno(mysql) != ER_UNKNOWN_SYSTEM_VARIABLE)
    {
      /* Fatal error */
      errmsg= "The slave I/O thread stops because a fatal error is encountered \
when it try to get the value of COLLATION_SERVER global variable from master.";
      err_code= mysql_errno(mysql);
      sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
      goto err;
    }
    else
      mi->report(WARNING_LEVEL, ER_UNKNOWN_SYSTEM_VARIABLE,
                 "Unknown system variable 'COLLATION_SERVER' on master, \
maybe it is a *VERY OLD MASTER*. *NOTE*: slave may experience \
inconsistency if replicated data deals with collation.");

    if (master_res)
    {
      mysql_free_result(master_res);
      master_res= NULL;
    }
  }

  /*
    Perform analogous check for time zone. Theoretically we also should
    perform check here to verify that SYSTEM time zones are the same on
    slave and master, but we can't rely on value of @@system_time_zone
    variable (it is time zone abbreviation) since it determined at start
    time and so could differ for slave and master even if they are really
    in the same system time zone. So we are omiting this check and just
    relying on documentation. Also according to Monty there are many users
    who are using replication between servers in various time zones. Hence
    such check will broke everything for them. (And now everything will
    work for them because by default both their master and slave will have
    'SYSTEM' time zone).
    This check is only necessary for 4.x masters (and < 5.0.4 masters but
    those were alpha).
  */
  if (*mysql->server_version == '4')
  {
    master_res= NULL;
    if (!mysql_real_query(mysql, STRING_WITH_LEN("SELECT @@GLOBAL.TIME_ZONE")) &&
        (master_res= mysql_store_result(mysql)) &&
        (master_row= mysql_fetch_row(master_res)))
    {
      if (strcmp(master_row[0],
                 global_system_variables.time_zone->get_name()->ptr()))
      {
        errmsg= "The slave I/O thread stops because master and slave have \
different values for the TIME_ZONE global variable. The values must \
be equal for the Statement-format replication to work";
        err_code= ER_SLAVE_FATAL_ERROR;
        sprintf(err_buff, ER(err_code), errmsg);
        goto err;
      }
    }
    else if (is_network_error(mysql_errno(mysql)))
    {
      mi->report(WARNING_LEVEL, mysql_errno(mysql),
                 "Get master TIME_ZONE failed with error: %s", mysql_error(mysql));
      goto network_err;
    } 
    else
    {
      /* Fatal error */
      errmsg= "The slave I/O thread stops because a fatal error is encountered \
when it try to get the value of TIME_ZONE global variable from master.";
      err_code= mysql_errno(mysql);
      sprintf(err_buff, "%s Error: %s", errmsg, mysql_error(mysql));
      goto err;
    }
    if (master_res)
    {
      mysql_free_result(master_res);
      master_res= NULL;
    }
  }

err:
  if (errmsg)
  {
    if (master_res)
      mysql_free_result(master_res);
    DBUG_ASSERT(err_code != 0);
    mi->report(ERROR_LEVEL, err_code, "%s", err_buff);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);

network_err:
  if (master_res)
    mysql_free_result(master_res);
  DBUG_RETURN(2);
}

/*
  Used by fetch_master_table (used by LOAD TABLE tblname FROM MASTER and LOAD
  DATA FROM MASTER). Drops the table (if 'overwrite' is true) and recreates it
  from the dump. Honours replication inclusion/exclusion rules.
  db must be non-zero (guarded by assertion).

  RETURN VALUES
    0           success
    1           error
*/

static int create_table_from_dump(THD* thd, MYSQL *mysql, const char* db,
                                  const char* table_name, bool overwrite)
{
  ulong packet_len;
  char *query, *save_db;
  uint32 save_db_length;
  Vio* save_vio;
  HA_CHECK_OPT check_opt;
  TABLE_LIST tables;
  int error= 1;
  handler *file;
  ulonglong save_options;
  NET *net= &mysql->net;
  const char *found_semicolon= NULL;
  DBUG_ENTER("create_table_from_dump");

  packet_len= my_net_read(net); // read create table statement
  if (packet_len == packet_error)
  {
    my_message(ER_MASTER_NET_READ, ER(ER_MASTER_NET_READ), MYF(0));
    DBUG_RETURN(1);
  }
  if (net->read_pos[0] == 255) // error from master
  {
    char *err_msg;
    err_msg= (char*) net->read_pos + ((mysql->server_capabilities &
                                       CLIENT_PROTOCOL_41) ?
                                      3+SQLSTATE_LENGTH+1 : 3);
    my_error(ER_MASTER, MYF(0), err_msg);
    DBUG_RETURN(1);
  }
  thd->command = COM_TABLE_DUMP;
  if (!(query = thd->strmake((char*) net->read_pos, packet_len)))
  {
    sql_print_error("create_table_from_dump: out of memory");
    my_message(ER_GET_ERRNO, "Out of memory", MYF(0));
    DBUG_RETURN(1);
  }
  thd->set_query(query, packet_len);
  thd->is_slave_error = 0;

  bzero((char*) &tables,sizeof(tables));
  tables.db = (char*)db;
  tables.alias= tables.table_name= (char*)table_name;

  /* Drop the table if 'overwrite' is true */
  if (overwrite)
  {
    if (mysql_rm_table(thd,&tables,1,0)) /* drop if exists */
    {
      sql_print_error("create_table_from_dump: failed to drop the table");
      goto err;
    }
    else
    {
      /* Clear the OK result of mysql_rm_table(). */
      thd->main_da.reset_diagnostics_area();
    }
  }

  /* Create the table. We do not want to log the "create table" statement */
  save_options = thd->options;
  thd->options &= ~ (OPTION_BIN_LOG);
  thd_proc_info(thd, "Creating table from master dump");
  // save old db in case we are creating in a different database
  save_db = thd->db;
  save_db_length= thd->db_length;
  thd->db = (char*)db;
  DBUG_ASSERT(thd->db != 0);
  thd->db_length= strlen(thd->db);
  mysql_parse(thd, thd->query, packet_len, &found_semicolon); // run create table
  thd->db = save_db;            // leave things the way the were before
  thd->db_length= save_db_length;
  thd->options = save_options;

  if (thd->is_slave_error)
    goto err;                   // mysql_parse took care of the error send

  thd_proc_info(thd, "Opening master dump table");
  thd->main_da.reset_diagnostics_area(); /* cleanup from CREATE_TABLE */
  /*
    Note: If this function starts to fail for MERGE tables,
    change the next two lines to these:
    tables.table= NULL; // was set by mysql_rm_table()
    if (!open_n_lock_single_table(thd, &tables, TL_WRITE))
  */
  tables.lock_type = TL_WRITE;
  if (!open_ltable(thd, &tables, TL_WRITE, 0))
  {
    sql_print_error("create_table_from_dump: could not open created table");
    goto err;
  }

  file = tables.table->file;
  thd_proc_info(thd, "Reading master dump table data");
  /* Copy the data file */
  if (file->net_read_dump(net))
  {
    my_message(ER_MASTER_NET_READ, ER(ER_MASTER_NET_READ), MYF(0));
    sql_print_error("create_table_from_dump: failed in\
 handler::net_read_dump()");
    goto err;
  }

  check_opt.init();
  check_opt.flags|= T_VERY_SILENT | T_CALC_CHECKSUM | T_QUICK;
  thd_proc_info(thd, "Rebuilding the index on master dump table");
  /*
    We do not want repair() to spam us with messages
    just send them to the error log, and report the failure in case of
    problems.
  */
  save_vio = thd->net.vio;
  thd->net.vio = 0;
  /* Rebuild the index file from the copied data file (with REPAIR) */
  error=file->ha_repair(thd,&check_opt) != 0;
  thd->net.vio = save_vio;
  if (error)
    my_error(ER_INDEX_REBUILD, MYF(0), tables.table->s->table_name.str);

err:
  close_thread_tables(thd);
  DBUG_RETURN(error);
}


int fetch_master_table(THD *thd, const char *db_name, const char *table_name,
                       Master_info *mi, MYSQL *mysql, bool overwrite)
{
  int error= 1;
  const char *errmsg=0;
  bool called_connected= (mysql != NULL);
  DBUG_ENTER("fetch_master_table");
  DBUG_PRINT("enter", ("db_name: '%s'  table_name: '%s'",
                       db_name,table_name));

  if (!called_connected)
  {
    if (!(mysql = mysql_init(NULL)))
    {
      DBUG_RETURN(1);
    }
    if (connect_to_master(thd, mysql, mi))
    {
      my_error(ER_CONNECT_TO_MASTER, MYF(0), mysql_error(mysql));
      /*
        We need to clear the active VIO since, theoretically, somebody
        might issue an awake() on this thread.  If we are then in the
        middle of closing and destroying the VIO inside the
        mysql_close(), we will have a problem.
       */
#ifdef SIGNAL_WITH_VIO_CLOSE
      thd->clear_active_vio();
#endif
      mysql_close(mysql);
      DBUG_RETURN(1);
    }
    if (thd->killed)
      goto err;
  }

  if (request_table_dump(mysql, db_name, table_name))
  {
    error= ER_UNKNOWN_ERROR;
    errmsg= "Failed on table dump request";
    goto err;
  }
  if (create_table_from_dump(thd, mysql, db_name,
                             table_name, overwrite))
    goto err;    // create_table_from_dump have sent the error already
  error = 0;

 err:
  if (!called_connected)
    mysql_close(mysql);
  if (errmsg && thd->vio_ok())
    my_message(error, errmsg, MYF(0));
  DBUG_RETURN(test(error));                     // Return 1 on error
}


static bool wait_for_relay_log_space(Relay_log_info* rli)
{
  bool slave_killed=0;
  Master_info* mi = rli->mi;
  const char *save_proc_info;
  THD* thd = mi->io_thd;
  DBUG_ENTER("wait_for_relay_log_space");

  pthread_mutex_lock(&rli->log_space_lock);
  save_proc_info= thd->enter_cond(&rli->log_space_cond,
                                  &rli->log_space_lock,
                                  "\
Waiting for the slave SQL thread to free enough relay log space");
  while (rli->log_space_limit < rli->log_space_total &&
         !(slave_killed=io_slave_killed(thd,mi)) &&
         !rli->ignore_log_space_limit)
    pthread_cond_wait(&rli->log_space_cond, &rli->log_space_lock);
  thd->exit_cond(save_proc_info);
  DBUG_RETURN(slave_killed);
}


/*
  Builds a Rotate from the ignored events' info and writes it to relay log.

  SYNOPSIS
  write_ignored_events_info_to_relay_log()
    thd             pointer to I/O thread's thd
    mi

  DESCRIPTION
    Slave I/O thread, going to die, must leave a durable trace of the
    ignored events' end position for the use of the slave SQL thread, by
    calling this function. Only that thread can call it (see assertion).
 */
static void write_ignored_events_info_to_relay_log(THD *thd, Master_info *mi)
{
  Relay_log_info *rli= &mi->rli;
  pthread_mutex_t *log_lock= rli->relay_log.get_log_lock();
  DBUG_ENTER("write_ignored_events_info_to_relay_log");

  DBUG_ASSERT(thd == mi->io_thd);
  pthread_mutex_lock(log_lock);
  if (rli->ign_master_log_name_end[0])
  {
    DBUG_PRINT("info",("writing a Rotate event to track down ignored events"));
    Rotate_log_event *ev= new Rotate_log_event(rli->ign_master_log_name_end,
                                               0, rli->ign_master_log_pos_end,
                                               Rotate_log_event::DUP_NAME);
    rli->ign_master_log_name_end[0]= 0;
    /* can unlock before writing as slave SQL thd will soon see our Rotate */
    pthread_mutex_unlock(log_lock);
    if (likely((bool)ev))
    {
      ev->server_id= 0; // don't be ignored by slave SQL thread
      if (unlikely(rli->relay_log.append(ev)))
        mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                   ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                   "failed to write a Rotate event"
                   " to the relay log, SHOW SLAVE STATUS may be"
                   " inaccurate");
      rli->relay_log.harvest_bytes_written(&rli->log_space_total);
      if (flush_master_info(mi, 1))
        sql_print_error("Failed to flush master info file");
      delete ev;
    }
    else
      mi->report(ERROR_LEVEL, ER_SLAVE_CREATE_EVENT_FAILURE,
                 ER(ER_SLAVE_CREATE_EVENT_FAILURE),
                 "Rotate_event (out of memory?),"
                 " SHOW SLAVE STATUS may be inaccurate");
  }
  else
    pthread_mutex_unlock(log_lock);
  DBUG_VOID_RETURN;
}


int register_slave_on_master(MYSQL* mysql, Master_info *mi,
                             bool *suppress_warnings)
{
  uchar buf[1024], *pos= buf;
  uint report_host_len, report_user_len=0, report_password_len=0;
  DBUG_ENTER("register_slave_on_master");

  *suppress_warnings= FALSE;
  if (!report_host)
    DBUG_RETURN(0);
  report_host_len= strlen(report_host);
  if (report_user)
    report_user_len= strlen(report_user);
  if (report_password)
    report_password_len= strlen(report_password);
  /* 30 is a good safety margin */
  if (report_host_len + report_user_len + report_password_len + 30 >
      sizeof(buf))
    DBUG_RETURN(0);                                     // safety

  int4store(pos, server_id); pos+= 4;
  pos= net_store_data(pos, (uchar*) report_host, report_host_len);
  pos= net_store_data(pos, (uchar*) report_user, report_user_len);
  pos= net_store_data(pos, (uchar*) report_password, report_password_len);
  int2store(pos, (uint16) report_port); pos+= 2;
  int4store(pos, rpl_recovery_rank);    pos+= 4;
  /* The master will fill in master_id */
  int4store(pos, 0);                    pos+= 4;

  if (simple_command(mysql, COM_REGISTER_SLAVE, buf, (size_t) (pos- buf), 0))
  {
    if (mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
    {
      *suppress_warnings= TRUE;                 // Suppress reconnect warning
    }
    else if (!check_io_slave_killed(mi->io_thd, mi, NULL))
    {
      char buf[256];
      my_snprintf(buf, sizeof(buf), "%s (Errno: %d)", mysql_error(mysql), 
                  mysql_errno(mysql));
      mi->report(ERROR_LEVEL, ER_SLAVE_MASTER_COM_FAILURE,
                 ER(ER_SLAVE_MASTER_COM_FAILURE), "COM_REGISTER_SLAVE", buf);
    }
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/**
  Execute a SHOW SLAVE STATUS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @param mi Pointer to Master_info object for the IO thread.

  @retval FALSE success
  @retval TRUE failure
*/
bool show_master_info(THD* thd, Master_info* mi)
{
  // TODO: fix this for multi-master
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_master_info");

  field_list.push_back(new Item_empty_string("Slave_IO_State",
                                                     14));
  field_list.push_back(new Item_empty_string("Master_Host",
                                                     sizeof(mi->host)));
  field_list.push_back(new Item_empty_string("Master_User",
                                                     sizeof(mi->user)));
  field_list.push_back(new Item_return_int("Master_Port", 7,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Connect_Retry", 10,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Master_Log_File",
                                             FN_REFLEN));
  field_list.push_back(new Item_return_int("Read_Master_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Relay_Log_File",
                                             FN_REFLEN));
  field_list.push_back(new Item_return_int("Relay_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Relay_Master_Log_File",
                                             FN_REFLEN));
  field_list.push_back(new Item_empty_string("Slave_IO_Running", 3));
  field_list.push_back(new Item_empty_string("Slave_SQL_Running", 3));
  field_list.push_back(new Item_empty_string("Replicate_Do_DB", 20));
  field_list.push_back(new Item_empty_string("Replicate_Ignore_DB", 20));
  field_list.push_back(new Item_empty_string("Replicate_Do_Table", 20));
  field_list.push_back(new Item_empty_string("Replicate_Ignore_Table", 23));
  field_list.push_back(new Item_empty_string("Replicate_Wild_Do_Table", 24));
  field_list.push_back(new Item_empty_string("Replicate_Wild_Ignore_Table",
                                             28));
  field_list.push_back(new Item_return_int("Last_Errno", 4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Last_Error", 20));
  field_list.push_back(new Item_return_int("Skip_Counter", 10,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Exec_Master_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_return_int("Relay_Log_Space", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Until_Condition", 6));
  field_list.push_back(new Item_empty_string("Until_Log_File", FN_REFLEN));
  field_list.push_back(new Item_return_int("Until_Log_Pos", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Master_SSL_Allowed", 7));
  field_list.push_back(new Item_empty_string("Master_SSL_CA_File",
                                             sizeof(mi->ssl_ca)));
  field_list.push_back(new Item_empty_string("Master_SSL_CA_Path",
                                             sizeof(mi->ssl_capath)));
  field_list.push_back(new Item_empty_string("Master_SSL_Cert",
                                             sizeof(mi->ssl_cert)));
  field_list.push_back(new Item_empty_string("Master_SSL_Cipher",
                                             sizeof(mi->ssl_cipher)));
  field_list.push_back(new Item_empty_string("Master_SSL_Key",
                                             sizeof(mi->ssl_key)));
  field_list.push_back(new Item_return_int("Seconds_Behind_Master", 10,
                                           MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Master_SSL_Verify_Server_Cert",
                                             3));
  field_list.push_back(new Item_return_int("Last_IO_Errno", 4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Last_IO_Error", 20));
  field_list.push_back(new Item_return_int("Last_SQL_Errno", 4, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Last_SQL_Error", 20));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  if (mi->host[0])
  {
    DBUG_PRINT("info",("host is set: '%s'", mi->host));
    String *packet= &thd->packet;
    protocol->prepare_for_resend();

    /*
      slave_running can be accessed without run_lock but not other
      non-volotile members like mi->io_thd, which is guarded by the mutex.
    */
    pthread_mutex_lock(&mi->run_lock);
    protocol->store(mi->io_thd ? mi->io_thd->proc_info : "", &my_charset_bin);
    pthread_mutex_unlock(&mi->run_lock);

    pthread_mutex_lock(&mi->data_lock);
    pthread_mutex_lock(&mi->rli.data_lock);
    pthread_mutex_lock(&mi->err_lock);
    pthread_mutex_lock(&mi->rli.err_lock);
    protocol->store(mi->host, &my_charset_bin);
    protocol->store(mi->user, &my_charset_bin);
    protocol->store((uint32) mi->port);
    protocol->store((uint32) mi->connect_retry);
    protocol->store(mi->master_log_name, &my_charset_bin);
    protocol->store((ulonglong) mi->master_log_pos);
    protocol->store(mi->rli.group_relay_log_name +
                    dirname_length(mi->rli.group_relay_log_name),
                    &my_charset_bin);
    protocol->store((ulonglong) mi->rli.group_relay_log_pos);
    protocol->store(mi->rli.group_master_log_name, &my_charset_bin);
    protocol->store(mi->slave_running == MYSQL_SLAVE_RUN_CONNECT ?
                    "Yes" : "No", &my_charset_bin);
    protocol->store(mi->rli.slave_running ? "Yes":"No", &my_charset_bin);
    protocol->store(rpl_filter->get_do_db());
    protocol->store(rpl_filter->get_ignore_db());

    char buf[256];
    String tmp(buf, sizeof(buf), &my_charset_bin);
    rpl_filter->get_do_table(&tmp);
    protocol->store(&tmp);
    rpl_filter->get_ignore_table(&tmp);
    protocol->store(&tmp);
    rpl_filter->get_wild_do_table(&tmp);
    protocol->store(&tmp);
    rpl_filter->get_wild_ignore_table(&tmp);
    protocol->store(&tmp);

    protocol->store(mi->rli.last_error().number);
    protocol->store(mi->rli.last_error().message, &my_charset_bin);
    protocol->store((uint32) mi->rli.slave_skip_counter);
    protocol->store((ulonglong) mi->rli.group_master_log_pos);
    protocol->store((ulonglong) mi->rli.log_space_total);

    protocol->store(
      mi->rli.until_condition==Relay_log_info::UNTIL_NONE ? "None":
        ( mi->rli.until_condition==Relay_log_info::UNTIL_MASTER_POS? "Master":
          "Relay"), &my_charset_bin);
    protocol->store(mi->rli.until_log_name, &my_charset_bin);
    protocol->store((ulonglong) mi->rli.until_log_pos);

#ifdef HAVE_OPENSSL
    protocol->store(mi->ssl? "Yes":"No", &my_charset_bin);
#else
    protocol->store(mi->ssl? "Ignored":"No", &my_charset_bin);
#endif
    protocol->store(mi->ssl_ca, &my_charset_bin);
    protocol->store(mi->ssl_capath, &my_charset_bin);
    protocol->store(mi->ssl_cert, &my_charset_bin);
    protocol->store(mi->ssl_cipher, &my_charset_bin);
    protocol->store(mi->ssl_key, &my_charset_bin);

    /*
      Seconds_Behind_Master: if SQL thread is running and I/O thread is
      connected, we can compute it otherwise show NULL (i.e. unknown).
    */
    if ((mi->slave_running == MYSQL_SLAVE_RUN_CONNECT) &&
        mi->rli.slave_running)
    {
      long time_diff= ((long)(time(0) - mi->rli.last_master_timestamp)
                       - mi->clock_diff_with_master);
      /*
        Apparently on some systems time_diff can be <0. Here are possible
        reasons related to MySQL:
        - the master is itself a slave of another master whose time is ahead.
        - somebody used an explicit SET TIMESTAMP on the master.
        Possible reason related to granularity-to-second of time functions
        (nothing to do with MySQL), which can explain a value of -1:
        assume the master's and slave's time are perfectly synchronized, and
        that at slave's connection time, when the master's timestamp is read,
        it is at the very end of second 1, and (a very short time later) when
        the slave's timestamp is read it is at the very beginning of second
        2. Then the recorded value for master is 1 and the recorded value for
        slave is 2. At SHOW SLAVE STATUS time, assume that the difference
        between timestamp of slave and rli->last_master_timestamp is 0
        (i.e. they are in the same second), then we get 0-(2-1)=-1 as a result.
        This confuses users, so we don't go below 0: hence the max().

        last_master_timestamp == 0 (an "impossible" timestamp 1970) is a
        special marker to say "consider we have caught up".
      */
      protocol->store((longlong)(mi->rli.last_master_timestamp ?
                                 max(0, time_diff) : 0));
    }
    else
    {
      protocol->store_null();
    }
    protocol->store(mi->ssl_verify_server_cert? "Yes":"No", &my_charset_bin);

    // Last_IO_Errno
    protocol->store(mi->last_error().number);
    // Last_IO_Error
    protocol->store(mi->last_error().message, &my_charset_bin);
    // Last_SQL_Errno
    protocol->store(mi->rli.last_error().number);
    // Last_SQL_Error
    protocol->store(mi->rli.last_error().message, &my_charset_bin);

    pthread_mutex_unlock(&mi->rli.err_lock);
    pthread_mutex_unlock(&mi->err_lock);
    pthread_mutex_unlock(&mi->rli.data_lock);
    pthread_mutex_unlock(&mi->data_lock);

    if (my_net_write(&thd->net, (uchar*) thd->packet.ptr(), packet->length()))
      DBUG_RETURN(TRUE);
  }
  my_eof(thd);
  DBUG_RETURN(FALSE);
}


void set_slave_thread_options(THD* thd)
{
  DBUG_ENTER("set_slave_thread_options");
  /*
     It's nonsense to constrain the slave threads with max_join_size; if a
     query succeeded on master, we HAVE to execute it. So set
     OPTION_BIG_SELECTS. Setting max_join_size to HA_POS_ERROR is not enough
     (and it's not needed if we have OPTION_BIG_SELECTS) because an INSERT
     SELECT examining more than 4 billion rows would still fail (yes, because
     when max_join_size is 4G, OPTION_BIG_SELECTS is automatically set, but
     only for client threads.
  */
  ulonglong options= thd->options | OPTION_BIG_SELECTS;
  if (opt_log_slave_updates)
    options|= OPTION_BIN_LOG;
  else
    options&= ~OPTION_BIN_LOG;
  thd->options= options;
  thd->variables.completion_type= 0;
  DBUG_VOID_RETURN;
}

void set_slave_thread_default_charset(THD* thd, Relay_log_info const *rli)
{
  DBUG_ENTER("set_slave_thread_default_charset");

  thd->variables.character_set_client=
    global_system_variables.character_set_client;
  thd->variables.collation_connection=
    global_system_variables.collation_connection;
  thd->variables.collation_server=
    global_system_variables.collation_server;
  thd->update_charset();

  /*
    We use a const cast here since the conceptual (and externally
    visible) behavior of the function is to set the default charset of
    the thread.  That the cache has to be invalidated is a secondary
    effect.
   */
  const_cast<Relay_log_info*>(rli)->cached_charset_invalidate();
  DBUG_VOID_RETURN;
}

/*
  init_slave_thread()
*/

static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type)
{
  DBUG_ENTER("init_slave_thread");
#if !defined(DBUG_OFF)
  int simulate_error= 0;
#endif
  thd->system_thread = (thd_type == SLAVE_THD_SQL) ?
    SYSTEM_THREAD_SLAVE_SQL : SYSTEM_THREAD_SLAVE_IO;
  thd->security_ctx->skip_grants();
  my_net_init(&thd->net, 0);
/*
  Adding MAX_LOG_EVENT_HEADER_LEN to the max_allowed_packet on all
  slave threads, since a replication event can become this much larger
  than the corresponding packet (query) sent from client to master.
*/
  thd->variables.max_allowed_packet= global_system_variables.max_allowed_packet
    + MAX_LOG_EVENT_HEADER;  /* note, incr over the global not session var */
  thd->slave_thread = 1;
  thd->enable_slow_log= opt_log_slow_slave_statements;
  set_slave_thread_options(thd);
  thd->client_capabilities = CLIENT_LOCAL_FILES;
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  DBUG_EXECUTE_IF("simulate_io_slave_error_on_init",
                  simulate_error|= (1 << SLAVE_THD_IO););
  DBUG_EXECUTE_IF("simulate_sql_slave_error_on_init",
                  simulate_error|= (1 << SLAVE_THD_SQL););
#if !defined(DBUG_OFF)
  if (init_thr_lock() || thd->store_globals() || simulate_error & (1<< thd_type))
#else
  if (init_thr_lock() || thd->store_globals())
#endif
  {
    thd->cleanup();
    DBUG_RETURN(-1);
  }
  lex_start(thd);

  if (thd_type == SLAVE_THD_SQL)
    thd_proc_info(thd, "Waiting for the next event in relay log");
  else
    thd_proc_info(thd, "Waiting for master update");
  thd->version=refresh_version;
  thd->set_time();
  DBUG_RETURN(0);
}


static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
                      void* thread_killed_arg)
{
  int nap_time;
  thr_alarm_t alarmed;
  DBUG_ENTER("safe_sleep");

  thr_alarm_init(&alarmed);
  time_t start_time= my_time(0);
  time_t end_time= start_time+sec;

  while ((nap_time= (int) (end_time - start_time)) > 0)
  {
    ALARM alarm_buff;
    /*
      The only reason we are asking for alarm is so that
      we will be woken up in case of murder, so if we do not get killed,
      set the alarm so it goes off after we wake up naturally
    */
    thr_alarm(&alarmed, 2 * nap_time, &alarm_buff);
    sleep(nap_time);
    thr_end_alarm(&alarmed);

    if ((*thread_killed)(thd,thread_killed_arg))
      DBUG_RETURN(1);
    start_time= my_time(0);
  }
  DBUG_RETURN(0);
}


static int request_dump(MYSQL* mysql, Master_info* mi,
                        bool *suppress_warnings)
{
  uchar buf[FN_REFLEN + 10];
  int len;
  int binlog_flags = 0; // for now
  char* logname = mi->master_log_name;
  DBUG_ENTER("request_dump");
  
  *suppress_warnings= FALSE;

  // TODO if big log files: Change next to int8store()
  int4store(buf, (ulong) mi->master_log_pos);
  int2store(buf + 4, binlog_flags);
  int4store(buf + 6, server_id);
  len = (uint) strlen(logname);
  memcpy(buf + 10, logname,len);
  if (simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
  {
    /*
      Something went wrong, so we will just reconnect and retry later
      in the future, we should do a better error analysis, but for
      now we just fill up the error log :-)
    */
    if (mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
      *suppress_warnings= TRUE;                 // Suppress reconnect warning
    else
      sql_print_error("Error on COM_BINLOG_DUMP: %d  %s, will retry in %d secs",
                      mysql_errno(mysql), mysql_error(mysql),
                      master_connect_retry);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


static int request_table_dump(MYSQL* mysql, const char* db, const char* table)
{
  uchar buf[1024], *p = buf;
  DBUG_ENTER("request_table_dump");

  uint table_len = (uint) strlen(table);
  uint db_len = (uint) strlen(db);
  if (table_len + db_len > sizeof(buf) - 2)
  {
    sql_print_error("request_table_dump: Buffer overrun");
    DBUG_RETURN(1);
  }

  *p++ = db_len;
  memcpy(p, db, db_len);
  p += db_len;
  *p++ = table_len;
  memcpy(p, table, table_len);

  if (simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
  {
    sql_print_error("request_table_dump: Error sending the table dump \
command");
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


/*
  Read one event from the master

  SYNOPSIS
    read_event()
    mysql               MySQL connection
    mi                  Master connection information
    suppress_warnings   TRUE when a normal net read timeout has caused us to
                        try a reconnect.  We do not want to print anything to
                        the error log in this case because this a anormal
                        event in an idle server.

    RETURN VALUES
    'packet_error'      Error
    number              Length of packet
*/

static ulong read_event(MYSQL* mysql, Master_info *mi, bool* suppress_warnings)
{
  ulong len;
  DBUG_ENTER("read_event");

  *suppress_warnings= FALSE;
  /*
    my_real_read() will time us out
    We check if we were told to die, and if not, try reading again
  */
#ifndef DBUG_OFF
  if (disconnect_slave_event_count && !(mi->events_till_disconnect--))
    DBUG_RETURN(packet_error);
#endif

  len = cli_safe_read(mysql);
  if (len == packet_error || (long) len < 1)
  {
    if (mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
    {
      /*
        We are trying a normal reconnect after a read timeout;
        we suppress prints to .err file as long as the reconnect
        happens without problems
      */
      *suppress_warnings= TRUE;
    }
    else
      sql_print_error("Error reading packet from server: %s ( server_errno=%d)",
                      mysql_error(mysql), mysql_errno(mysql));
    DBUG_RETURN(packet_error);
  }

  /* Check if eof packet */
  if (len < 8 && mysql->net.read_pos[0] == 254)
  {
    sql_print_information("Slave: received end packet from server, apparent "
                          "master shutdown: %s",
                     mysql_error(mysql));
     DBUG_RETURN(packet_error);
  }

  DBUG_PRINT("exit", ("len: %lu  net->read_pos[4]: %d",
                      len, mysql->net.read_pos[4]));
  DBUG_RETURN(len - 1);
}

/*
  Check if the current error is of temporary nature of not.
  Some errors are temporary in nature, such as
  ER_LOCK_DEADLOCK and ER_LOCK_WAIT_TIMEOUT.  Ndb also signals
  that the error is temporary by pushing a warning with the error code
  ER_GET_TEMPORARY_ERRMSG, if the originating error is temporary.
*/
static int has_temporary_error(THD *thd)
{
  DBUG_ENTER("has_temporary_error");

  DBUG_EXECUTE_IF("all_errors_are_temporary_errors",
                  if (thd->main_da.is_error())
                  {
                    thd->clear_error();
                    my_error(ER_LOCK_DEADLOCK, MYF(0));
                  });

  /*
    If there is no message in THD, we can't say if it's a temporary
    error or not. This is currently the case for Incident_log_event,
    which sets no message. Return FALSE.
  */
  if (!thd->is_error())
    DBUG_RETURN(0);

  /*
    Temporary error codes:
    currently, InnoDB deadlock detected by InnoDB or lock
    wait timeout (innodb_lock_wait_timeout exceeded
  */
  if (thd->main_da.sql_errno() == ER_LOCK_DEADLOCK ||
      thd->main_da.sql_errno() == ER_LOCK_WAIT_TIMEOUT)
    DBUG_RETURN(1);

#ifdef HAVE_NDB_BINLOG
  /*
    currently temporary error set in ndbcluster
  */
  List_iterator_fast<MYSQL_ERROR> it(thd->warn_list);
  MYSQL_ERROR *err;
  while ((err= it++))
  {
    DBUG_PRINT("info", ("has warning %d %s", err->code, err->msg));
    switch (err->code)
    {
    case ER_GET_TEMPORARY_ERRMSG:
      DBUG_RETURN(1);
    default:
      break;
    }
  }
#endif
  DBUG_RETURN(0);
}


/**
  Applies the given event and advances the relay log position.

  In essence, this function does:

  @code
    ev->apply_event(rli);
    ev->update_pos(rli);
  @endcode

  But it also does some maintainance, such as skipping events if
  needed and reporting errors.

  If the @c skip flag is set, then it is tested whether the event
  should be skipped, by looking at the slave_skip_counter and the
  server id.  The skip flag should be set when calling this from a
  replication thread but not set when executing an explicit BINLOG
  statement.

  @retval 0 OK.

  @retval 1 Error calling ev->apply_event().

  @retval 2 No error calling ev->apply_event(), but error calling
  ev->update_pos().
*/
int apply_event_and_update_pos(Log_event* ev, THD* thd, Relay_log_info* rli)
{
  int exec_res= 0;

  DBUG_ENTER("apply_event_and_update_pos");

  DBUG_PRINT("exec_event",("%s(type_code: %d; server_id: %d)",
                           ev->get_type_str(), ev->get_type_code(),
                           ev->server_id));
  DBUG_PRINT("info", ("thd->options: %s%s; rli->last_event_start_time: %lu",
                      FLAGSTR(thd->options, OPTION_NOT_AUTOCOMMIT),
                      FLAGSTR(thd->options, OPTION_BEGIN),
                      rli->last_event_start_time));

  /*
    Execute the event to change the database and update the binary
    log coordinates, but first we set some data that is needed for
    the thread.

    The event will be executed unless it is supposed to be skipped.

    Queries originating from this server must be skipped.  Low-level
    events (Format_description_log_event, Rotate_log_event,
    Stop_log_event) from this server must also be skipped. But for
    those we don't want to modify 'group_master_log_pos', because
    these events did not exist on the master.
    Format_description_log_event is not completely skipped.

    Skip queries specified by the user in 'slave_skip_counter'.  We
    can't however skip events that has something to do with the log
    files themselves.

    Filtering on own server id is extremely important, to ignore
    execution of events created by the creation/rotation of the relay
    log (remember that now the relay log starts with its Format_desc,
    has a Rotate etc).
  */

  thd->server_id = ev->server_id; // use the original server id for logging
  thd->set_time();                            // time the query
  thd->lex->current_select= 0;
  if (!ev->when)
    ev->when= my_time(0);
  ev->thd = thd; // because up to this point, ev->thd == 0

  int reason= ev->shall_skip(rli);
  if (reason == Log_event::EVENT_SKIP_COUNT)
    --rli->slave_skip_counter;
  pthread_mutex_unlock(&rli->data_lock);
  if (reason == Log_event::EVENT_SKIP_NOT)
    exec_res= ev->apply_event(rli);

#ifndef DBUG_OFF
  /*
    This only prints information to the debug trace.

    TODO: Print an informational message to the error log?
  */
  static const char *const explain[] = {
    // EVENT_SKIP_NOT,
    "not skipped",
    // EVENT_SKIP_IGNORE,
    "skipped because event should be ignored",
    // EVENT_SKIP_COUNT
    "skipped because event skip counter was non-zero"
  };
  DBUG_PRINT("info", ("OPTION_BEGIN: %d; IN_STMT: %d",
                      thd->options & OPTION_BEGIN ? 1 : 0,
                      rli->get_flag(Relay_log_info::IN_STMT)));
  DBUG_PRINT("skip_event", ("%s event was %s",
                            ev->get_type_str(), explain[reason]));
#endif

  DBUG_PRINT("info", ("apply_event error = %d", exec_res));
  if (exec_res == 0)
  {
    int error= ev->update_pos(rli);
#ifdef HAVE_purify
    if (!rli->is_fake)
#endif
    {
#ifndef DBUG_OFF
      char buf[22];
#endif
      DBUG_PRINT("info", ("update_pos error = %d", error));
      DBUG_PRINT("info", ("group %s %s",
                          llstr(rli->group_relay_log_pos, buf),
                          rli->group_relay_log_name));
      DBUG_PRINT("info", ("event %s %s",
                          llstr(rli->event_relay_log_pos, buf),
                          rli->event_relay_log_name));
    }
    /*
      The update should not fail, so print an error message and
      return an error code.

      TODO: Replace this with a decent error message when merged
      with BUG#24954 (which adds several new error message).
    */
    if (error)
    {
      char buf[22];
      rli->report(ERROR_LEVEL, ER_UNKNOWN_ERROR,
                  "It was not possible to update the positions"
                  " of the relay log information: the slave may"
                  " be in an inconsistent state."
                  " Stopped in %s position %s",
                  rli->group_relay_log_name,
                  llstr(rli->group_relay_log_pos, buf));
      DBUG_RETURN(2);
    }
  }

  DBUG_RETURN(exec_res ? 1 : 0);
}


/**
  Top-level function for executing the next event from the relay log.

  This function reads the event from the relay log, executes it, and
  advances the relay log position.  It also handles errors, etc.

  This function may fail to apply the event for the following reasons:

   - The position specfied by the UNTIL condition of the START SLAVE
     command is reached.

   - It was not possible to read the event from the log.

   - The slave is killed.

   - An error occurred when applying the event, and the event has been
     tried slave_trans_retries times.  If the event has been retried
     fewer times, 0 is returned.

   - init_master_info or init_relay_log_pos failed. (These are called
     if a failure occurs when applying the event.)

   - An error occurred when updating the binlog position.

  @retval 0 The event was applied.

  @retval 1 The event was not applied.
*/
static int exec_relay_log_event(THD* thd, Relay_log_info* rli)
{
  DBUG_ENTER("exec_relay_log_event");

  /*
     We acquire this mutex since we need it for all operations except
     event execution. But we will release it in places where we will
     wait for something for example inside of next_event().
   */
  pthread_mutex_lock(&rli->data_lock);

  Log_event * ev = next_event(rli);

  DBUG_ASSERT(rli->sql_thd==thd);

  if (sql_slave_killed(thd,rli))
  {
    pthread_mutex_unlock(&rli->data_lock);
    delete ev;
    DBUG_RETURN(1);
  }
  if (ev)
  {
    int exec_res;

    /*
      This tests if the position of the beginning of the current event
      hits the UNTIL barrier.
    */
    if (rli->until_condition != Relay_log_info::UNTIL_NONE &&
        rli->is_until_satisfied((rli->is_in_group() || !ev->log_pos) ?
                                rli->group_master_log_pos :
                                ev->log_pos - ev->data_written))
    {
      char buf[22];
      sql_print_information("Slave SQL thread stopped because it reached its"
                            " UNTIL position %s", llstr(rli->until_pos(), buf));
      /*
        Setting abort_slave flag because we do not want additional message about
        error in query execution to be printed.
      */
      rli->abort_slave= 1;
      pthread_mutex_unlock(&rli->data_lock);
      delete ev;
      DBUG_RETURN(1);
    }
    exec_res= apply_event_and_update_pos(ev, thd, rli);

    /*
      Format_description_log_event should not be deleted because it will be
      used to read info about the relay log's format; it will be deleted when
      the SQL thread does not need it, i.e. when this thread terminates.
    */
    if (ev->get_type_code() != FORMAT_DESCRIPTION_EVENT)
    {
      DBUG_PRINT("info", ("Deleting the event after it has been executed"));
      delete ev;
    }

    /*
      update_log_pos failed: this should not happen, so we don't
      retry.
    */
    if (exec_res == 2)
      DBUG_RETURN(1);

    if (slave_trans_retries)
    {
      int temp_err;
      if (exec_res && (temp_err= has_temporary_error(thd)))
      {
        const char *errmsg;
        /*
          We were in a transaction which has been rolled back because of a
          temporary error;
          let's seek back to BEGIN log event and retry it all again.
	  Note, if lock wait timeout (innodb_lock_wait_timeout exceeded)
	  there is no rollback since 5.0.13 (ref: manual).
          We have to not only seek but also
          a) init_master_info(), to seek back to hot relay log's start for later
          (for when we will come back to this hot log after re-processing the
          possibly existing old logs where BEGIN is: check_binlog_magic() will
          then need the cache to be at position 0 (see comments at beginning of
          init_master_info()).
          b) init_relay_log_pos(), because the BEGIN may be an older relay log.
        */
        if (rli->trans_retries < slave_trans_retries)
        {
          if (init_master_info(rli->mi, 0, 0, 0, SLAVE_SQL))
            sql_print_error("Failed to initialize the master info structure");
          else if (init_relay_log_pos(rli,
                                      rli->group_relay_log_name,
                                      rli->group_relay_log_pos,
                                      1, &errmsg, 1))
            sql_print_error("Error initializing relay log position: %s",
                            errmsg);
          else
          {
            exec_res= 0;
            end_trans(thd, ROLLBACK);
            /* chance for concurrent connection to get more locks */
            safe_sleep(thd, min(rli->trans_retries, MAX_SLAVE_RETRY_PAUSE),
                       (CHECK_KILLED_FUNC)sql_slave_killed, (void*)rli);
            pthread_mutex_lock(&rli->data_lock); // because of SHOW STATUS
            rli->trans_retries++;
            rli->retried_trans++;
            pthread_mutex_unlock(&rli->data_lock);
            DBUG_PRINT("info", ("Slave retries transaction "
                                "rli->trans_retries: %lu", rli->trans_retries));
          }
        }
        else
          sql_print_error("Slave SQL thread retried transaction %lu time(s) "
                          "in vain, giving up. Consider raising the value of "
                          "the slave_transaction_retries variable.",
                          slave_trans_retries);
      }
      else if ((exec_res && !temp_err) ||
               (opt_using_transactions &&
                rli->group_relay_log_pos == rli->event_relay_log_pos))
      {
        /*
          Only reset the retry counter if the entire group succeeded
          or failed with a non-transient error.  On a successful
          event, the execution will proceed as usual; in the case of a
          non-transient error, the slave will stop with an error.
         */
        rli->trans_retries= 0; // restart from fresh
        DBUG_PRINT("info", ("Resetting retry counter, rli->trans_retries: %lu",
                            rli->trans_retries));
      }
    }
    DBUG_RETURN(exec_res);
  }
  pthread_mutex_unlock(&rli->data_lock);
  rli->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_READ_FAILURE,
              ER(ER_SLAVE_RELAY_LOG_READ_FAILURE), "\
Could not parse relay log event entry. The possible reasons are: the master's \
binary log is corrupted (you can check this by running 'mysqlbinlog' on the \
binary log), the slave's relay log is corrupted (you can check this by running \
'mysqlbinlog' on the relay log), a network problem, or a bug in the master's \
or slave's MySQL code. If you want to check the master's binary log or slave's \
relay log, you will be able to know their names by issuing 'SHOW SLAVE STATUS' \
on this slave.\
");
  DBUG_RETURN(1);
}


static bool check_io_slave_killed(THD *thd, Master_info *mi, const char *info)
{
  if (io_slave_killed(thd, mi))
  {
    if (info && global_system_variables.log_warnings)
      sql_print_information("%s", info);
    return TRUE;
  }
  return FALSE;
}


/**
  @brief Try to reconnect slave IO thread.

  @details Terminates current connection to master, sleeps for
  @c mi->connect_retry msecs and initiates new connection with
  @c safe_reconnect(). Variable pointed by @c retry_count is increased -
  if it exceeds @c master_retry_count then connection is not re-established
  and function signals error.
  Unless @c suppres_warnings is TRUE, a warning is put in the server error log
  when reconnecting. The warning message and messages used to report errors
  are taken from @c messages array. In case @c master_retry_count is exceeded,
  no messages are added to the log.

  @param[in]     thd                 Thread context.
  @param[in]     mysql               MySQL connection.
  @param[in]     mi                  Master connection information.
  @param[in,out] retry_count         Number of attempts to reconnect.
  @param[in]     suppress_warnings   TRUE when a normal net read timeout 
                                     has caused to reconnecting.
  @param[in]     messages            Messages to print/log, see 
                                     reconnect_messages[] array.

  @retval        0                   OK.
  @retval        1                   There was an error.
*/

static int try_to_reconnect(THD *thd, MYSQL *mysql, Master_info *mi,
                            uint *retry_count, bool suppress_warnings,
                            const char *messages[SLAVE_RECON_MSG_MAX])
{
  mi->slave_running= MYSQL_SLAVE_RUN_NOT_CONNECT;
  thd->proc_info= messages[SLAVE_RECON_MSG_WAIT];
#ifdef SIGNAL_WITH_VIO_CLOSE  
  thd->clear_active_vio();
#endif
  end_server(mysql);
  if ((*retry_count)++)
  {
    if (*retry_count > master_retry_count)
      return 1;                             // Don't retry forever
    safe_sleep(thd, mi->connect_retry, (CHECK_KILLED_FUNC) io_slave_killed,
               (void *) mi);
  }
  if (check_io_slave_killed(thd, mi, messages[SLAVE_RECON_MSG_KILLED_WAITING]))
    return 1;
  thd->proc_info = messages[SLAVE_RECON_MSG_AFTER];
  if (!suppress_warnings) 
  {
    char buf[256], llbuff[22];
    my_snprintf(buf, sizeof(buf), messages[SLAVE_RECON_MSG_FAILED], 
                IO_RPL_LOG_NAME, llstr(mi->master_log_pos, llbuff));
    /* 
      Raise a warining during registering on master/requesting dump.
      Log a message reading event.
    */
    if (messages[SLAVE_RECON_MSG_COMMAND][0])
    {
      mi->report(WARNING_LEVEL, ER_SLAVE_MASTER_COM_FAILURE,
                 ER(ER_SLAVE_MASTER_COM_FAILURE), 
                 messages[SLAVE_RECON_MSG_COMMAND], buf);
    }
    else
    {
      sql_print_information("%s", buf);
    }
  }
  if (safe_reconnect(thd, mysql, mi, 1) || io_slave_killed(thd, mi))
  {
    if (global_system_variables.log_warnings)
      sql_print_information("%s", messages[SLAVE_RECON_MSG_KILLED_AFTER]);
    return 1;
  }
  return 0;
}


/**
  Slave IO thread entry point.

  @param arg Pointer to Master_info struct that holds information for
  the IO thread.

  @return Always 0.
*/
pthread_handler_t handle_slave_io(void *arg)
{
  THD *thd; // needs to be first for thread_stack
  MYSQL *mysql;
  Master_info *mi = (Master_info*)arg;
  Relay_log_info *rli= &mi->rli;
  char llbuff[22];
  uint retry_count;
  bool suppress_warnings;
  int ret;
#ifndef DBUG_OFF
  uint retry_count_reg= 0, retry_count_dump= 0, retry_count_event= 0;
#endif
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_io");

  DBUG_ASSERT(mi->inited);
  mysql= NULL ;
  retry_count= 0;

  pthread_mutex_lock(&mi->run_lock);
  /* Inform waiting threads that slave has started */
  mi->slave_run_id++;

#ifndef DBUG_OFF
  mi->events_till_disconnect = disconnect_slave_event_count;
#endif

  thd= new THD; // note that contructor of THD uses DBUG_ !
  THD_CHECK_SENTRY(thd);
  mi->io_thd = thd;

  pthread_detach_this_thread();
  thd->thread_stack= (char*) &thd; // remember where our stack is
  mi->clear_error();
  if (init_slave_thread(thd, SLAVE_THD_IO))
  {
    pthread_cond_broadcast(&mi->start_cond);
    pthread_mutex_unlock(&mi->run_lock);
    sql_print_error("Failed during slave I/O thread initialization");
    goto err;
  }
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  mi->slave_running = 1;
  mi->abort_slave = 0;
  pthread_mutex_unlock(&mi->run_lock);
  pthread_cond_broadcast(&mi->start_cond);

  DBUG_PRINT("master_info",("log_file_name: '%s'  position: %s",
                            mi->master_log_name,
                            llstr(mi->master_log_pos,llbuff)));

  if (!(mi->mysql = mysql = mysql_init(NULL)))
  {
    mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
               ER(ER_SLAVE_FATAL_ERROR), "error in mysql_init()");
    goto err;
  }

  thd_proc_info(thd, "Connecting to master");
  // we can get killed during safe_connect
  if (!safe_connect(thd, mysql, mi))
  {
    sql_print_information("Slave I/O thread: connected to master '%s@%s:%d',"
                          "replication started in log '%s' at position %s",
                          mi->user, mi->host, mi->port,
			  IO_RPL_LOG_NAME,
			  llstr(mi->master_log_pos,llbuff));
  /*
    Adding MAX_LOG_EVENT_HEADER_LEN to the max_packet_size on the I/O
    thread, since a replication event can become this much larger than
    the corresponding packet (query) sent from client to master.
  */
    mysql->net.max_packet_size= thd->net.max_packet_size+= MAX_LOG_EVENT_HEADER;
  }
  else
  {
    sql_print_information("Slave I/O thread killed while connecting to master");
    goto err;
  }

connected:

  // TODO: the assignment below should be under mutex (5.0)
  mi->slave_running= MYSQL_SLAVE_RUN_CONNECT;
  thd->slave_net = &mysql->net;
  thd_proc_info(thd, "Checking master version");
  ret= get_master_version_and_clock(mysql, mi);
  if (ret == 1)
    /* Fatal error */
    goto err;
  
  if (ret == 2) 
  { 
    if (check_io_slave_killed(mi->io_thd, mi, "Slave I/O thread killed"
                              "while calling get_master_version_and_clock(...)"))
      goto err;
    suppress_warnings= FALSE;
    /* Try to reconnect because the error was caused by a transient network problem */
    if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_REG]))
      goto err;
    goto connected;
  } 

  if (mi->rli.relay_log.description_event_for_queue->binlog_version > 1)
  {
    /*
      Register ourselves with the master.
    */
    thd_proc_info(thd, "Registering slave on master");
    if (register_slave_on_master(mysql, mi, &suppress_warnings))
    {
      if (!check_io_slave_killed(thd, mi, "Slave I/O thread killed "
                                "while registering slave on master"))
      {
        sql_print_error("Slave I/O thread couldn't register on master");
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_REG]))
          goto err;
      }
      else
        goto err;
      goto connected;
    }
    DBUG_EXECUTE_IF("FORCE_SLAVE_TO_RECONNECT_REG", 
      if (!retry_count_reg)
      {
        retry_count_reg++;
        sql_print_information("Forcing to reconnect slave I/O thread");
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_REG]))
          goto err;
        goto connected;
      });
  }

  DBUG_PRINT("info",("Starting reading binary log from master"));
  while (!io_slave_killed(thd,mi))
  {
    thd_proc_info(thd, "Requesting binlog dump");
    if (request_dump(mysql, mi, &suppress_warnings))
    {
      sql_print_error("Failed on request_dump()");
      if (check_io_slave_killed(thd, mi, "Slave I/O thread killed while \
requesting master dump") ||
          try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                           reconnect_messages[SLAVE_RECON_ACT_DUMP]))
        goto err;
      goto connected;
    }
    DBUG_EXECUTE_IF("FORCE_SLAVE_TO_RECONNECT_DUMP", 
      if (!retry_count_dump)
      {
        retry_count_dump++;
        sql_print_information("Forcing to reconnect slave I/O thread");
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_DUMP]))
          goto err;
        goto connected;
      });

    DBUG_ASSERT(mi->last_error().number == 0);
    while (!io_slave_killed(thd,mi))
    {
      ulong event_len;
      /*
         We say "waiting" because read_event() will wait if there's nothing to
         read. But if there's something to read, it will not wait. The
         important thing is to not confuse users by saying "reading" whereas
         we're in fact receiving nothing.
      */
      thd_proc_info(thd, "Waiting for master to send event");
      event_len= read_event(mysql, mi, &suppress_warnings);
      if (check_io_slave_killed(thd, mi, "Slave I/O thread killed while \
reading event"))
        goto err;
      DBUG_EXECUTE_IF("FORCE_SLAVE_TO_RECONNECT_EVENT",
        if (!retry_count_event)
        {
          retry_count_event++;
          sql_print_information("Forcing to reconnect slave I/O thread");
          if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                               reconnect_messages[SLAVE_RECON_ACT_EVENT]))
            goto err;
          goto connected;
        });

      if (event_len == packet_error)
      {
        uint mysql_error_number= mysql_errno(mysql);
        switch (mysql_error_number) {
        case CR_NET_PACKET_TOO_LARGE:
          sql_print_error("\
Log entry on master is longer than max_allowed_packet (%ld) on \
slave. If the entry is correct, restart the server with a higher value of \
max_allowed_packet",
                          thd->variables.max_allowed_packet);
          mi->report(ERROR_LEVEL, ER_NET_PACKET_TOO_LARGE,
                     "%s", ER(ER_NET_PACKET_TOO_LARGE));
          goto err;
        case ER_MASTER_FATAL_ERROR_READING_BINLOG:
          mi->report(ERROR_LEVEL, ER_MASTER_FATAL_ERROR_READING_BINLOG,
                     ER(ER_MASTER_FATAL_ERROR_READING_BINLOG),
                     mysql_error_number, mysql_error(mysql));
          goto err;
        case ER_OUT_OF_RESOURCES:
          sql_print_error("\
Stopping slave I/O thread due to out-of-memory error from master");
          mi->report(ERROR_LEVEL, ER_OUT_OF_RESOURCES,
                     "%s", ER(ER_OUT_OF_RESOURCES));
          goto err;
        }
        if (try_to_reconnect(thd, mysql, mi, &retry_count, suppress_warnings,
                             reconnect_messages[SLAVE_RECON_ACT_EVENT]))
          goto err;
        goto connected;
      } // if (event_len == packet_error)

      retry_count=0;                    // ok event, reset retry counter
      thd_proc_info(thd, "Queueing master event to the relay log");
      if (queue_event(mi,(const char*)mysql->net.read_pos + 1,
                      event_len))
      {
        mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                   ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                   "could not queue event from master");
        goto err;
      }
      if (flush_master_info(mi, 1))
      {
        sql_print_error("Failed to flush master info file");
        goto err;
      }
      /*
        See if the relay logs take too much space.
        We don't lock mi->rli.log_space_lock here; this dirty read saves time
        and does not introduce any problem:
        - if mi->rli.ignore_log_space_limit is 1 but becomes 0 just after (so
        the clean value is 0), then we are reading only one more event as we
        should, and we'll block only at the next event. No big deal.
        - if mi->rli.ignore_log_space_limit is 0 but becomes 1 just after (so
        the clean value is 1), then we are going into wait_for_relay_log_space()
        for no reason, but this function will do a clean read, notice the clean
        value and exit immediately.
      */
#ifndef DBUG_OFF
      {
        char llbuf1[22], llbuf2[22];
        DBUG_PRINT("info", ("log_space_limit=%s log_space_total=%s \
ignore_log_space_limit=%d",
                            llstr(rli->log_space_limit,llbuf1),
                            llstr(rli->log_space_total,llbuf2),
                            (int) rli->ignore_log_space_limit));
      }
#endif

      if (rli->log_space_limit && rli->log_space_limit <
          rli->log_space_total &&
          !rli->ignore_log_space_limit)
        if (wait_for_relay_log_space(rli))
        {
          sql_print_error("Slave I/O thread aborted while waiting for relay \
log space");
          goto err;
        }
    }
  }

  // error = 0;
err:
  // print the current replication position
  sql_print_information("Slave I/O thread exiting, read up to log '%s', position %s",
                  IO_RPL_LOG_NAME, llstr(mi->master_log_pos,llbuff));
  thd->set_query(NULL, 0);
  thd->reset_db(NULL, 0);
  if (mysql)
  {
    /*
      Here we need to clear the active VIO before closing the
      connection with the master.  The reason is that THD::awake()
      might be called from terminate_slave_thread() because somebody
      issued a STOP SLAVE.  If that happends, the close_active_vio()
      can be called in the middle of closing the VIO associated with
      the 'mysql' object, causing a crash.
    */
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->clear_active_vio();
#endif
    mysql_close(mysql);
    mi->mysql=0;
  }
  write_ignored_events_info_to_relay_log(thd, mi);
  thd_proc_info(thd, "Waiting for slave mutex on exit");
  pthread_mutex_lock(&mi->run_lock);

  /* Forget the relay log's format */
  delete mi->rli.relay_log.description_event_for_queue;
  mi->rli.relay_log.description_event_for_queue= 0;
  // TODO: make rpl_status part of Master_info
  change_rpl_status(RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE);
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because net.vio is 0
  close_thread_tables(thd);
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
  mi->abort_slave= 0;
  mi->slave_running= 0;
  mi->io_thd= 0;
  /*
    Note: the order of the two following calls (first broadcast, then unlock)
    is important. Otherwise a killer_thread can execute between the calls and
    delete the mi structure leading to a crash! (see BUG#25306 for details)
   */ 
  pthread_cond_broadcast(&mi->stop_cond);       // tell the world we are done
  DBUG_EXECUTE_IF("simulate_slave_delay_at_terminate_bug38694", sleep(5););
  pthread_mutex_unlock(&mi->run_lock);

  DBUG_LEAVE;                                   // Must match DBUG_ENTER()
  my_thread_end();
  pthread_exit(0);
  return 0;                                     // Avoid compiler warnings
}

/*
  Check the temporary directory used by commands like
  LOAD DATA INFILE.
 */
static 
int check_temp_dir(char* tmp_file)
{
  int fd;
  MY_DIR *dirp;
  char tmp_dir[FN_REFLEN];
  size_t tmp_dir_size;

  DBUG_ENTER("check_temp_dir");

  /*
    Get the directory from the temporary file.
  */
  dirname_part(tmp_dir, tmp_file, &tmp_dir_size);

  /*
    Check if the directory exists.
   */
  if (!(dirp=my_dir(tmp_dir,MYF(MY_WME))))
    DBUG_RETURN(1);
  my_dirend(dirp);

  /*
    Check permissions to create a file.
   */
  if ((fd= my_create(tmp_file, CREATE_MODE,
                     O_WRONLY | O_BINARY | O_EXCL | O_NOFOLLOW,
                     MYF(MY_WME))) < 0)
  DBUG_RETURN(1);

  /*
    Clean up.
   */
  my_close(fd, MYF(0));
  my_delete(tmp_file, MYF(0));

  DBUG_RETURN(0);
}

/**
  Slave SQL thread entry point.

  @param arg Pointer to Relay_log_info object that holds information
  for the SQL thread.

  @return Always 0.
*/
pthread_handler_t handle_slave_sql(void *arg)
{
  THD *thd;                     /* needs to be first for thread_stack */
  char llbuff[22],llbuff1[22];

  Relay_log_info* rli = &((Master_info*)arg)->rli;
  const char *errmsg;

  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_sql");

  DBUG_ASSERT(rli->inited);
  pthread_mutex_lock(&rli->run_lock);
  DBUG_ASSERT(!rli->slave_running);
  errmsg= 0;
#ifndef DBUG_OFF
  rli->events_till_abort = abort_slave_event_count;
#endif

  thd = new THD; // note that contructor of THD uses DBUG_ !
  thd->thread_stack = (char*)&thd; // remember where our stack is
  rli->sql_thd= thd;
  
  /* Inform waiting threads that slave has started */
  rli->slave_run_id++;
  rli->slave_running = 1;

  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_SQL))
  {
    /*
      TODO: this is currently broken - slave start and change master
      will be stuck if we fail here
    */
    pthread_cond_broadcast(&rli->start_cond);
    pthread_mutex_unlock(&rli->run_lock);
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, 
                "Failed during slave thread initialization");
    goto err;
  }
  thd->init_for_queries();
  thd->temporary_tables = rli->save_temporary_tables; // restore temp tables
  set_thd_in_use_temporary_tables(rli);   // (re)set sql_thd in use for saved temp tables
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  /*
    We are going to set slave_running to 1. Assuming slave I/O thread is
    alive and connected, this is going to make Seconds_Behind_Master be 0
    i.e. "caught up". Even if we're just at start of thread. Well it's ok, at
    the moment we start we can think we are caught up, and the next second we
    start receiving data so we realize we are not caught up and
    Seconds_Behind_Master grows. No big deal.
  */
  rli->abort_slave = 0;
  pthread_mutex_unlock(&rli->run_lock);
  pthread_cond_broadcast(&rli->start_cond);

  /*
    Reset errors for a clean start (otherwise, if the master is idle, the SQL
    thread may execute no Query_log_event, so the error will remain even
    though there's no problem anymore). Do not reset the master timestamp
    (imagine the slave has caught everything, the STOP SLAVE and START SLAVE:
    as we are not sure that we are going to receive a query, we want to
    remember the last master timestamp (to say how many seconds behind we are
    now.
    But the master timestamp is reset by RESET SLAVE & CHANGE MASTER.
  */
  rli->clear_error();

  //tell the I/O thread to take relay_log_space_limit into account from now on
  pthread_mutex_lock(&rli->log_space_lock);
  rli->ignore_log_space_limit= 0;
  pthread_mutex_unlock(&rli->log_space_lock);
  rli->trans_retries= 0; // start from "no error"
  DBUG_PRINT("info", ("rli->trans_retries: %lu", rli->trans_retries));

  if (init_relay_log_pos(rli,
                         rli->group_relay_log_name,
                         rli->group_relay_log_pos,
                         1 /*need data lock*/, &errmsg,
                         1 /*look for a description_event*/))
  { 
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR, 
                "Error initializing relay log position: %s", errmsg);
    goto err;
  }
  THD_CHECK_SENTRY(thd);
#ifndef DBUG_OFF
  {
    char llbuf1[22], llbuf2[22];
    DBUG_PRINT("info", ("my_b_tell(rli->cur_log)=%s rli->event_relay_log_pos=%s",
                        llstr(my_b_tell(rli->cur_log),llbuf1),
                        llstr(rli->event_relay_log_pos,llbuf2)));
    DBUG_ASSERT(rli->event_relay_log_pos >= BIN_LOG_HEADER_SIZE);
    /*
      Wonder if this is correct. I (Guilhem) wonder if my_b_tell() returns the
      correct position when it's called just after my_b_seek() (the questionable
      stuff is those "seek is done on next read" comments in the my_b_seek()
      source code).
      The crude reality is that this assertion randomly fails whereas
      replication seems to work fine. And there is no easy explanation why it
      fails (as we my_b_seek(rli->event_relay_log_pos) at the very end of
      init_relay_log_pos() called above). Maybe the assertion would be
      meaningful if we held rli->data_lock between the my_b_seek() and the
      DBUG_ASSERT().
    */
#ifdef SHOULD_BE_CHECKED
    DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->event_relay_log_pos);
#endif
  }
#endif
  DBUG_ASSERT(rli->sql_thd == thd);

  DBUG_PRINT("master_info",("log_file_name: %s  position: %s",
                            rli->group_master_log_name,
                            llstr(rli->group_master_log_pos,llbuff)));
  if (global_system_variables.log_warnings)
    sql_print_information("Slave SQL thread initialized, starting replication in \
log '%s' at position %s, relay log '%s' position: %s", RPL_LOG_NAME,
                    llstr(rli->group_master_log_pos,llbuff),rli->group_relay_log_name,
                    llstr(rli->group_relay_log_pos,llbuff1));

  if (check_temp_dir(rli->slave_patternload_file))
  {
    rli->report(ERROR_LEVEL, thd->main_da.sql_errno(), 
                "Unable to use slave's temporary directory %s - %s", 
                slave_load_tmpdir, thd->main_da.message());
    goto err;
  }

  /* execute init_slave variable */
  if (sys_init_slave.value_length)
  {
    execute_init_command(thd, &sys_init_slave, &LOCK_sys_init_slave);
    if (thd->is_slave_error)
    {
      rli->report(ERROR_LEVEL, thd->main_da.sql_errno(), 
                  "Slave SQL thread aborted. Can't execute init_slave query");
      goto err;
    }
  }

  /*
    First check until condition - probably there is nothing to execute. We
    do not want to wait for next event in this case.
  */
  pthread_mutex_lock(&rli->data_lock);
  if (rli->until_condition != Relay_log_info::UNTIL_NONE &&
      rli->is_until_satisfied(rli->group_master_log_pos))
  {
    char buf[22];
    sql_print_information("Slave SQL thread stopped because it reached its"
                          " UNTIL position %s", llstr(rli->until_pos(), buf));
    pthread_mutex_unlock(&rli->data_lock);
    goto err;
  }
  pthread_mutex_unlock(&rli->data_lock);

  /* Read queries from the IO/THREAD until this thread is killed */

  while (!sql_slave_killed(thd,rli))
  {
    thd_proc_info(thd, "Reading event from the relay log");
    DBUG_ASSERT(rli->sql_thd == thd);
    THD_CHECK_SENTRY(thd);
    if (exec_relay_log_event(thd,rli))
    {
      DBUG_PRINT("info", ("exec_relay_log_event() failed"));
      // do not scare the user if SQL thread was simply killed or stopped
      if (!sql_slave_killed(thd,rli))
      {
        /*
          retrieve as much info as possible from the thd and, error
          codes and warnings and print this to the error log as to
          allow the user to locate the error
        */
        uint32 const last_errno= rli->last_error().number;

        if (thd->is_error())
        {
          char const *const errmsg= thd->main_da.message();

          DBUG_PRINT("info",
                     ("thd->main_da.sql_errno()=%d; rli->last_error.number=%d",
                      thd->main_da.sql_errno(), last_errno));
          if (last_errno == 0)
          {
            /*
 	      This function is reporting an error which was not reported
 	      while executing exec_relay_log_event().
 	    */ 
            rli->report(ERROR_LEVEL, thd->main_da.sql_errno(), "%s", errmsg);
          }
          else if (last_errno != thd->main_da.sql_errno())
          {
            /*
             * An error was reported while executing exec_relay_log_event()
             * however the error code differs from what is in the thread.
             * This function prints out more information to help finding
             * what caused the problem.
             */  
            sql_print_error("Slave (additional info): %s Error_code: %d",
                            errmsg, thd->main_da.sql_errno());
          }
        }

        /* Print any warnings issued */
        List_iterator_fast<MYSQL_ERROR> it(thd->warn_list);
        MYSQL_ERROR *err;
        /*
          Added controlled slave thread cancel for replication
          of user-defined variables.
        */
        bool udf_error = false;
        while ((err= it++))
        {
          if (err->code == ER_CANT_OPEN_LIBRARY)
            udf_error = true;
          sql_print_warning("Slave: %s Error_code: %d",err->msg, err->code);
        }
        if (udf_error)
          sql_print_error("Error loading user-defined library, slave SQL "
            "thread aborted. Install the missing library, and restart the "
            "slave SQL thread with \"SLAVE START\". We stopped at log '%s' "
            "position %s", RPL_LOG_NAME, llstr(rli->group_master_log_pos, 
            llbuff));
        else
          sql_print_error("\
Error running query, slave SQL thread aborted. Fix the problem, and restart \
the slave SQL thread with \"SLAVE START\". We stopped at log \
'%s' position %s", RPL_LOG_NAME, llstr(rli->group_master_log_pos, llbuff));
      }
      goto err;
    }
  }

  /* Thread stopped. Print the current replication position to the log */
  sql_print_information("Slave SQL thread exiting, replication stopped in log "
                        "'%s' at position %s",
                        RPL_LOG_NAME, llstr(rli->group_master_log_pos,llbuff));

 err:

  /*
    Some events set some playgrounds, which won't be cleared because thread
    stops. Stopping of this thread may not be known to these events ("stop"
    request is detected only by the present function, not by events), so we
    must "proactively" clear playgrounds:
  */
  rli->cleanup_context(thd, 1);
  /*
    Some extra safety, which should not been needed (normally, event deletion
    should already have done these assignments (each event which sets these
    variables is supposed to set them to 0 before terminating)).
  */
  thd->catalog= 0;
  thd->set_query(NULL, 0);
  thd->reset_db(NULL, 0);
  thd_proc_info(thd, "Waiting for slave mutex on exit");
  pthread_mutex_lock(&rli->run_lock);
  /* We need data_lock, at least to wake up any waiting master_pos_wait() */
  pthread_mutex_lock(&rli->data_lock);
  DBUG_ASSERT(rli->slave_running == 1); // tracking buffer overrun
  /* When master_pos_wait() wakes up it will check this and terminate */
  rli->slave_running= 0;
  /* Forget the relay log's format */
  delete rli->relay_log.description_event_for_exec;
  rli->relay_log.description_event_for_exec= 0;
  /* Wake up master_pos_wait() */
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_PRINT("info",("Signaling possibly waiting master_pos_wait() functions"));
  pthread_cond_broadcast(&rli->data_cond);
  rli->ignore_log_space_limit= 0; /* don't need any lock */
  /* we die so won't remember charset - re-update them on next thread start */
  rli->cached_charset_invalidate();
  rli->save_temporary_tables = thd->temporary_tables;

  /*
    TODO: see if we can do this conditionally in next_event() instead
    to avoid unneeded position re-init
  */
  thd->temporary_tables = 0; // remove tempation from destructor to close them
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because we are weird
  DBUG_ASSERT(rli->sql_thd == thd);
  THD_CHECK_SENTRY(thd);
  rli->sql_thd= 0;
  set_thd_in_use_temporary_tables(rli);  // (re)set sql_thd in use for saved temp tables
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
 /*
  Note: the order of the broadcast and unlock calls below (first broadcast, then unlock)
  is important. Otherwise a killer_thread can execute between the calls and
  delete the mi structure leading to a crash! (see BUG#25306 for details)
 */ 
  pthread_cond_broadcast(&rli->stop_cond);
  DBUG_EXECUTE_IF("simulate_slave_delay_at_terminate_bug38694", sleep(5););
  pthread_mutex_unlock(&rli->run_lock);  // tell the world we are done

  DBUG_LEAVE;                                   // Must match DBUG_ENTER()
  my_thread_end();
  pthread_exit(0);
  return 0;                                     // Avoid compiler warnings
}


/*
  process_io_create_file()
*/

static int process_io_create_file(Master_info* mi, Create_file_log_event* cev)
{
  int error = 1;
  ulong num_bytes;
  bool cev_not_written;
  THD *thd = mi->io_thd;
  NET *net = &mi->mysql->net;
  DBUG_ENTER("process_io_create_file");

  if (unlikely(!cev->is_valid()))
    DBUG_RETURN(1);

  if (!rpl_filter->db_ok(cev->db))
  {
    skip_load_data_infile(net);
    DBUG_RETURN(0);
  }
  DBUG_ASSERT(cev->inited_from_old);
  thd->file_id = cev->file_id = mi->file_id++;
  thd->server_id = cev->server_id;
  cev_not_written = 1;

  if (unlikely(net_request_file(net,cev->fname)))
  {
    sql_print_error("Slave I/O: failed requesting download of '%s'",
                    cev->fname);
    goto err;
  }

  /*
    This dummy block is so we could instantiate Append_block_log_event
    once and then modify it slightly instead of doing it multiple times
    in the loop
  */
  {
    Append_block_log_event aev(thd,0,0,0,0);

    for (;;)
    {
      if (unlikely((num_bytes=my_net_read(net)) == packet_error))
      {
        sql_print_error("Network read error downloading '%s' from master",
                        cev->fname);
        goto err;
      }
      if (unlikely(!num_bytes)) /* eof */
      {
	/* 3.23 master wants it */
        net_write_command(net, 0, (uchar*) "", 0, (uchar*) "", 0);
        /*
          If we wrote Create_file_log_event, then we need to write
          Execute_load_log_event. If we did not write Create_file_log_event,
          then this is an empty file and we can just do as if the LOAD DATA
          INFILE had not existed, i.e. write nothing.
        */
        if (unlikely(cev_not_written))
          break;
        Execute_load_log_event xev(thd,0,0);
        xev.log_pos = cev->log_pos;
        if (unlikely(mi->rli.relay_log.append(&xev)))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Exec_load event to relay log");
          goto err;
        }
        mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
        break;
      }
      if (unlikely(cev_not_written))
      {
        cev->block = net->read_pos;
        cev->block_len = num_bytes;
        if (unlikely(mi->rli.relay_log.append(cev)))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Create_file event to relay log");
          goto err;
        }
        cev_not_written=0;
        mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
      }
      else
      {
        aev.block = net->read_pos;
        aev.block_len = num_bytes;
        aev.log_pos = cev->log_pos;
        if (unlikely(mi->rli.relay_log.append(&aev)))
        {
          mi->report(ERROR_LEVEL, ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
                     ER(ER_SLAVE_RELAY_LOG_WRITE_FAILURE),
                     "error writing Append_block event to relay log");
          goto err;
        }
        mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total) ;
      }
    }
  }
  error=0;
err:
  DBUG_RETURN(error);
}


/*
  Start using a new binary log on the master

  SYNOPSIS
    process_io_rotate()
    mi                  master_info for the slave
    rev                 The rotate log event read from the binary log

  DESCRIPTION
    Updates the master info with the place in the next binary
    log where we should start reading.
    Rotate the relay log to avoid mixed-format relay logs.

  NOTES
    We assume we already locked mi->data_lock

  RETURN VALUES
    0           ok
    1           Log event is illegal

*/

static int process_io_rotate(Master_info *mi, Rotate_log_event *rev)
{
  DBUG_ENTER("process_io_rotate");
  safe_mutex_assert_owner(&mi->data_lock);

  if (unlikely(!rev->is_valid()))
    DBUG_RETURN(1);

  /* Safe copy as 'rev' has been "sanitized" in Rotate_log_event's ctor */
  memcpy(mi->master_log_name, rev->new_log_ident, rev->ident_len+1);
  mi->master_log_pos= rev->pos;
  DBUG_PRINT("info", ("master_log_pos: '%s' %lu",
                      mi->master_log_name, (ulong) mi->master_log_pos));
#ifndef DBUG_OFF
  /*
    If we do not do this, we will be getting the first
    rotate event forever, so we need to not disconnect after one.
  */
  if (disconnect_slave_event_count)
    mi->events_till_disconnect++;
#endif

  /*
    If description_event_for_queue is format <4, there is conversion in the
    relay log to the slave's format (4). And Rotate can mean upgrade or
    nothing. If upgrade, it's to 5.0 or newer, so we will get a Format_desc, so
    no need to reset description_event_for_queue now. And if it's nothing (same
    master version as before), no need (still using the slave's format).
  */
  if (mi->rli.relay_log.description_event_for_queue->binlog_version >= 4)
  {
    delete mi->rli.relay_log.description_event_for_queue;
    /* start from format 3 (MySQL 4.0) again */
    mi->rli.relay_log.description_event_for_queue= new
      Format_description_log_event(3);
  }
  /*
    Rotate the relay log makes binlog format detection easier (at next slave
    start or mysqlbinlog)
  */
  rotate_relay_log(mi); /* will take the right mutexes */
  DBUG_RETURN(0);
}

/*
  Reads a 3.23 event and converts it to the slave's format. This code was
  copied from MySQL 4.0.
*/
static int queue_binlog_ver_1_event(Master_info *mi, const char *buf,
                           ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  bool ignore_event= 0;
  char *tmp_buf = 0;
  Relay_log_info *rli= &mi->rli;
  DBUG_ENTER("queue_binlog_ver_1_event");

  /*
    If we get Load event, we need to pass a non-reusable buffer
    to read_log_event, so we do a trick
  */
  if (buf[EVENT_TYPE_OFFSET] == LOAD_EVENT)
  {
    if (unlikely(!(tmp_buf=(char*)my_malloc(event_len+1,MYF(MY_WME)))))
    {
      mi->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                 ER(ER_SLAVE_FATAL_ERROR), "Memory allocation failed");
      DBUG_RETURN(1);
    }
    memcpy(tmp_buf,buf,event_len);
    /*
      Create_file constructor wants a 0 as last char of buffer, this 0 will
      serve as the string-termination char for the file's name (which is at the
      end of the buffer)
      We must increment event_len, otherwise the event constructor will not see
      this end 0, which leads to segfault.
    */
    tmp_buf[event_len++]=0;
    int4store(tmp_buf+EVENT_LEN_OFFSET, event_len);
    buf = (const char*)tmp_buf;
  }
  /*
    This will transform LOAD_EVENT into CREATE_FILE_EVENT, ask the master to
    send the loaded file, and write it to the relay log in the form of
    Append_block/Exec_load (the SQL thread needs the data, as that thread is not
    connected to the master).
  */
  Log_event *ev = Log_event::read_log_event(buf,event_len, &errmsg,
                                            mi->rli.relay_log.description_event_for_queue);
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
                    errmsg);
    my_free((char*) tmp_buf, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }

  pthread_mutex_lock(&mi->data_lock);
  ev->log_pos= mi->master_log_pos; /* 3.23 events don't contain log_pos */
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    ignore_event= 1;
    inc_pos= event_len;
    break;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    inc_pos= 0;
    break;
  case CREATE_FILE_EVENT:
    /*
      Yes it's possible to have CREATE_FILE_EVENT here, even if we're in
      queue_old_event() which is for 3.23 events which don't comprise
      CREATE_FILE_EVENT. This is because read_log_event() above has just
      transformed LOAD_EVENT into CREATE_FILE_EVENT.
    */
  {
    /* We come here when and only when tmp_buf != 0 */
    DBUG_ASSERT(tmp_buf != 0);
    inc_pos=event_len;
    ev->log_pos+= inc_pos;
    int error = process_io_create_file(mi,(Create_file_log_event*)ev);
    delete ev;
    mi->master_log_pos += inc_pos;
    DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
    pthread_mutex_unlock(&mi->data_lock);
    my_free((char*)tmp_buf, MYF(0));
    DBUG_RETURN(error);
  }
  default:
    inc_pos= event_len;
    break;
  }
  if (likely(!ignore_event))
  {
    if (ev->log_pos)
      /*
         Don't do it for fake Rotate events (see comment in
      Log_event::Log_event(const char* buf...) in log_event.cc).
      */
      ev->log_pos+= event_len; /* make log_pos be the pos of the end of the event */
    if (unlikely(rli->relay_log.append(ev)))
    {
      delete ev;
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  }
  delete ev;
  mi->master_log_pos+= inc_pos;
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(0);
}

/*
  Reads a 4.0 event and converts it to the slave's format. This code was copied
  from queue_binlog_ver_1_event(), with some affordable simplifications.
*/
static int queue_binlog_ver_3_event(Master_info *mi, const char *buf,
                           ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  char *tmp_buf = 0;
  Relay_log_info *rli= &mi->rli;
  DBUG_ENTER("queue_binlog_ver_3_event");

  /* read_log_event() will adjust log_pos to be end_log_pos */
  Log_event *ev = Log_event::read_log_event(buf,event_len, &errmsg,
                                            mi->rli.relay_log.description_event_for_queue);
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
                    errmsg);
    my_free((char*) tmp_buf, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
  pthread_mutex_lock(&mi->data_lock);
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    goto err;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    inc_pos= 0;
    break;
  default:
    inc_pos= event_len;
    break;
  }
  if (unlikely(rli->relay_log.append(ev)))
  {
    delete ev;
    pthread_mutex_unlock(&mi->data_lock);
    DBUG_RETURN(1);
  }
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  delete ev;
  mi->master_log_pos+= inc_pos;
err:
  DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(0);
}

/*
  queue_old_event()

  Writes a 3.23 or 4.0 event to the relay log, after converting it to the 5.0
  (exactly, slave's) format. To do the conversion, we create a 5.0 event from
  the 3.23/4.0 bytes, then write this event to the relay log.

  TODO:
    Test this code before release - it has to be tested on a separate
    setup with 3.23 master or 4.0 master
*/

static int queue_old_event(Master_info *mi, const char *buf,
                           ulong event_len)
{
  DBUG_ENTER("queue_old_event");

  switch (mi->rli.relay_log.description_event_for_queue->binlog_version)
  {
  case 1:
      DBUG_RETURN(queue_binlog_ver_1_event(mi,buf,event_len));
  case 3:
      DBUG_RETURN(queue_binlog_ver_3_event(mi,buf,event_len));
  default: /* unsupported format; eg version 2 */
    DBUG_PRINT("info",("unsupported binlog format %d in queue_old_event()",
                       mi->rli.relay_log.description_event_for_queue->binlog_version));
    DBUG_RETURN(1);
  }
}

/*
  queue_event()

  If the event is 3.23/4.0, passes it to queue_old_event() which will convert
  it. Otherwise, writes a 5.0 (or newer) event to the relay log. Then there is
  no format conversion, it's pure read/write of bytes.
  So a 5.0.0 slave's relay log can contain events in the slave's format or in
  any >=5.0.0 format.
*/

static int queue_event(Master_info* mi,const char* buf, ulong event_len)
{
  int error= 0;
  ulong inc_pos;
  Relay_log_info *rli= &mi->rli;
  pthread_mutex_t *log_lock= rli->relay_log.get_log_lock();
  DBUG_ENTER("queue_event");

  LINT_INIT(inc_pos);

  if (mi->rli.relay_log.description_event_for_queue->binlog_version<4 &&
      buf[EVENT_TYPE_OFFSET] != FORMAT_DESCRIPTION_EVENT /* a way to escape */)
    DBUG_RETURN(queue_old_event(mi,buf,event_len));

  LINT_INIT(inc_pos);
  pthread_mutex_lock(&mi->data_lock);

  switch (buf[EVENT_TYPE_OFFSET]) {
  case STOP_EVENT:
    /*
      We needn't write this event to the relay log. Indeed, it just indicates a
      master server shutdown. The only thing this does is cleaning. But
      cleaning is already done on a per-master-thread basis (as the master
      server is shutting down cleanly, it has written all DROP TEMPORARY TABLE
      prepared statements' deletion are TODO only when we binlog prep stmts).

      We don't even increment mi->master_log_pos, because we may be just after
      a Rotate event. Btw, in a few milliseconds we are going to have a Start
      event from the next binlog (unless the master is presently running
      without --log-bin).
    */
    goto err;
  case ROTATE_EVENT:
  {
    Rotate_log_event rev(buf,event_len,mi->rli.relay_log.description_event_for_queue);
    if (unlikely(process_io_rotate(mi,&rev)))
    {
      error= 1;
      goto err;
    }
    /*
      Now the I/O thread has just changed its mi->master_log_name, so
      incrementing mi->master_log_pos is nonsense.
    */
    inc_pos= 0;
    break;
  }
  case FORMAT_DESCRIPTION_EVENT:
  {
    /*
      Create an event, and save it (when we rotate the relay log, we will have
      to write this event again).
    */
    /*
      We are the only thread which reads/writes description_event_for_queue.
      The relay_log struct does not move (though some members of it can
      change), so we needn't any lock (no rli->data_lock, no log lock).
    */
    Format_description_log_event* tmp;
    const char* errmsg;
    if (!(tmp= (Format_description_log_event*)
          Log_event::read_log_event(buf, event_len, &errmsg,
                                    mi->rli.relay_log.description_event_for_queue)))
    {
      error= 2;
      goto err;
    }
    delete mi->rli.relay_log.description_event_for_queue;
    mi->rli.relay_log.description_event_for_queue= tmp;
    /*
       Though this does some conversion to the slave's format, this will
       preserve the master's binlog format version, and number of event types.
    */
    /*
       If the event was not requested by the slave (the slave did not ask for
       it), i.e. has end_log_pos=0, we do not increment mi->master_log_pos
    */
    inc_pos= uint4korr(buf+LOG_POS_OFFSET) ? event_len : 0;
    DBUG_PRINT("info",("binlog format is now %d",
                       mi->rli.relay_log.description_event_for_queue->binlog_version));

  }
  break;
  default:
    inc_pos= event_len;
    break;
  }

  /*
     If this event is originating from this server, don't queue it.
     We don't check this for 3.23 events because it's simpler like this; 3.23
     will be filtered anyway by the SQL slave thread which also tests the
     server id (we must also keep this test in the SQL thread, in case somebody
     upgrades a 4.0 slave which has a not-filtered relay log).

     ANY event coming from ourselves can be ignored: it is obvious for queries;
     for STOP_EVENT/ROTATE_EVENT/START_EVENT: these cannot come from ourselves
     (--log-slave-updates would not log that) unless this slave is also its
     direct master (an unsupported, useless setup!).
  */

  pthread_mutex_lock(log_lock);

  if ((uint4korr(buf + SERVER_ID_OFFSET) == ::server_id) &&
      !mi->rli.replicate_same_server_id)
  {
    /*
      Do not write it to the relay log.
      a) We still want to increment mi->master_log_pos, so that we won't
      re-read this event from the master if the slave IO thread is now
      stopped/restarted (more efficient if the events we are ignoring are big
      LOAD DATA INFILE).
      b) We want to record that we are skipping events, for the information of
      the slave SQL thread, otherwise that thread may let
      rli->group_relay_log_pos stay too small if the last binlog's event is
      ignored.
      But events which were generated by this slave and which do not exist in
      the master's binlog (i.e. Format_desc, Rotate & Stop) should not increment
      mi->master_log_pos.
    */
    if (buf[EVENT_TYPE_OFFSET]!=FORMAT_DESCRIPTION_EVENT &&
        buf[EVENT_TYPE_OFFSET]!=ROTATE_EVENT &&
        buf[EVENT_TYPE_OFFSET]!=STOP_EVENT)
    {
      mi->master_log_pos+= inc_pos;
      memcpy(rli->ign_master_log_name_end, mi->master_log_name, FN_REFLEN);
      DBUG_ASSERT(rli->ign_master_log_name_end[0]);
      rli->ign_master_log_pos_end= mi->master_log_pos;
    }
    rli->relay_log.signal_update(); // the slave SQL thread needs to re-check
    DBUG_PRINT("info", ("master_log_pos: %lu, event originating from the same server, ignored",
                        (ulong) mi->master_log_pos));
  }
  else
  {
    /* write the event to the relay log */
    if (likely(!(rli->relay_log.appendv(buf,event_len,0))))
    {
      mi->master_log_pos+= inc_pos;
      DBUG_PRINT("info", ("master_log_pos: %lu", (ulong) mi->master_log_pos));
      rli->relay_log.harvest_bytes_written(&rli->log_space_total);
    }
    else
      error= 3;
    rli->ign_master_log_name_end[0]= 0; // last event is not ignored
  }
  pthread_mutex_unlock(log_lock);


err:
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_PRINT("info", ("error: %d", error));
  DBUG_RETURN(error);
}


void end_relay_log_info(Relay_log_info* rli)
{
  DBUG_ENTER("end_relay_log_info");

  if (!rli->inited)
    DBUG_VOID_RETURN;
  if (rli->info_fd >= 0)
  {
    end_io_cache(&rli->info_file);
    (void) my_close(rli->info_fd, MYF(MY_WME));
    rli->info_fd = -1;
  }
  if (rli->cur_log_fd >= 0)
  {
    end_io_cache(&rli->cache_buf);
    (void)my_close(rli->cur_log_fd, MYF(MY_WME));
    rli->cur_log_fd = -1;
  }
  rli->inited = 0;
  rli->relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT);
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  /*
    Delete the slave's temporary tables from memory.
    In the future there will be other actions than this, to ensure persistance
    of slave's temp tables after shutdown.
  */
  rli->close_temporary_tables();
  DBUG_VOID_RETURN;
}


/**
  Hook to detach the active VIO before closing a connection handle.

  The client API might close the connection (and associated data)
  in case it encounters a unrecoverable (network) error. This hook
  is called from the client code before the VIO handle is deleted
  allows the thread to detach the active vio so it does not point
  to freed memory.

  Other calls to THD::clear_active_vio throughout this module are
  redundant due to the hook but are left in place for illustrative
  purposes.
*/

extern "C" void slave_io_thread_detach_vio()
{
#ifdef SIGNAL_WITH_VIO_CLOSE
  THD *thd= current_thd;
  if (thd && thd->slave_thread)
    thd->clear_active_vio();
#endif
}


/*
  Try to connect until successful or slave killed

  SYNPOSIS
    safe_connect()
    thd                 Thread handler for slave
    mysql               MySQL connection handle
    mi                  Replication handle

  RETURN
    0   ok
    #   Error
*/

static int safe_connect(THD* thd, MYSQL* mysql, Master_info* mi)
{
  DBUG_ENTER("safe_connect");

  DBUG_RETURN(connect_to_master(thd, mysql, mi, 0, 0));
}


/*
  SYNPOSIS
    connect_to_master()

  IMPLEMENTATION
    Try to connect until successful or slave killed or we have retried
    master_retry_count times
*/

static int connect_to_master(THD* thd, MYSQL* mysql, Master_info* mi,
                             bool reconnect, bool suppress_warnings)
{
  int slave_was_killed;
  int last_errno= -2;                           // impossible error
  ulong err_count=0;
  char llbuff[22];
  DBUG_ENTER("connect_to_master");

#ifndef DBUG_OFF
  mi->events_till_disconnect = disconnect_slave_event_count;
#endif
  ulong client_flag= CLIENT_REMEMBER_OPTIONS;
  if (opt_slave_compressed_protocol)
    client_flag=CLIENT_COMPRESS;                /* We will use compression */

  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *) &slave_net_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, (char *) &slave_net_timeout);

#ifdef HAVE_OPENSSL
  if (mi->ssl)
  {
    mysql_ssl_set(mysql,
                  mi->ssl_key[0]?mi->ssl_key:0,
                  mi->ssl_cert[0]?mi->ssl_cert:0,
                  mi->ssl_ca[0]?mi->ssl_ca:0,
                  mi->ssl_capath[0]?mi->ssl_capath:0,
                  mi->ssl_cipher[0]?mi->ssl_cipher:0);
    mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &mi->ssl_verify_server_cert);
  }
#endif

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  /* This one is not strictly needed but we have it here for completeness */
  mysql_options(mysql, MYSQL_SET_CHARSET_DIR, (char *) charsets_dir);

  while (!(slave_was_killed = io_slave_killed(thd,mi)) &&
         (reconnect ? mysql_reconnect(mysql) != 0 :
          mysql_real_connect(mysql, mi->host, mi->user, mi->password, 0,
                             mi->port, 0, client_flag) == 0))
  {
    /* Don't repeat last error */
    if ((int)mysql_errno(mysql) != last_errno)
    {
      last_errno=mysql_errno(mysql);
      suppress_warnings= 0;
      mi->report(ERROR_LEVEL, last_errno,
                 "error %s to master '%s@%s:%d'"
                 " - retry-time: %d  retries: %lu",
                 (reconnect ? "reconnecting" : "connecting"),
                 mi->user, mi->host, mi->port,
                 mi->connect_retry, master_retry_count);
    }
    /*
      By default we try forever. The reason is that failure will trigger
      master election, so if the user did not set master_retry_count we
      do not want to have election triggered on the first failure to
      connect
    */
    if (++err_count == master_retry_count)
    {
      slave_was_killed=1;
      if (reconnect)
        change_rpl_status(RPL_ACTIVE_SLAVE,RPL_LOST_SOLDIER);
      break;
    }
    safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
               (void*)mi);
  }

  if (!slave_was_killed)
  {
    mi->clear_error(); // clear possible left over reconnect error
    if (reconnect)
    {
      if (!suppress_warnings && global_system_variables.log_warnings)
        sql_print_information("Slave: connected to master '%s@%s:%d',\
replication resumed in log '%s' at position %s", mi->user,
                        mi->host, mi->port,
                        IO_RPL_LOG_NAME,
                        llstr(mi->master_log_pos,llbuff));
    }
    else
    {
      change_rpl_status(RPL_IDLE_SLAVE,RPL_ACTIVE_SLAVE);
      general_log_print(thd, COM_CONNECT_OUT, "%s@%s:%d",
                        mi->user, mi->host, mi->port);
    }
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->set_active_vio(mysql->net.vio);
#endif
  }
  mysql->reconnect= 1;
  DBUG_PRINT("exit",("slave_was_killed: %d", slave_was_killed));
  DBUG_RETURN(slave_was_killed);
}


/*
  safe_reconnect()

  IMPLEMENTATION
    Try to connect until successful or slave killed or we have retried
    master_retry_count times
*/

static int safe_reconnect(THD* thd, MYSQL* mysql, Master_info* mi,
                          bool suppress_warnings)
{
  DBUG_ENTER("safe_reconnect");
  DBUG_RETURN(connect_to_master(thd, mysql, mi, 1, suppress_warnings));
}


/*
  Store the file and position where the execute-slave thread are in the
  relay log.

  SYNOPSIS
    flush_relay_log_info()
    rli                 Relay log information

  NOTES
    - As this is only called by the slave thread, we don't need to
      have a lock on this.
    - If there is an active transaction, then we don't update the position
      in the relay log.  This is to ensure that we re-execute statements
      if we die in the middle of an transaction that was rolled back.
    - As a transaction never spans binary logs, we don't have to handle the
      case where we do a relay-log-rotation in the middle of the transaction.
      If this would not be the case, we would have to ensure that we
      don't delete the relay log file where the transaction started when
      we switch to a new relay log file.

  TODO
    - Change the log file information to a binary format to avoid calling
      longlong2str.

  RETURN VALUES
    0   ok
    1   write error
*/

bool flush_relay_log_info(Relay_log_info* rli)
{
  bool error=0;
  DBUG_ENTER("flush_relay_log_info");

  if (unlikely(rli->no_storage))
    DBUG_RETURN(0);

  IO_CACHE *file = &rli->info_file;
  char buff[FN_REFLEN*2+22*2+4], *pos;

  my_b_seek(file, 0L);
  pos=strmov(buff, rli->group_relay_log_name);
  *pos++='\n';
  pos=longlong2str(rli->group_relay_log_pos, pos, 10);
  *pos++='\n';
  pos=strmov(pos, rli->group_master_log_name);
  *pos++='\n';
  pos=longlong2str(rli->group_master_log_pos, pos, 10);
  *pos='\n';
  if (my_b_write(file, (uchar*) buff, (size_t) (pos-buff)+1))
    error=1;
  if (flush_io_cache(file))
    error=1;

  /* Flushing the relay log is done by the slave I/O thread */
  DBUG_RETURN(error);
}


/*
  Called when we notice that the current "hot" log got rotated under our feet.
*/

static IO_CACHE *reopen_relay_log(Relay_log_info *rli, const char **errmsg)
{
  DBUG_ENTER("reopen_relay_log");
  DBUG_ASSERT(rli->cur_log != &rli->cache_buf);
  DBUG_ASSERT(rli->cur_log_fd == -1);

  IO_CACHE *cur_log = rli->cur_log=&rli->cache_buf;
  if ((rli->cur_log_fd=open_binlog(cur_log,rli->event_relay_log_name,
                                   errmsg)) <0)
    DBUG_RETURN(0);
  /*
    We want to start exactly where we was before:
    relay_log_pos       Current log pos
    pending             Number of bytes already processed from the event
  */
  rli->event_relay_log_pos= max(rli->event_relay_log_pos, BIN_LOG_HEADER_SIZE);
  my_b_seek(cur_log,rli->event_relay_log_pos);
  DBUG_RETURN(cur_log);
}


/**
  Reads next event from the relay log.  Should be called from the
  slave IO thread.

  @param rli Relay_log_info structure for the slave IO thread.

  @return The event read, or NULL on error.  If an error occurs, the
  error is reported through the sql_print_information() or
  sql_print_error() functions.
*/
static Log_event* next_event(Relay_log_info* rli)
{
  Log_event* ev;
  IO_CACHE* cur_log = rli->cur_log;
  pthread_mutex_t *log_lock = rli->relay_log.get_log_lock();
  const char* errmsg=0;
  THD* thd = rli->sql_thd;
  DBUG_ENTER("next_event");

  DBUG_ASSERT(thd != 0);

#ifndef DBUG_OFF
  if (abort_slave_event_count && !rli->events_till_abort--)
    DBUG_RETURN(0);
#endif

  /*
    For most operations we need to protect rli members with data_lock,
    so we assume calling function acquired this mutex for us and we will
    hold it for the most of the loop below However, we will release it
    whenever it is worth the hassle,  and in the cases when we go into a
    pthread_cond_wait() with the non-data_lock mutex
  */
  safe_mutex_assert_owner(&rli->data_lock);

  while (!sql_slave_killed(thd,rli))
  {
    /*
      We can have two kinds of log reading:
      hot_log:
        rli->cur_log points at the IO_CACHE of relay_log, which
        is actively being updated by the I/O thread. We need to be careful
        in this case and make sure that we are not looking at a stale log that
        has already been rotated. If it has been, we reopen the log.

      The other case is much simpler:
        We just have a read only log that nobody else will be updating.
    */
    bool hot_log;
    if ((hot_log = (cur_log != &rli->cache_buf)))
    {
      DBUG_ASSERT(rli->cur_log_fd == -1); // foreign descriptor
      pthread_mutex_lock(log_lock);

      /*
        Reading xxx_file_id is safe because the log will only
        be rotated when we hold relay_log.LOCK_log
      */
      if (rli->relay_log.get_open_count() != rli->cur_log_old_open_count)
      {
        // The master has switched to a new log file; Reopen the old log file
        cur_log=reopen_relay_log(rli, &errmsg);
        pthread_mutex_unlock(log_lock);
        if (!cur_log)                           // No more log files
          goto err;
        hot_log=0;                              // Using old binary log
      }
    }
    /* 
      As there is no guarantee that the relay is open (for example, an I/O
      error during a write by the slave I/O thread may have closed it), we
      have to test it.
    */
    if (!my_b_inited(cur_log))
      goto err;
#ifndef DBUG_OFF
    {
      /* This is an assertion which sometimes fails, let's try to track it */
      char llbuf1[22], llbuf2[22];
      DBUG_PRINT("info", ("my_b_tell(cur_log)=%s rli->event_relay_log_pos=%s",
                          llstr(my_b_tell(cur_log),llbuf1),
                          llstr(rli->event_relay_log_pos,llbuf2)));
      DBUG_ASSERT(my_b_tell(cur_log) >= BIN_LOG_HEADER_SIZE);
      DBUG_ASSERT(my_b_tell(cur_log) == rli->event_relay_log_pos);
    }
#endif
    /*
      Relay log is always in new format - if the master is 3.23, the
      I/O thread will convert the format for us.
      A problem: the description event may be in a previous relay log. So if
      the slave has been shutdown meanwhile, we would have to look in old relay
      logs, which may even have been deleted. So we need to write this
      description event at the beginning of the relay log.
      When the relay log is created when the I/O thread starts, easy: the
      master will send the description event and we will queue it.
      But if the relay log is created by new_file(): then the solution is:
      MYSQL_BIN_LOG::open() will write the buffered description event.
    */
    if ((ev=Log_event::read_log_event(cur_log,0,
                                      rli->relay_log.description_event_for_exec)))

    {
      DBUG_ASSERT(thd==rli->sql_thd);
      /*
        read it while we have a lock, to avoid a mutex lock in
        inc_event_relay_log_pos()
      */
      rli->future_event_relay_log_pos= my_b_tell(cur_log);
      if (hot_log)
        pthread_mutex_unlock(log_lock);
      DBUG_RETURN(ev);
    }
    DBUG_ASSERT(thd==rli->sql_thd);
    if (opt_reckless_slave)                     // For mysql-test
      cur_log->error = 0;
    if (cur_log->error < 0)
    {
      errmsg = "slave SQL thread aborted because of I/O error";
      if (hot_log)
        pthread_mutex_unlock(log_lock);
      goto err;
    }
    if (!cur_log->error) /* EOF */
    {
      /*
        On a hot log, EOF means that there are no more updates to
        process and we must block until I/O thread adds some and
        signals us to continue
      */
      if (hot_log)
      {
        /*
          We say in Seconds_Behind_Master that we have "caught up". Note that
          for example if network link is broken but I/O slave thread hasn't
          noticed it (slave_net_timeout not elapsed), then we'll say "caught
          up" whereas we're not really caught up. Fixing that would require
          internally cutting timeout in smaller pieces in network read, no
          thanks. Another example: SQL has caught up on I/O, now I/O has read
          a new event and is queuing it; the false "0" will exist until SQL
          finishes executing the new event; it will be look abnormal only if
          the events have old timestamps (then you get "many", 0, "many").

          Transient phases like this can be fixed with implemeting
          Heartbeat event which provides the slave the status of the
          master at time the master does not have any new update to send.
          Seconds_Behind_Master would be zero only when master has no
          more updates in binlog for slave. The heartbeat can be sent
          in a (small) fraction of slave_net_timeout. Until it's done
          rli->last_master_timestamp is temporarely (for time of
          waiting for the following event) reset whenever EOF is
          reached.
        */
        time_t save_timestamp= rli->last_master_timestamp;
        rli->last_master_timestamp= 0;

        DBUG_ASSERT(rli->relay_log.get_open_count() ==
                    rli->cur_log_old_open_count);

        if (rli->ign_master_log_name_end[0])
        {
          /* We generate and return a Rotate, to make our positions advance */
          DBUG_PRINT("info",("seeing an ignored end segment"));
          ev= new Rotate_log_event(rli->ign_master_log_name_end,
                                   0, rli->ign_master_log_pos_end,
                                   Rotate_log_event::DUP_NAME);
          rli->ign_master_log_name_end[0]= 0;
          pthread_mutex_unlock(log_lock);
          if (unlikely(!ev))
          {
            errmsg= "Slave SQL thread failed to create a Rotate event "
              "(out of memory?), SHOW SLAVE STATUS may be inaccurate";
            goto err;
          }
          ev->server_id= 0; // don't be ignored by slave SQL thread
          DBUG_RETURN(ev);
        }

        /*
          We can, and should release data_lock while we are waiting for
          update. If we do not, show slave status will block
        */
        pthread_mutex_unlock(&rli->data_lock);

        /*
          Possible deadlock :
          - the I/O thread has reached log_space_limit
          - the SQL thread has read all relay logs, but cannot purge for some
          reason:
            * it has already purged all logs except the current one
            * there are other logs than the current one but they're involved in
            a transaction that finishes in the current one (or is not finished)
          Solution :
          Wake up the possibly waiting I/O thread, and set a boolean asking
          the I/O thread to temporarily ignore the log_space_limit
          constraint, because we do not want the I/O thread to block because of
          space (it's ok if it blocks for any other reason (e.g. because the
          master does not send anything). Then the I/O thread stops waiting
          and reads more events.
          The SQL thread decides when the I/O thread should take log_space_limit
          into account again : ignore_log_space_limit is reset to 0
          in purge_first_log (when the SQL thread purges the just-read relay
          log), and also when the SQL thread starts. We should also reset
          ignore_log_space_limit to 0 when the user does RESET SLAVE, but in
          fact, no need as RESET SLAVE requires that the slave
          be stopped, and the SQL thread sets ignore_log_space_limit to 0 when
          it stops.
        */
        pthread_mutex_lock(&rli->log_space_lock);
        // prevent the I/O thread from blocking next times
        rli->ignore_log_space_limit= 1;
        /*
          If the I/O thread is blocked, unblock it.  Ok to broadcast
          after unlock, because the mutex is only destroyed in
          ~Relay_log_info(), i.e. when rli is destroyed, and rli will
          not be destroyed before we exit the present function.
        */
        pthread_mutex_unlock(&rli->log_space_lock);
        pthread_cond_broadcast(&rli->log_space_cond);
        // Note that wait_for_update unlocks lock_log !
        rli->relay_log.wait_for_update(rli->sql_thd, 1);
        // re-acquire data lock since we released it earlier
        pthread_mutex_lock(&rli->data_lock);
        rli->last_master_timestamp= save_timestamp;
        continue;
      }
      /*
        If the log was not hot, we need to move to the next log in
        sequence. The next log could be hot or cold, we deal with both
        cases separately after doing some common initialization
      */
      end_io_cache(cur_log);
      DBUG_ASSERT(rli->cur_log_fd >= 0);
      my_close(rli->cur_log_fd, MYF(MY_WME));
      rli->cur_log_fd = -1;

      if (relay_log_purge)
      {
        /*
          purge_first_log will properly set up relay log coordinates in rli.
          If the group's coordinates are equal to the event's coordinates
          (i.e. the relay log was not rotated in the middle of a group),
          we can purge this relay log too.
          We do ulonglong and string comparisons, this may be slow but
          - purging the last relay log is nice (it can save 1GB of disk), so we
          like to detect the case where we can do it, and given this,
          - I see no better detection method
          - purge_first_log is not called that often
        */
        if (rli->relay_log.purge_first_log
            (rli,
             rli->group_relay_log_pos == rli->event_relay_log_pos
             && !strcmp(rli->group_relay_log_name,rli->event_relay_log_name)))
        {
          errmsg = "Error purging processed logs";
          goto err;
        }
      }
      else
      {
        /*
          If hot_log is set, then we already have a lock on
          LOCK_log.  If not, we have to get the lock.

          According to Sasha, the only time this code will ever be executed
          is if we are recovering from a bug.
        */
        if (rli->relay_log.find_next_log(&rli->linfo, !hot_log))
        {
          errmsg = "error switching to the next log";
          goto err;
        }
        rli->event_relay_log_pos = BIN_LOG_HEADER_SIZE;
        strmake(rli->event_relay_log_name,rli->linfo.log_file_name,
                sizeof(rli->event_relay_log_name)-1);
        flush_relay_log_info(rli);
      }

      /*
        Now we want to open this next log. To know if it's a hot log (the one
        being written by the I/O thread now) or a cold log, we can use
        is_active(); if it is hot, we use the I/O cache; if it's cold we open
        the file normally. But if is_active() reports that the log is hot, this
        may change between the test and the consequence of the test. So we may
        open the I/O cache whereas the log is now cold, which is nonsense.
        To guard against this, we need to have LOCK_log.
      */

      DBUG_PRINT("info",("hot_log: %d",hot_log));
      if (!hot_log) /* if hot_log, we already have this mutex */
        pthread_mutex_lock(log_lock);
      if (rli->relay_log.is_active(rli->linfo.log_file_name))
      {
#ifdef EXTRA_DEBUG
        if (global_system_variables.log_warnings)
          sql_print_information("next log '%s' is currently active",
                                rli->linfo.log_file_name);
#endif
        rli->cur_log= cur_log= rli->relay_log.get_log_file();
        rli->cur_log_old_open_count= rli->relay_log.get_open_count();
        DBUG_ASSERT(rli->cur_log_fd == -1);

        /*
          Read pointer has to be at the start since we are the only
          reader.
          We must keep the LOCK_log to read the 4 first bytes, as this is a hot
          log (same as when we call read_log_event() above: for a hot log we
          take the mutex).
        */
        if (check_binlog_magic(cur_log,&errmsg))
        {
          if (!hot_log) pthread_mutex_unlock(log_lock);
          goto err;
        }
        if (!hot_log) pthread_mutex_unlock(log_lock);
        continue;
      }
      if (!hot_log) pthread_mutex_unlock(log_lock);
      /*
        if we get here, the log was not hot, so we will have to open it
        ourselves. We are sure that the log is still not hot now (a log can get
        from hot to cold, but not from cold to hot). No need for LOCK_log.
      */
#ifdef EXTRA_DEBUG
      if (global_system_variables.log_warnings)
        sql_print_information("next log '%s' is not active",
                              rli->linfo.log_file_name);
#endif
      // open_binlog() will check the magic header
      if ((rli->cur_log_fd=open_binlog(cur_log,rli->linfo.log_file_name,
                                       &errmsg)) <0)
        goto err;
    }
    else
    {
      /*
        Read failed with a non-EOF error.
        TODO: come up with something better to handle this error
      */
      if (hot_log)
        pthread_mutex_unlock(log_lock);
      sql_print_error("Slave SQL thread: I/O error reading \
event(errno: %d  cur_log->error: %d)",
                      my_errno,cur_log->error);
      // set read position to the beginning of the event
      my_b_seek(cur_log,rli->event_relay_log_pos);
      /* otherwise, we have had a partial read */
      errmsg = "Aborting slave SQL thread because of partial event read";
      break;                                    // To end of function
    }
  }
  if (!errmsg && global_system_variables.log_warnings)
  {
    sql_print_information("Error reading relay log event: %s",
                          "slave SQL thread was killed");
    DBUG_RETURN(0);
  }

err:
  if (errmsg)
    sql_print_error("Error reading relay log event: %s", errmsg);
  DBUG_RETURN(0);
}

/*
  Rotate a relay log (this is used only by FLUSH LOGS; the automatic rotation
  because of size is simpler because when we do it we already have all relevant
  locks; here we don't, so this function is mainly taking locks).
  Returns nothing as we cannot catch any error (MYSQL_BIN_LOG::new_file()
  is void).
*/

void rotate_relay_log(Master_info* mi)
{
  DBUG_ENTER("rotate_relay_log");
  Relay_log_info* rli= &mi->rli;

  /* We don't lock rli->run_lock. This would lead to deadlocks. */
  pthread_mutex_lock(&mi->run_lock);

  /*
     We need to test inited because otherwise, new_file() will attempt to lock
     LOCK_log, which may not be inited (if we're not a slave).
  */
  if (!rli->inited)
  {
    DBUG_PRINT("info", ("rli->inited == 0"));
    goto end;
  }

  /* If the relay log is closed, new_file() will do nothing. */
  rli->relay_log.new_file();

  /*
    We harvest now, because otherwise BIN_LOG_HEADER_SIZE will not immediately
    be counted, so imagine a succession of FLUSH LOGS  and assume the slave
    threads are started:
    relay_log_space decreases by the size of the deleted relay log, but does
    not increase, so flush-after-flush we may become negative, which is wrong.
    Even if this will be corrected as soon as a query is replicated on the
    slave (because the I/O thread will then call harvest_bytes_written() which
    will harvest all these BIN_LOG_HEADER_SIZE we forgot), it may give strange
    output in SHOW SLAVE STATUS meanwhile. So we harvest now.
    If the log is closed, then this will just harvest the last writes, probably
    0 as they probably have been harvested.
  */
  rli->relay_log.harvest_bytes_written(&rli->log_space_total);
end:
  pthread_mutex_unlock(&mi->run_lock);
  DBUG_VOID_RETURN;
}


/**
   Detects, based on master's version (as found in the relay log), if master
   has a certain bug.
   @param rli Relay_log_info which tells the master's version
   @param bug_id Number of the bug as found in bugs.mysql.com
   @param report bool report error message, default TRUE

   @param pred Predicate function that will be called with @c param to
   check for the bug. If the function return @c true, the bug is present,
   otherwise, it is not.

   @param param  State passed to @c pred function.

   @return TRUE if master has the bug, FALSE if it does not.
*/
bool rpl_master_has_bug(const Relay_log_info *rli, uint bug_id, bool report,
                        bool (*pred)(const void *), const void *param)
{
  struct st_version_range_for_one_bug {
    uint        bug_id;
    const uchar introduced_in[3]; // first version with bug
    const uchar fixed_in[3];      // first version with fix
  };
  static struct st_version_range_for_one_bug versions_for_all_bugs[]=
  {
    {24432, { 5, 0, 24 }, { 5, 0, 38 } },
    {24432, { 5, 1, 12 }, { 5, 1, 17 } },
    {33029, { 5, 0,  0 }, { 5, 0, 58 } },
    {33029, { 5, 1,  0 }, { 5, 1, 12 } },
    {37426, { 5, 1,  0 }, { 5, 1, 26 } },
  };
  const uchar *master_ver=
    rli->relay_log.description_event_for_exec->server_version_split;

  DBUG_ASSERT(sizeof(rli->relay_log.description_event_for_exec->server_version_split) == 3);

  for (uint i= 0;
       i < sizeof(versions_for_all_bugs)/sizeof(*versions_for_all_bugs);i++)
  {
    const uchar *introduced_in= versions_for_all_bugs[i].introduced_in,
      *fixed_in= versions_for_all_bugs[i].fixed_in;
    if ((versions_for_all_bugs[i].bug_id == bug_id) &&
        (memcmp(introduced_in, master_ver, 3) <= 0) &&
        (memcmp(fixed_in,      master_ver, 3) >  0) &&
        (pred == NULL || (*pred)(param)))
    {
      if (!report)
	return TRUE;
      // a short message for SHOW SLAVE STATUS (message length constraints)
      my_printf_error(ER_UNKNOWN_ERROR, "master may suffer from"
                      " http://bugs.mysql.com/bug.php?id=%u"
                      " so slave stops; check error log on slave"
                      " for more info", MYF(0), bug_id);
      // a verbose message for the error log
      rli->report(ERROR_LEVEL, ER_UNKNOWN_ERROR,
                  "According to the master's version ('%s'),"
                  " it is probable that master suffers from this bug:"
                      " http://bugs.mysql.com/bug.php?id=%u"
                      " and thus replicating the current binary log event"
                      " may make the slave's data become different from the"
                      " master's data."
                      " To take no risk, slave refuses to replicate"
                      " this event and stops."
                      " We recommend that all updates be stopped on the"
                      " master and slave, that the data of both be"
                      " manually synchronized,"
                      " that master's binary logs be deleted,"
                      " that master be upgraded to a version at least"
                      " equal to '%d.%d.%d'. Then replication can be"
                      " restarted.",
                      rli->relay_log.description_event_for_exec->server_version,
                      bug_id,
                      fixed_in[0], fixed_in[1], fixed_in[2]);
      return TRUE;
    }
  }
  return FALSE;
}

/**
   BUG#33029, For all 5.0 up to 5.0.58 exclusive, and 5.1 up to 5.1.12
   exclusive, if one statement in a SP generated AUTO_INCREMENT value
   by the top statement, all statements after it would be considered
   generated AUTO_INCREMENT value by the top statement, and a
   erroneous INSERT_ID value might be associated with these statement,
   which could cause duplicate entry error and stop the slave.

   Detect buggy master to work around.
 */
bool rpl_master_erroneous_autoinc(THD *thd)
{
  if (active_mi && active_mi->rli.sql_thd == thd)
  {
    Relay_log_info *rli= &active_mi->rli;
    DBUG_EXECUTE_IF("simulate_bug33029", return TRUE;);
    return rpl_master_has_bug(rli, 33029, FALSE, NULL, NULL);
  }
  return FALSE;
}

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class I_List_iterator<i_string>;
template class I_List_iterator<i_string_pair>;
#endif

/**
  @} (end of group Replication)
*/

#endif /* HAVE_REPLICATION */
