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


#include "mysql_priv.h"
#include <mysql.h>
#include <myisam.h>
#include "mini_client.h"
#include "slave.h"
#include "sql_repl.h"
#include "repl_failsafe.h"
#include <thr_alarm.h>
#include <my_dir.h>
#include <assert.h>

bool use_slave_mask = 0;
MY_BITMAP slave_error_mask;

typedef bool (*CHECK_KILLED_FUNC)(THD*,void*);

volatile bool slave_sql_running = 0, slave_io_running = 0;
char* slave_load_tmpdir = 0;
MASTER_INFO *active_mi;
HASH replicate_do_table, replicate_ignore_table;
DYNAMIC_ARRAY replicate_wild_do_table, replicate_wild_ignore_table;
bool do_table_inited = 0, ignore_table_inited = 0;
bool wild_do_table_inited = 0, wild_ignore_table_inited = 0;
bool table_rules_on= 0, replicate_same_server_id;
ulonglong relay_log_space_limit = 0;

/*
  When slave thread exits, we need to remember the temporary tables so we
  can re-use them on slave start.

  TODO: move the vars below under MASTER_INFO
*/

int disconnect_slave_event_count = 0, abort_slave_event_count = 0;
int events_till_abort = -1;
static int events_till_disconnect = -1;

typedef enum { SLAVE_THD_IO, SLAVE_THD_SQL} SLAVE_THD_TYPE;

void skip_load_data_infile(NET* net);
static int process_io_rotate(MASTER_INFO* mi, Rotate_log_event* rev);
static int process_io_create_file(MASTER_INFO* mi, Create_file_log_event* cev);
static bool wait_for_relay_log_space(RELAY_LOG_INFO* rli);
static inline bool io_slave_killed(THD* thd,MASTER_INFO* mi);
static inline bool sql_slave_killed(THD* thd,RELAY_LOG_INFO* rli);
static int count_relay_log_space(RELAY_LOG_INFO* rli);
static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type);
static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi);
static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			  bool suppress_warnings);
static int connect_to_master(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			     bool reconnect, bool suppress_warnings);
static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
		      void* thread_killed_arg);
static int request_table_dump(MYSQL* mysql, const char* db, const char* table);
static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name, bool overwrite);
static int check_master_version(MYSQL* mysql, MASTER_INFO* mi);


/*
  Get a bit mask for which threads are running so that we later can
  restart these threads
*/

void init_thread_mask(int* mask,MASTER_INFO* mi,bool inverse)
{
  bool set_io = mi->slave_running, set_sql = mi->rli.slave_running;
  register int tmp_mask=0;
  if (set_io)
    tmp_mask |= SLAVE_IO;
  if (set_sql)
    tmp_mask |= SLAVE_SQL;
  if (inverse)
    tmp_mask^= (SLAVE_IO | SLAVE_SQL);
  *mask = tmp_mask;
}


void lock_slave_threads(MASTER_INFO* mi)
{
  //TODO: see if we can do this without dual mutex
  pthread_mutex_lock(&mi->run_lock);
  pthread_mutex_lock(&mi->rli.run_lock);
}

void unlock_slave_threads(MASTER_INFO* mi)
{
  //TODO: see if we can do this without dual mutex
  pthread_mutex_unlock(&mi->rli.run_lock);
  pthread_mutex_unlock(&mi->run_lock);
}


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
  active_mi= new MASTER_INFO;

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


static void free_table_ent(TABLE_RULE_ENT* e)
{
  my_free((gptr) e, MYF(0));
}

static byte* get_table_key(TABLE_RULE_ENT* e, uint* len,
			   my_bool not_used __attribute__((unused)))
{
  *len = e->key_len;
  return (byte*)e->db;
}


/*
  Open the given relay log

  SYNOPSIS
    init_relay_log_pos()
    rli			Relay information (will be initialized)
    log			Name of relay log file to read from. NULL = First log
    pos			Position in relay log file 
    need_data_lock	Set to 1 if this functions should do mutex locks
    errmsg		Store pointer to error message here

  DESCRIPTION
  - Close old open relay log files.
  - If we are using the same relay log as the running IO-thread, then set
    rli->cur_log to point to the same IO_CACHE entry.
  - If not, open the 'log' binary file.

  TODO
    - check proper initialization of master_log_name/master_log_pos
    - We may always want to delete all logs before 'log'.
      Currently if we are not calling this with 'log' as NULL or the first
      log we will never delete relay logs.
      If we want this we should not set skip_log_purge to 1.

  RETURN VALUES
    0	ok
    1	error.  errmsg is set to point to the error message
*/

int init_relay_log_pos(RELAY_LOG_INFO* rli,const char* log,
		       ulonglong pos, bool need_data_lock,
		       const char** errmsg)
{
  DBUG_ENTER("init_relay_log_pos");

  *errmsg=0;
  pthread_mutex_t *log_lock=rli->relay_log.get_log_lock();
  pthread_mutex_lock(log_lock);
  if (need_data_lock)
    pthread_mutex_lock(&rli->data_lock);
  
  /* Close log file and free buffers if it's already open */
  if (rli->cur_log_fd >= 0)
  {
    end_io_cache(&rli->cache_buf);
    my_close(rli->cur_log_fd, MYF(MY_WME));
    rli->cur_log_fd = -1;
  }
  
  rli->relay_log_pos = pos;

  /*
    Test to see if the previous run was with the skip of purging
    If yes, we do not purge when we restart
  */
  if (rli->relay_log.find_log_pos(&rli->linfo, NullS, 1))
  {
    *errmsg="Could not find first log during relay log initialization";
    goto err;
  }

  if (log)					// If not first log
  {
    if (strcmp(log, rli->linfo.log_file_name))
      rli->skip_log_purge= 1;			// Different name; Don't purge
    if (rli->relay_log.find_log_pos(&rli->linfo, log, 1))
    {
      *errmsg="Could not find target log during relay log initialization";
      goto err;
    }
  }
  strmake(rli->relay_log_name,rli->linfo.log_file_name,
	  sizeof(rli->relay_log_name)-1);
  if (rli->relay_log.is_active(rli->linfo.log_file_name))
  {
    /*
      The IO thread is using this log file.
      In this case, we will use the same IO_CACHE pointer to
      read data as the IO thread is using to write data.
    */
    if (my_b_tell((rli->cur_log=rli->relay_log.get_log_file())) == 0 &&
	check_binlog_magic(rli->cur_log,errmsg))
      goto err;
    rli->cur_log_old_open_count=rli->relay_log.get_open_count();
  }
  else
  {
    /*
      Open the relay log and set rli->cur_log to point at this one
    */
    if ((rli->cur_log_fd=open_binlog(&rli->cache_buf,
				     rli->linfo.log_file_name,errmsg)) < 0)
      goto err;
    rli->cur_log = &rli->cache_buf;
  }
  if (pos >= BIN_LOG_HEADER_SIZE)
    my_b_seek(rli->cur_log,(off_t)pos);

err:
  /*
    If we don't purge, we can't honour relay_log_space_limit ;
    silently discard it
  */
  if (rli->skip_log_purge)
    rli->log_space_limit= 0;
  pthread_cond_broadcast(&rli->data_cond);
  if (need_data_lock)
    pthread_mutex_unlock(&rli->data_lock);

  pthread_mutex_unlock(log_lock);
  DBUG_RETURN ((*errmsg) ? 1 : 0);
}


/* called from get_options() in mysqld.cc on start-up */

void init_slave_skip_errors(const char* arg)
{
  const char *p;
  if (bitmap_init(&slave_error_mask,MAX_SLAVE_ERROR,0))
  {
    fprintf(stderr, "Badly out of memory, please check your system status\n");
    exit(1);
  }
  use_slave_mask = 1;
  for (;isspace(*arg);++arg)
    /* empty */;
  if (!my_casecmp(arg,"all",3))
  {
    bitmap_set_all(&slave_error_mask);
    return;
  }
  for (p= arg ; *p; )
  {
    long err_code;
    if (!(p= str2int(p, 10, 0, LONG_MAX, &err_code)))
      break;
    if (err_code < MAX_SLAVE_ERROR)
       bitmap_set_bit(&slave_error_mask,(uint)err_code);
    while (!isdigit(*p) && *p)
      p++;
  }
}

void st_relay_log_info::inc_pending(ulonglong val)
{
  pending += val;
}

/* TODO: this probably needs to be fixed */
void st_relay_log_info::inc_pos(ulonglong val, ulonglong log_pos, bool skip_lock)
{
  if (!skip_lock)
    pthread_mutex_lock(&data_lock);
  relay_log_pos += val+pending;
  pending = 0;
  if (log_pos)
#if MYSQL_VERSION_ID < 50000
    /*
      If the event was converted from a 3.23 format, get_event_len() has
      grown by 6 bytes (at least for most events, except LOAD DATA INFILE
      which is already a big problem for 3.23->4.0 replication); 6 bytes is
      the difference between the header's size in 4.0 (LOG_EVENT_HEADER_LEN)
      and the header's size in 3.23 (OLD_HEADER_LEN). Note that using
      mi->old_format will not help if the I/O thread has not started yet.
      Yes this is a hack but it's just to make 3.23->4.x replication work;
      3.23->5.0 replication is working much better.
      
      The line "mi->old_format ? : " below should NOT BE MERGED to 5.0 which
      already works. But it SHOULD be merged to 4.1.
    */
    master_log_pos= log_pos + val -
      (mi->old_format ? (LOG_EVENT_HEADER_LEN - OLD_HEADER_LEN) : 0);
#endif
  pthread_cond_broadcast(&data_cond);
  if (!skip_lock)
    pthread_mutex_unlock(&data_lock);
}

/*
  thread safe read of position - not needed if we are in the slave thread,
  but required otherwise as var is a longlong
*/
void st_relay_log_info::read_pos(ulonglong& var)
{
  pthread_mutex_lock(&data_lock);
  var = relay_log_pos;
  pthread_mutex_unlock(&data_lock);
}

void st_relay_log_info::close_temporary_tables()
{
  TABLE *table,*next;

  for (table=save_temporary_tables ; table ; table=next)
  {
    next=table->next;
    /*
      Don't ask for disk deletion. For now, anyway they will be deleted when
      slave restarts, but it is a better intention to not delete them.
    */
    close_temporary(table, 0);
  }
  save_temporary_tables= 0;
  slave_open_temp_tables= 0;
}

/*
  We assume we have a run lock on rli and that both slave thread
  are not running
*/

int purge_relay_logs(RELAY_LOG_INFO* rli, THD *thd, bool just_reset,
		     const char** errmsg)
{
  int error=0;
  DBUG_ENTER("purge_relay_logs");

  /*
    Even if rli->inited==0, we still try to empty rli->master_log_* variables.
    Indeed, rli->inited==0 does not imply that they already are empty.
    It could be that slave's info initialization partly succeeded : 
    for example if relay-log.info existed but *relay-bin*.*
    have been manually removed, init_relay_log_info reads the old 
    relay-log.info and fills rli->master_log_*, then init_relay_log_info
    checks for the existence of the relay log, this fails and
    init_relay_log_info leaves rli->inited to 0.
    In that pathological case, rli->master_log_pos* will be properly reinited
    at the next START SLAVE (as RESET SLAVE or CHANGE
    MASTER, the callers of purge_relay_logs, will delete bogus *.info files
    or replace them with correct files), however if the user does SHOW SLAVE
    STATUS before START SLAVE, he will see old, confusing rli->master_log_*.
    In other words, we reinit rli->master_log_* for SHOW SLAVE STATUS 
    to display fine in any case.
  */

  rli->master_log_name[0]= 0;
  rli->master_log_pos= 0;
  rli->pending= 0;

  if (!rli->inited)
  {
    DBUG_PRINT("info", ("rli->inited == 0"));
    DBUG_RETURN(0);
  }

  DBUG_ASSERT(rli->slave_running == 0);
  DBUG_ASSERT(rli->mi->slave_running == 0);

  rli->slave_skip_counter=0;
  pthread_mutex_lock(&rli->data_lock);
  if (rli->relay_log.reset_logs(thd))
  {
    *errmsg = "Failed during log reset";
    error=1;
    goto err;
  }
  /* Save name of used relay log file */
  strmake(rli->relay_log_name, rli->relay_log.get_log_fname(),
	  sizeof(rli->relay_log_name)-1);
  // Just first log with magic number and nothing else
  rli->log_space_total= BIN_LOG_HEADER_SIZE;
  rli->relay_log_pos=   BIN_LOG_HEADER_SIZE;
  rli->relay_log.reset_bytes_written();
  if (!just_reset)
    error= init_relay_log_pos(rli, rli->relay_log_name, rli->relay_log_pos,
			      0 /* do not need data lock */, errmsg);

err:
#ifndef DBUG_OFF
  char buf[22];
#endif  
  DBUG_PRINT("info",("log_space_total: %s",llstr(rli->log_space_total,buf)));
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(error);
}


int terminate_slave_threads(MASTER_INFO* mi,int thread_mask,bool skip_lock)
{
  if (!mi->inited)
    return 0; /* successfully do nothing */
  int error,force_all = (thread_mask & SLAVE_FORCE_ALL);
  pthread_mutex_t *sql_lock = &mi->rli.run_lock, *io_lock = &mi->run_lock;
  pthread_mutex_t *sql_cond_lock,*io_cond_lock;
  DBUG_ENTER("terminate_slave_threads");

  sql_cond_lock=sql_lock;
  io_cond_lock=io_lock;
  
  if (skip_lock)
  {
    sql_lock = io_lock = 0;
  }
  if ((thread_mask & (SLAVE_IO|SLAVE_FORCE_ALL)) && mi->slave_running)
  {
    DBUG_PRINT("info",("Terminating IO thread"));
    mi->abort_slave=1;
    if ((error=terminate_slave_thread(mi->io_thd,io_lock,
				      io_cond_lock,
				      &mi->stop_cond,
				      &mi->slave_running)) &&
	!force_all)
      DBUG_RETURN(error);
  }
  if ((thread_mask & (SLAVE_SQL|SLAVE_FORCE_ALL)) && mi->rli.slave_running)
  {
    DBUG_PRINT("info",("Terminating SQL thread"));
    DBUG_ASSERT(mi->rli.sql_thd != 0) ;
    mi->rli.abort_slave=1;
    if ((error=terminate_slave_thread(mi->rli.sql_thd,sql_lock,
				      sql_cond_lock,
				      &mi->rli.stop_cond,
				      &mi->rli.slave_running)) &&
	!force_all)
      DBUG_RETURN(error);
  }
  DBUG_RETURN(0);
}


int terminate_slave_thread(THD* thd, pthread_mutex_t* term_lock,
			   pthread_mutex_t *cond_lock,
			   pthread_cond_t* term_cond,
			   volatile bool* slave_running)
{
  if (term_lock)
  {
    pthread_mutex_lock(term_lock);
    if (!*slave_running)
    {
      pthread_mutex_unlock(term_lock);
      return ER_SLAVE_NOT_RUNNING;
    }
  }
  DBUG_ASSERT(thd != 0);
  /*
    Is is criticate to test if the slave is running. Otherwise, we might
    be referening freed memory trying to kick it
  */
  THD_CHECK_SENTRY(thd);

  while (*slave_running)			// Should always be true
  {
    KICK_SLAVE(thd);
    /*
      There is a small chance that slave thread might miss the first
      alarm. To protect againts it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(abstime,2);
    pthread_cond_timedwait(term_cond, cond_lock, &abstime);
  }
  if (term_lock)
    pthread_mutex_unlock(term_lock);
  return 0;
}


int start_slave_thread(pthread_handler h_func, pthread_mutex_t *start_lock,
		       pthread_mutex_t *cond_lock,
		       pthread_cond_t *start_cond,
		       volatile bool *slave_running,
		       volatile ulong *slave_run_id,
		       MASTER_INFO* mi)
{
  pthread_t th;
  ulong start_id;
  DBUG_ASSERT(mi->inited);
  DBUG_ENTER("start_slave_thread");

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
	DBUG_RETURN(ER_SERVER_SHUTDOWN);
    }
  }
  if (start_lock)
    pthread_mutex_unlock(start_lock);
  DBUG_RETURN(0);
}


/*
  SLAVE_FORCE_ALL is not implemented here on purpose since it does not make
  sense to do that for starting a slave - we always care if it actually
  started the threads that were not previously running
*/

int start_slave_threads(bool need_slave_mutex, bool wait_for_start,
			MASTER_INFO* mi, const char* master_info_fname,
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
			     mi);
  if (!error && (thread_mask & SLAVE_SQL))
  {
    error=start_slave_thread(handle_slave_sql,lock_sql,lock_cond_sql,
			     cond_sql,
			     &mi->rli.slave_running, &mi->rli.slave_run_id,
			     mi);
    if (error)
      terminate_slave_threads(mi, thread_mask & SLAVE_IO, 0);
  }
  DBUG_RETURN(error);
}


void init_table_rule_hash(HASH* h, bool* h_inited)
{
  hash_init(h, TABLE_RULE_HASH_SIZE,0,0,
	    (hash_get_key) get_table_key,
	    (hash_free_key) free_table_ent, 0);
  *h_inited = 1;
}

void init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited)
{
  my_init_dynamic_array(a, sizeof(TABLE_RULE_ENT*), TABLE_RULE_ARR_SIZE,
		     TABLE_RULE_ARR_SIZE);
  *a_inited = 1;
}

static TABLE_RULE_ENT* find_wild(DYNAMIC_ARRAY *a, const char* key, int len)
{
  uint i;
  const char* key_end = key + len;
  
  for (i = 0; i < a->elements; i++)
    {
      TABLE_RULE_ENT* e ;
      get_dynamic(a, (gptr)&e, i);
      if (!wild_case_compare(key, key_end, (const char*)e->db,
			    (const char*)(e->db + e->key_len),'\\'))
	return e;
    }
  
  return 0;
}


/*
  Checks whether tables match some (wild_)do_table and (wild_)ignore_table
  rules (for replication)

  SYNOPSIS
    tables_ok()
    thd             thread (SQL slave thread normally)
    tables          list of tables to check

  NOTES
    Note that changing the order of the tables in the list can lead to
    different results. Note also the order of precedence of the do/ignore 
    rules (see code below). For that reason, users should not set conflicting 
    rules because they may get unpredicted results (precedence order is
    explained in the manual).
    If no table of the list is marked "updating" (so far this can only happen
    if the statement is a multi-delete (SQLCOM_DELETE_MULTI) and the "tables"
    is the tables in the FROM): then we always return 0, because there is no
    reason we play this statement on this slave if it updates nothing. In the
    case of SQLCOM_DELETE_MULTI, there will be a second call to tables_ok(),
    with tables having "updating==TRUE" (those after the DELETE), so this
    second call will make the decision (because
    all_tables_not_ok() = !tables_ok(1st_list) && !tables_ok(2nd_list)).

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated                  
*/

int tables_ok(THD* thd, TABLE_LIST* tables)
{
  bool some_tables_updating= 0;
  DBUG_ENTER("tables_ok");

  for (; tables; tables = tables->next)
  {
    char hash_key[2*NAME_LEN+2];
    char *end;
    uint len;

    if (!tables->updating) 
      continue;
    some_tables_updating= 1;
    end= strmov(hash_key, tables->db ? tables->db : thd->db);
    *end++= '.';
    len= (uint) (strmov(end, tables->real_name) - hash_key);
    if (do_table_inited) // if there are any do's
    {
      if (hash_search(&replicate_do_table, (byte*) hash_key, len))
	DBUG_RETURN(1);
    }
    if (ignore_table_inited) // if there are any ignores
    {
      if (hash_search(&replicate_ignore_table, (byte*) hash_key, len))
	DBUG_RETURN(0); 
    }
    if (wild_do_table_inited && find_wild(&replicate_wild_do_table,
					  hash_key, len))
      DBUG_RETURN(1);
    if (wild_ignore_table_inited && find_wild(&replicate_wild_ignore_table,
					      hash_key, len))
      DBUG_RETURN(0);
  }

  /*
    If no table was to be updated, ignore statement (no reason we play it on
    slave, slave is supposed to replicate _changes_ only).
    If no explicit rule found and there was a do list, do not replicate.
    If there was no do list, go ahead
  */
  DBUG_RETURN(some_tables_updating &&
              !do_table_inited && !wild_do_table_inited);
}


/*
  Checks whether a db matches wild_do_table and wild_ignore_table
  rules (for replication)

  SYNOPSIS
    db_ok_with_wild_table()
    db		name of the db to check.
		Is tested with check_db_name() before calling this function.

  NOTES
    Here is the reason for this function.
    We advise users who want to exclude a database 'db1' safely to do it
    with replicate_wild_ignore_table='db1.%' instead of binlog_ignore_db or
    replicate_ignore_db because the two lasts only check for the selected db,
    which won't work in that case:
    USE db2;
    UPDATE db1.t SET ... #this will be replicated and should not
    whereas replicate_wild_ignore_table will work in all cases.
    With replicate_wild_ignore_table, we only check tables. When
    one does 'DROP DATABASE db1', tables are not involved and the
    statement will be replicated, while users could expect it would not (as it
    rougly means 'DROP db1.first_table, DROP db1.second_table...').
    In other words, we want to interpret 'db1.%' as "everything touching db1".
    That is why we want to match 'db1' against 'db1.%' wild table rules.

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated
 */

int db_ok_with_wild_table(const char *db)
{
  char hash_key[NAME_LEN+2];
  char *end;
  int len;
  end= strmov(hash_key, db);
  *end++= '.';
  len= end - hash_key ;
  if (wild_do_table_inited && find_wild(&replicate_wild_do_table,
                                        hash_key, len))
    return 1;
  if (wild_ignore_table_inited && find_wild(&replicate_wild_ignore_table,
                                            hash_key, len))
    return 0;
  
  /*
    If no explicit rule found and there was a do list, do not replicate.
    If there was no do list, go ahead
  */
  return !wild_do_table_inited;
}


int add_table_rule(HASH* h, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  // len is always > 0 because we know the there exists a '.'
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db = (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name = e->db + (dot - table_spec) + 1;
  e->key_len = len;
  memcpy(e->db, table_spec, len);
  (void)hash_insert(h, (byte*)e);
  return 0;
}

int add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db = (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name = e->db + (dot - table_spec) + 1;
  e->key_len = len;
  memcpy(e->db, table_spec, len);
  insert_dynamic(a, (gptr)&e);
  return 0;
}

static void free_string_array(DYNAMIC_ARRAY *a)
{
  uint i;
  for (i = 0; i < a->elements; i++)
    {
      char* p;
      get_dynamic(a, (gptr) &p, i);
      my_free(p, MYF(MY_WME));
    }
  delete_dynamic(a);
}

#ifdef NOT_USED_YET

static int end_slave_on_walk(MASTER_INFO* mi, gptr /*unused*/)
{
  end_master_info(mi);
  return 0;
}
#endif


void end_slave()
{
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
    end_master_info(active_mi);
    if (do_table_inited)
      hash_free(&replicate_do_table);
    if (ignore_table_inited)
      hash_free(&replicate_ignore_table);
    if (wild_do_table_inited)
      free_string_array(&replicate_wild_do_table);
    if (wild_ignore_table_inited)
      free_string_array(&replicate_wild_ignore_table);
    delete active_mi;
    active_mi= 0;
  }
  pthread_mutex_unlock(&LOCK_active_mi);
}


static bool io_slave_killed(THD* thd, MASTER_INFO* mi)
{
  DBUG_ASSERT(mi->io_thd == thd);
  DBUG_ASSERT(mi->slave_running == 1); // tracking buffer overrun
  return mi->abort_slave || abort_loop || thd->killed;
}


static bool sql_slave_killed(THD* thd, RELAY_LOG_INFO* rli)
{
  DBUG_ASSERT(rli->sql_thd == thd);
  DBUG_ASSERT(rli->slave_running == 1);// tracking buffer overrun
  return rli->abort_slave || abort_loop || thd->killed;
}


/*
  Writes an error message to rli->last_slave_error and rli->last_slave_errno
  (which will be displayed by SHOW SLAVE STATUS), and prints it to stderr.

  SYNOPSIS
    slave_print_error()
    rli		
    err_code    The error code
    msg         The error message (usually related to the error code, but can
                contain more information).
    ...         (this is printf-like format, with % symbols in msg)

  RETURN VALUES
    void
*/

void slave_print_error(RELAY_LOG_INFO* rli, int err_code, const char* msg, ...)
{
  va_list args;
  va_start(args,msg);
  my_vsnprintf(rli->last_slave_error,
	       sizeof(rli->last_slave_error), msg, args);
  rli->last_slave_errno = err_code;
  /* If the error string ends with '.', do not add a ',' it would be ugly */
  if (rli->last_slave_error[0] && 
      (*(strend(rli->last_slave_error)-1) == '.'))
    sql_print_error("Slave: %s Error_code: %d", rli->last_slave_error,
                    err_code);
  else
    sql_print_error("Slave: %s, Error_code: %d", rli->last_slave_error,
                    err_code);

}


void skip_load_data_infile(NET* net)
{
  (void)my_net_write(net, "\xfb/dev/null", 10);
  (void)net_flush(net);
  (void)my_net_read(net);			// discard response
  send_ok(net);					// the master expects it
}


const char *rewrite_db(const char* db)
{
  if (replicate_rewrite_db.is_empty() || !db)
    return db;
  I_List_iterator<i_string_pair> it(replicate_rewrite_db);
  i_string_pair* tmp;

  while ((tmp=it++))
  {
    if (!strcmp(tmp->key, db))
      return tmp->val;
  }
  return db;
}

/*
  From other comments and tests in code, it looks like
  sometimes Query_log_event and Load_log_event can have db == 0
  (see rewrite_db() above for example)
  (cases where this happens are unclear; it may be when the master is 3.23).
*/

const char *print_slave_db_safe(const char* db)
{
  return (db ? rewrite_db(db) : "");
}

/*
  Checks whether a db matches some do_db and ignore_db rules
  (for logging or replication)

  SYNOPSIS
    db_ok()
    db              name of the db to check
    do_list         either binlog_do_db or replicate_do_db
    ignore_list     either binlog_ignore_db or replicate_ignore_db

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated                  
*/

int db_ok(const char* db, I_List<i_string> &do_list,
	  I_List<i_string> &ignore_list )
{
  if (do_list.is_empty() && ignore_list.is_empty())
    return 1; // ok to replicate if the user puts no constraints

  /*
    If the user has specified restrictions on which databases to replicate
    and db was not selected, do not replicate.
  */
  if (!db)
    return 0;

  if (!do_list.is_empty()) // if the do's are not empty
  {
    I_List_iterator<i_string> it(do_list);
    i_string* tmp;

    while ((tmp=it++))
    {
      if (!strcmp(tmp->ptr, db))
	return 1; // match
    }
    return 0;
  }
  else // there are some elements in the don't, otherwise we cannot get here
  {
    I_List_iterator<i_string> it(ignore_list);
    i_string* tmp;

    while ((tmp=it++))
    {
      if (!strcmp(tmp->ptr, db))
	return 0; // match
    }
    return 1;
  }
}


static int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
				 const char *default_val)
{
  uint length;
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
      while (((c=my_b_get(f)) != '\n' && c != my_b_EOF));
    }
    return 0;
  }
  else if (default_val)
  {
    strmake(var,  default_val, max_size-1);
    return 0;
  }
  return 1;
}


static int init_intvar_from_file(int* var, IO_CACHE* f, int default_val)
{
  char buf[32];
  
  if (my_b_gets(f, buf, sizeof(buf))) 
  {
    *var = atoi(buf);
    return 0;
  }
  else if (default_val)
  {
    *var = default_val;
    return 0;
  }
  return 1;
}


static int check_master_version(MYSQL* mysql, MASTER_INFO* mi)
{
  const char* errmsg= 0;
  
  /*
    Note the following switch will bug when we have MySQL branch 30 ;)
  */
  switch (*mysql->server_version) {
  case '3':
    mi->old_format = 
      (strncmp(mysql->server_version, "3.23.57", 7) < 0) /* < .57 */ ?
      BINLOG_FORMAT_323_LESS_57 : 
      BINLOG_FORMAT_323_GEQ_57 ;
    break;
  case '4':
    mi->old_format = BINLOG_FORMAT_CURRENT;
    break;
  default:
    /* 5.0 is not supported */
    errmsg = "Master reported an unrecognized MySQL version. Note that 4.0 \
slaves can't replicate a 5.0 or newer master.";
    break;
  }

  if (errmsg)
  {
    sql_print_error(errmsg);
    return 1;
  }
  return 0;
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

static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name, bool overwrite)
{
  ulong packet_len = my_net_read(net); // read create table statement
  char *query, *save_db;
  uint32 save_db_length;
  Vio* save_vio;
  HA_CHECK_OPT check_opt;
  TABLE_LIST tables;
  int error= 1;
  handler *file;
  ulong save_options;
  
  if (packet_len == packet_error)
  {
    send_error(&thd->net, ER_MASTER_NET_READ);
    return 1;
  }
  if (net->read_pos[0] == 255) // error from master
  {
    net->read_pos[packet_len] = 0;
    net_printf(&thd->net, ER_MASTER, net->read_pos + 3);
    return 1;
  }
  thd->command = COM_TABLE_DUMP;
  /* Note that we should not set thd->query until the area is initalized */
  if (!(query = sql_alloc(packet_len + 1)))
  {
    sql_print_error("create_table_from_dump: out of memory");
    net_printf(&thd->net, ER_GET_ERRNO, "Out of memory");
    return 1;
  }
  memcpy(query, net->read_pos, packet_len);
  query[packet_len]= 0;
  thd->query_length= packet_len;
  /*
    We make the following lock in an attempt to ensure that the compiler will
    not rearrange the code so that thd->query is set too soon
  */
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query= query;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->current_tablenr = 0;
  thd->query_error = 0;
  thd->net.no_send_ok = 1;

  bzero((char*) &tables,sizeof(tables));
  tables.db = (char*)db;
  tables.alias= tables.real_name= (char*)table_name;
  /* Drop the table if 'overwrite' is true */
  if (overwrite && mysql_rm_table(thd,&tables,1)) /* drop if exists */
  {
    send_error(&thd->net);
    sql_print_error("create_table_from_dump: failed to drop the table");
    goto err;
  }
  
  /* Create the table. We do not want to log the "create table" statement */
  save_options = thd->options;
  thd->options &= ~(ulong) (OPTION_BIN_LOG);
  thd->proc_info = "Creating table from master dump";
  // save old db in case we are creating in a different database
  save_db = thd->db;
  save_db_length= thd->db_length;
  thd->db = (char*)db;
  DBUG_ASSERT(thd->db);
  thd->db_length= strlen(thd->db);
  mysql_parse(thd, thd->query, packet_len); // run create table
  thd->db = save_db;		// leave things the way the were before
  thd->db_length= save_db_length;
  thd->options = save_options;
  
  if (thd->query_error)
    goto err;			// mysql_parse took care of the error send

  thd->proc_info = "Opening master dump table";
  tables.lock_type = TL_WRITE;
  if (!open_ltable(thd, &tables, TL_WRITE))
  {
    send_error(&thd->net,0,0);			// Send error from open_ltable
    sql_print_error("create_table_from_dump: could not open created table");
    goto err;
  }
  
  file = tables.table->file;
  thd->proc_info = "Reading master dump table data";
  /* Copy the data file */
  if (file->net_read_dump(net))
  {
    net_printf(&thd->net, ER_MASTER_NET_READ);
    sql_print_error("create_table_from_dump: failed in\
 handler::net_read_dump()");
    goto err;
  }

  check_opt.init();
  check_opt.flags|= T_VERY_SILENT | T_CALC_CHECKSUM | T_QUICK;
  thd->proc_info = "Rebuilding the index on master dump table";
  /*
    We do not want repair() to spam us with messages
    just send them to the error log, and report the failure in case of
    problems.
  */
  save_vio = thd->net.vio;
  thd->net.vio = 0;
  /* Rebuild the index file from the copied data file (with REPAIR) */
  error=file->repair(thd,&check_opt) != 0;
  thd->net.vio = save_vio;
  if (error)
    net_printf(&thd->net, ER_INDEX_REBUILD,tables.table->real_name);

err:
  close_thread_tables(thd);
  thd->net.no_send_ok = 0;
  return error; 
}

int fetch_master_table(THD *thd, const char *db_name, const char *table_name,
		       MASTER_INFO *mi, MYSQL *mysql, bool overwrite)
{
  int error= 1;
  const char *errmsg=0;
  bool called_connected= (mysql != NULL);
  DBUG_ENTER("fetch_master_table");
  DBUG_PRINT("enter", ("db_name: '%s'  table_name: '%s'",
		       db_name,table_name));

  if (!called_connected)
  { 
    if (!(mysql = mc_mysql_init(NULL)))
    {
      send_error(&thd->net);			// EOM
      DBUG_RETURN(1);
    }
    if (connect_to_master(thd, mysql, mi))
    {
      net_printf(&thd->net, ER_CONNECT_TO_MASTER, mc_mysql_error(mysql));
      mc_mysql_close(mysql);
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

  if (create_table_from_dump(thd, &mysql->net, db_name,
			    table_name, overwrite))
    goto err; // create_table_from_dump will have send_error already
  error = 0;

 err:
  thd->net.no_send_ok = 0; // Clear up garbage after create_table_from_dump
  if (!called_connected)
    mc_mysql_close(mysql);
  if (errmsg && thd->net.vio)
    send_error(&thd->net, error, errmsg);
  DBUG_RETURN(test(error));			// Return 1 on error
}


void end_master_info(MASTER_INFO* mi)
{
  DBUG_ENTER("end_master_info");

  if (!mi->inited)
    DBUG_VOID_RETURN;
  end_relay_log_info(&mi->rli);
  if (mi->fd >= 0)
  {
    end_io_cache(&mi->file);
    (void)my_close(mi->fd, MYF(MY_WME));
    mi->fd = -1;
  }
  mi->inited = 0;

  DBUG_VOID_RETURN;
}


int init_relay_log_info(RELAY_LOG_INFO* rli, const char* info_fname)
{
  char fname[FN_REFLEN+128];
  int info_fd;
  const char* msg = 0;
  int error = 0;
  DBUG_ENTER("init_relay_log_info");

  if (rli->inited)				// Set if this function called
    DBUG_RETURN(0);
  fn_format(fname, info_fname, mysql_data_home, "", 4+32);
  pthread_mutex_lock(&rli->data_lock);
  info_fd = rli->info_fd;
  rli->pending = 0;
  rli->cur_log_fd = -1;
  rli->slave_skip_counter=0;
  rli->abort_pos_wait=0;
  rli->skip_log_purge=0;
  rli->log_space_limit = relay_log_space_limit;
  rli->log_space_total = 0;

  // TODO: make this work with multi-master
  if (!opt_relay_logname)
  {
    char tmp[FN_REFLEN];
    /*
      TODO: The following should be using fn_format();  We just need to
      first change fn_format() to cut the file name if it's too long.
    */
    strmake(tmp,glob_hostname,FN_REFLEN-5);
    strmov(strcend(tmp,'.'),"-relay-bin");
    opt_relay_logname=my_strdup(tmp,MYF(MY_WME));
  }

  /*
    The relay log will now be opened, as a SEQ_READ_APPEND IO_CACHE. It is
    notable that the last kilobytes of it (8 kB for example) may live in
    memory, not on disk (depending on what the thread using it does). While
    this is efficient, it has a side-effect one must know: 
    The size of the relay log on disk (displayed by 'ls -l' on Unix) can be a
    few kilobytes less than one would expect by doing SHOW SLAVE STATUS; this
    happens when only the IO thread is started (not the SQL thread). The
    "missing" kilobytes are in memory, are preserved during 'STOP SLAVE; START
    SLAVE IO_THREAD', and are flushed to disk when the slave's mysqld stops. So
    this does not cause any bug. Example of how disk size grows by leaps:

     Read_Master_Log_Pos: 7811 -rw-rw----    1 guilhem  qq              4 Jun  5 16:19 gbichot2-relay-bin.002
     ...later...
     Read_Master_Log_Pos: 9744 -rw-rw----    1 guilhem  qq           8192 Jun  5 16:27 gbichot2-relay-bin.002

    See how 4 is less than 7811 and 8192 is less than 9744.

    WARNING: this is risky because the slave can stay like this for a long
    time; then if it has a power failure, master.info says the I/O thread has
    read until 9744 while the relay-log contains only until 8192 (the
    in-memory part from 8192 to 9744 has been lost), so the SQL slave thread
    will miss some events, silently breaking replication.
    Ideally we would like to flush master.info only when we know that the relay
    log has no in-memory tail.
    Note that the above problem may arise only when only the IO thread is
    started, which is unlikely.
  */

  /*
    For the maximum log size, we choose max_relay_log_size if it is
    non-zero, max_binlog_size otherwise. If later the user does SET
    GLOBAL on one of these variables, fix_max_binlog_size and
    fix_max_relay_log_size will reconsider the choice (for example
    if the user changes max_relay_log_size to zero, we have to
    switch to using max_binlog_size for the relay log) and update
    rli->relay_log.max_size (and mysql_bin_log.max_size).
  */

  if (open_log(&rli->relay_log, glob_hostname, opt_relay_logname,
	       "-relay-bin", opt_relaylog_index_name,
	       LOG_BIN, 1 /* read_append cache */,
	       1 /* no auto events */,
               max_relay_log_size ? max_relay_log_size : max_binlog_size))
  {
    pthread_mutex_unlock(&rli->data_lock);
    sql_print_error("Failed in open_log() called from init_relay_log_info()");
    DBUG_RETURN(1);
  }

  /* if file does not exist */
  if (access(fname,F_OK))
  {
    /*
      If someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (info_fd >= 0)
      my_close(info_fd, MYF(MY_WME));
    if ((info_fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0)
    {
      sql_print_error("Failed to create a new relay log info file (\
file '%s', errno %d)", fname, my_errno);
      msg= current_thd->net.last_error;
      goto err;
    }
    if (init_io_cache(&rli->info_file, info_fd, IO_SIZE*2, READ_CACHE, 0L,0,
		      MYF(MY_WME))) 
    {
      sql_print_error("Failed to create a cache on relay log info file '%s'",
		      fname);
      msg= current_thd->net.last_error;
      goto err;
    }

    /* Init relay log with first entry in the relay index file */
    if (init_relay_log_pos(rli,NullS,BIN_LOG_HEADER_SIZE,0 /* no data lock */,
			   &msg))
    {
      sql_print_error("Failed to open the relay log 'FIRST' (relay_log_pos 4)");
      goto err;
    }
    rli->master_log_name[0]= 0;
    rli->master_log_pos= 0;		
    rli->info_fd= info_fd;
  }
  else // file exists
  {
    if (info_fd >= 0)
      reinit_io_cache(&rli->info_file, READ_CACHE, 0L,0,0);
    else 
    {
      int error=0;
      if ((info_fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0)
      {
        sql_print_error("\
Failed to open the existing relay log info file '%s' (errno %d)",
			fname, my_errno);
        error= 1;
      }
      else if (init_io_cache(&rli->info_file, info_fd,
                             IO_SIZE*2, READ_CACHE, 0L, 0, MYF(MY_WME)))
      {
        sql_print_error("Failed to create a cache on relay log info file '%s'",
			fname);
        error= 1;
      }
      if (error)
      {
        if (info_fd >= 0)
          my_close(info_fd, MYF(0));
        rli->info_fd= -1;
        rli->relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT);
        pthread_mutex_unlock(&rli->data_lock);
        DBUG_RETURN(1);
      }
    }
         
    rli->info_fd = info_fd;
    int relay_log_pos, master_log_pos;
    if (init_strvar_from_file(rli->relay_log_name,
			      sizeof(rli->relay_log_name), &rli->info_file,
			      "") ||
       init_intvar_from_file(&relay_log_pos,
			     &rli->info_file, BIN_LOG_HEADER_SIZE) ||
       init_strvar_from_file(rli->master_log_name,
			     sizeof(rli->master_log_name), &rli->info_file,
			     "") ||
       init_intvar_from_file(&master_log_pos, &rli->info_file, 0))
    {
      msg="Error reading slave log configuration";
      goto err;
    }
    rli->relay_log_pos=  relay_log_pos;
    rli->master_log_pos= master_log_pos;

    if (init_relay_log_pos(rli,
			   rli->relay_log_name,
			   rli->relay_log_pos,
			   0 /* no data lock*/,
			   &msg))
    {
      char llbuf[22];
      sql_print_error("Failed to open the relay log '%s' (relay_log_pos %s)",
		      rli->relay_log_name, llstr(rli->relay_log_pos, llbuf));
      goto err;
    }
  }
  DBUG_ASSERT(rli->relay_log_pos >= BIN_LOG_HEADER_SIZE);
  DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->relay_log_pos);
  /*
    Now change the cache from READ to WRITE - must do this
    before flush_relay_log_info
  */
  reinit_io_cache(&rli->info_file, WRITE_CACHE,0L,0,1);
  if ((error= flush_relay_log_info(rli)))
    sql_print_error("Failed to flush relay log info file");
  if (count_relay_log_space(rli))
  {
    msg="Error counting relay log space";
    goto err;
  }
  rli->inited= 1;
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(error);

err:
  sql_print_error(msg);
  end_io_cache(&rli->info_file);
  if (info_fd >= 0)
    my_close(info_fd, MYF(0));
  rli->info_fd= -1;
  rli->relay_log.close(LOG_CLOSE_INDEX | LOG_CLOSE_STOP_EVENT);
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(1);
}


static inline int add_relay_log(RELAY_LOG_INFO* rli,LOG_INFO* linfo)
{
  MY_STAT s;
  DBUG_ENTER("add_relay_log");
  if (!my_stat(linfo->log_file_name,&s,MYF(0)))
  {
    sql_print_error("log %s listed in the index, but failed to stat",
		    linfo->log_file_name);
    DBUG_RETURN(1);
  }
  rli->log_space_total += s.st_size;
#ifndef DBUG_OFF
  char buf[22];
  DBUG_PRINT("info",("log_space_total: %s", llstr(rli->log_space_total,buf)));
#endif  
  DBUG_RETURN(0);
}


static bool wait_for_relay_log_space(RELAY_LOG_INFO* rli)
{
  bool slave_killed=0;
  MASTER_INFO* mi = rli->mi;
  THD* thd = mi->io_thd;

  DBUG_ENTER("wait_for_relay_log_space");
  pthread_mutex_lock(&rli->log_space_lock);
  const char* save_proc_info= thd->enter_cond(&rli->log_space_cond,
                                              &rli->log_space_lock, 
                                              "Waiting for the SQL slave \
thread to free enough relay log space");
  while (rli->log_space_limit < rli->log_space_total &&
	 !(slave_killed=io_slave_killed(thd,mi)) &&
         !rli->ignore_log_space_limit)
    pthread_cond_wait(&rli->log_space_cond, &rli->log_space_lock);
  thd->exit_cond(save_proc_info);
  DBUG_RETURN(slave_killed);
}


static int count_relay_log_space(RELAY_LOG_INFO* rli)
{
  LOG_INFO linfo;
  DBUG_ENTER("count_relay_log_space");
  rli->log_space_total = 0;
  if (rli->relay_log.find_log_pos(&linfo, NullS, 1))
  {
    sql_print_error("Could not find first log while counting relay log space");
    DBUG_RETURN(1);
  }
  do
  {
    if (add_relay_log(rli,&linfo))
      DBUG_RETURN(1);
  } while (!rli->relay_log.find_next_log(&linfo, 1));
  /* 
     As we have counted everything, including what may have written in a
     preceding write, we must reset bytes_written, or we may count some space 
     twice.
  */
  rli->relay_log.reset_bytes_written();
  DBUG_RETURN(0);
}

void init_master_info_with_options(MASTER_INFO* mi)
{
  mi->master_log_name[0] = 0;
  mi->master_log_pos = BIN_LOG_HEADER_SIZE;		// skip magic number
  
  if (master_host)
    strmake(mi->host, master_host, sizeof(mi->host) - 1);
  if (master_user)
    strmake(mi->user, master_user, sizeof(mi->user) - 1);
  if (master_password)
    strmake(mi->password, master_password, HASH_PASSWORD_LENGTH);
  mi->port = master_port;
  mi->connect_retry = master_connect_retry;
}

void clear_last_slave_error(RELAY_LOG_INFO* rli)
{
  //Clear the errors displayed by SHOW SLAVE STATUS
  rli->last_slave_error[0]=0;
  rli->last_slave_errno=0;
}

int init_master_info(MASTER_INFO* mi, const char* master_info_fname,
		     const char* slave_info_fname,
		     bool abort_if_no_master_info_file,
		     int thread_mask)
{
  int fd,error;
  char fname[FN_REFLEN+128];
  DBUG_ENTER("init_master_info");

  if (mi->inited)
  {
    /*
      We have to reset read position of relay-log-bin as we may have
      already been reading from 'hotlog' when the slave was stopped
      last time. If this case pos_in_file would be set and we would
      get a crash when trying to read the signature for the binary
      relay log.
      
      We only rewind the read position if we are starting the SQL
      thread. The handle_slave_sql thread assumes that the read
      position is at the beginning of the file, and will read the
      "signature" and then fast-forward to the last position read.
    */
    if (thread_mask & SLAVE_SQL)
    {
      my_b_seek(mi->rli.cur_log, (my_off_t) 0);
    }
    DBUG_RETURN(0);
  }

  mi->mysql=0;
  mi->file_id=1;
  mi->ignore_stop_event=0;
  fn_format(fname, master_info_fname, mysql_data_home, "", 4+32);

  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */

  pthread_mutex_lock(&mi->data_lock);
  fd = mi->fd;

  /* does master.info exist ? */
  
  if (access(fname,F_OK))
  {
    if (abort_if_no_master_info_file)
    {
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(0);
    }
    /*
      if someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (fd >= 0)
      my_close(fd, MYF(MY_WME));
    if ((fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0 )
    {
      sql_print_error("Failed to create a new master info file (\
file '%s', errno %d)", fname, my_errno);
      goto err;
    }
    if (init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,0,
		      MYF(MY_WME)))
    {
      sql_print_error("Failed to create a cache on master info file (\
file '%s')", fname);
      goto err;
    }

    mi->fd = fd;
    init_master_info_with_options(mi);

  }
  else // file exists
  {
    if (fd >= 0)
      reinit_io_cache(&mi->file, READ_CACHE, 0L,0,0);
    else 
    {
      if ((fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0 )
      {
        sql_print_error("Failed to open the existing master info file (\
file '%s', errno %d)", fname, my_errno);
        goto err;
      }
      if (init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,
                        0, MYF(MY_WME)))
      {
        sql_print_error("Failed to create a cache on master info file (\
file '%s')", fname);
        goto err;
      }
    }

    mi->fd = fd;
    int port, connect_retry, master_log_pos;

    if (init_strvar_from_file(mi->master_log_name,
			      sizeof(mi->master_log_name), &mi->file,
			      "") ||
	init_intvar_from_file(&master_log_pos, &mi->file, 4) ||
	init_strvar_from_file(mi->host, sizeof(mi->host), &mi->file,
			      master_host) ||
	init_strvar_from_file(mi->user, sizeof(mi->user), &mi->file,
			      master_user) || 
	init_strvar_from_file(mi->password, HASH_PASSWORD_LENGTH+1, &mi->file,
			      master_password) ||
	init_intvar_from_file(&port, &mi->file, master_port) ||
	init_intvar_from_file(&connect_retry, &mi->file,
			      master_connect_retry))
    {
      sql_print_error("Error reading master configuration");
      goto err;
    }
    /*
      This has to be handled here as init_intvar_from_file can't handle
      my_off_t types
    */
    mi->master_log_pos= (my_off_t) master_log_pos;
    mi->port= (uint) port;
    mi->connect_retry= (uint) connect_retry;
  }
  DBUG_PRINT("master_info",("log_file_name: %s  position: %ld",
			    mi->master_log_name,
			    (ulong) mi->master_log_pos));

  mi->rli.mi = mi;
  if (init_relay_log_info(&mi->rli, slave_info_fname))
    goto err;

  mi->inited = 1;
  // now change cache READ -> WRITE - must do this before flush_master_info
  reinit_io_cache(&mi->file, WRITE_CACHE,0L,0,1);
  if ((error= test(flush_master_info(mi, 1))))
    sql_print_error("Failed to flush master info file");
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(error);

err:
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&mi->file);
  }
  mi->fd= -1;
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(1);
}


int register_slave_on_master(MYSQL* mysql)
{
  String packet;
  char buf[4];

  if (!report_host)
    return 0;
  
  int4store(buf, server_id);
  packet.append(buf, 4);

  net_store_data(&packet, report_host); 
  if (report_user)
    net_store_data(&packet, report_user);
  else
    packet.append((char)0);
  
  if (report_password)
    net_store_data(&packet, report_password);
  else
    packet.append((char)0);

  int2store(buf, (uint16)report_port);
  packet.append(buf, 2);
  int4store(buf, rpl_recovery_rank);
  packet.append(buf, 4);
  int4store(buf, 0); /* tell the master will fill in master_id */
  packet.append(buf, 4);

  if (mc_simple_command(mysql, COM_REGISTER_SLAVE, (char*)packet.ptr(),
		       packet.length(), 0))
  {
    sql_print_error("Error on COM_REGISTER_SLAVE: %d '%s'",
		    mc_mysql_errno(mysql),
		    mc_mysql_error(mysql));
    return 1;
  }

  return 0;
}

int show_master_info(THD* thd, MASTER_INFO* mi)
{
  // TODO: fix this for multi-master
  DBUG_ENTER("show_master_info");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Master_Host",
						     sizeof(mi->host)));
  field_list.push_back(new Item_empty_string("Master_User",
						     sizeof(mi->user)));
  field_list.push_back(new Item_empty_string("Master_Port", 6));
  field_list.push_back(new Item_empty_string("Connect_retry", 6));
  field_list.push_back(new Item_empty_string("Master_Log_File",
						     FN_REFLEN));
  field_list.push_back(new Item_empty_string("Read_Master_Log_Pos", 12));
  field_list.push_back(new Item_empty_string("Relay_Log_File",
						     FN_REFLEN));
  field_list.push_back(new Item_empty_string("Relay_Log_Pos", 12));
  field_list.push_back(new Item_empty_string("Relay_Master_Log_File",
						     FN_REFLEN));
  field_list.push_back(new Item_empty_string("Slave_IO_Running", 3));
  field_list.push_back(new Item_empty_string("Slave_SQL_Running", 3));
  field_list.push_back(new Item_empty_string("Replicate_do_db", 20));
  field_list.push_back(new Item_empty_string("Replicate_ignore_db", 20));
  field_list.push_back(new Item_empty_string("Last_errno", 4));
  field_list.push_back(new Item_empty_string("Last_error", 20));
  field_list.push_back(new Item_empty_string("Skip_counter", 12));
  field_list.push_back(new Item_empty_string("Exec_master_log_pos", 12));
  field_list.push_back(new Item_empty_string("Relay_log_space", 12));
  if (send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);

  if (mi->host[0])
  {
    DBUG_PRINT("info",("host is set: '%s'", mi->host));
    String *packet= &thd->packet;
    packet->length(0);
  
    pthread_mutex_lock(&mi->data_lock);
    pthread_mutex_lock(&mi->rli.data_lock);
    net_store_data(packet, mi->host);
    net_store_data(packet, mi->user);
    net_store_data(packet, (uint32) mi->port);
    net_store_data(packet, (uint32) mi->connect_retry);
    net_store_data(packet, mi->master_log_name);
    net_store_data(packet, (longlong) mi->master_log_pos);
    net_store_data(packet, mi->rli.relay_log_name +
		   dirname_length(mi->rli.relay_log_name));
    net_store_data(packet, (longlong) mi->rli.relay_log_pos);
    net_store_data(packet, mi->rli.master_log_name);
    net_store_data(packet, mi->slave_running ? "Yes":"No");
    net_store_data(packet, mi->rli.slave_running ? "Yes":"No");
    net_store_data(packet, &replicate_do_db);
    net_store_data(packet, &replicate_ignore_db);
    net_store_data(packet, (uint32)mi->rli.last_slave_errno);
    net_store_data(packet, mi->rli.last_slave_error);
    net_store_data(packet, mi->rli.slave_skip_counter);
    net_store_data(packet, (longlong) mi->rli.master_log_pos);
    net_store_data(packet, (longlong) mi->rli.log_space_total);
    pthread_mutex_unlock(&mi->rli.data_lock);
    pthread_mutex_unlock(&mi->data_lock);
  
    if (my_net_write(&thd->net, (char*)thd->packet.ptr(), packet->length()))
      DBUG_RETURN(-1);
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}


bool flush_master_info(MASTER_INFO* mi, bool flush_relay_log_cache)
{
  IO_CACHE* file = &mi->file;
  char lbuf[22];
  DBUG_ENTER("flush_master_info");
  DBUG_PRINT("enter",("master_pos: %ld", (long) mi->master_log_pos));

  if (flush_relay_log_cache)   /* Comments for this are in MySQL 4.1 */
    flush_io_cache(mi->rli.relay_log.get_log_file());
  my_b_seek(file, 0L);
  my_b_printf(file, "%s\n%s\n%s\n%s\n%s\n%d\n%d\n",
	      mi->master_log_name, llstr(mi->master_log_pos, lbuf),
	      mi->host, mi->user,
	      mi->password, mi->port, mi->connect_retry
	      );
  flush_io_cache(file);
  DBUG_RETURN(0);
}


st_relay_log_info::st_relay_log_info()
  :info_fd(-1), cur_log_fd(-1), master_log_pos(0), save_temporary_tables(0),
   cur_log_old_open_count(0), log_space_total(0), ignore_log_space_limit(0),
   slave_skip_counter(0), abort_pos_wait(0), slave_run_id(0),
   sql_thd(0), last_slave_errno(0), inited(0), abort_slave(0),
   slave_running(0), skip_log_purge(0),
   inside_transaction(0) /* the default is autocommit=1 */
{
  relay_log_name[0] = master_log_name[0] = 0;
  last_slave_error[0]=0;
  

  bzero((char *)&info_file,sizeof(info_file));
  bzero((char *)&cache_buf, sizeof(cache_buf));
  pthread_mutex_init(&run_lock, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&data_lock, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&log_space_lock, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&data_cond, NULL);
  pthread_cond_init(&start_cond, NULL);
  pthread_cond_init(&stop_cond, NULL);
  pthread_cond_init(&log_space_cond, NULL);
  relay_log.init_pthread_objects();
}


st_relay_log_info::~st_relay_log_info()
{
  pthread_mutex_destroy(&run_lock);
  pthread_mutex_destroy(&data_lock);
  pthread_mutex_destroy(&log_space_lock);
  pthread_cond_destroy(&data_cond);
  pthread_cond_destroy(&start_cond);
  pthread_cond_destroy(&stop_cond);
  pthread_cond_destroy(&log_space_cond);
}

/*
  Waits until the SQL thread reaches (has executed up to) the
  log/position or timed out.

  SYNOPSIS
    wait_for_pos()
    thd             client thread that sent SELECT MASTER_POS_WAIT
    log_name        log name to wait for
    log_pos         position to wait for 
    timeout         timeout in seconds before giving up waiting

  NOTES
    timeout is longlong whereas it should be ulong ; but this is
    to catch if the user submitted a negative timeout.

  RETURN VALUES
    -2          improper arguments (log_pos<0)
                or slave not running, or master info changed
                during the function's execution,
                or client thread killed. -2 is translated to NULL by caller
    -1          timed out
    >=0         number of log events the function had to wait
                before reaching the desired log/position
 */

int st_relay_log_info::wait_for_pos(THD* thd, String* log_name,
                                    longlong log_pos,
                                    longlong timeout)
{
  if (!inited)
    return -1;
  int event_count = 0;
  ulong init_abort_pos_wait;
  int error=0;
  struct timespec abstime; // for timeout checking
  set_timespec(abstime,timeout);

  DBUG_ENTER("wait_for_pos");
  DBUG_PRINT("enter",("master_log_name: '%s'  pos: %lu timeout: %ld",
                      master_log_name, (ulong) master_log_pos, 
                      (long) timeout));

  pthread_mutex_lock(&data_lock);
  const char *msg= thd->enter_cond(&data_cond, &data_lock,
                                   "Waiting for the SQL slave thread to "
                                   "advance position");
  /* 
     This function will abort when it notices that some CHANGE MASTER or
     RESET MASTER has changed the master info.
     To catch this, these commands modify abort_pos_wait ; We just monitor
     abort_pos_wait and see if it has changed.
     Why do we have this mechanism instead of simply monitoring slave_running
     in the loop (we do this too), as CHANGE MASTER/RESET SLAVE require that
     the SQL thread be stopped?
     This is becasue if someones does:
     STOP SLAVE;CHANGE MASTER/RESET SLAVE; START SLAVE;
     the change may happen very quickly and we may not notice that
     slave_running briefly switches between 1/0/1.
  */
  init_abort_pos_wait= abort_pos_wait;

  /*
    We'll need to 
    handle all possible log names comparisons (e.g. 999 vs 1000).
    We use ulong for string->number conversion ; this is no 
    stronger limitation than in find_uniq_filename in sql/log.cc
  */
  ulong log_name_extension;
  char log_name_tmp[FN_REFLEN]; //make a char[] from String
  char *end= strmake(log_name_tmp, log_name->ptr(), min(log_name->length(),
							FN_REFLEN-1));
  char *p= fn_ext(log_name_tmp);
  char *p_end;
  if (!*p || log_pos<0)   
  {
    error= -2; //means improper arguments
    goto err;
  }
  // Convert 0-3 to 4
  log_pos= max(log_pos, BIN_LOG_HEADER_SIZE);
  /* p points to '.' */
  log_name_extension= strtoul(++p, &p_end, 10);
  /*
    p_end points to the first invalid character.
    If it equals to p, no digits were found, error.
    If it contains '\0' it means conversion went ok.
  */
  if (p_end==p || *p_end)
  {
    error= -2;
    goto err;
  }    

  /* The "compare and wait" main loop */
  while (!thd->killed &&
         init_abort_pos_wait == abort_pos_wait &&
         slave_running)
  {
    bool pos_reached;
    int cmp_result= 0;
    /*
      master_log_name can be "", if we are just after a fresh replication start
      or after a CHANGE MASTER TO MASTER_HOST/PORT (before we have executed one
      Rotate event from the master) or (rare) if the user is doing a weird
      slave setup (see next paragraph).
      If master_log_name is "", we assume we don't have enough info to do the
      comparison yet, so we just wait until more data. In this case
      master_log_pos is always 0 except if somebody (wrongly) sets this slave
      to be a slave of itself without using --replicate-same-server-id (an
      unsupported configuration which does nothing), then master_log_pos will
      grow and master_log_name will stay "".
    */
    if (*master_log_name)
    {
      char *basename= master_log_name + dirname_length(master_log_name);
      /*
        First compare the parts before the extension.
        Find the dot in the master's log basename,
        and protect against user's input error :
        if the names do not match up to '.' included, return error
      */
      char *q= (char*)(fn_ext(basename)+1);
      if (strncmp(basename, log_name_tmp, (int)(q-basename)))
      {
        error= -2;
        break;
      }
      // Now compare extensions.
      char *q_end;
      ulong master_log_name_extension= strtoul(q, &q_end, 10);
      if (master_log_name_extension < log_name_extension)
        cmp_result= -1 ;
      else
        cmp_result= (master_log_name_extension > log_name_extension) ? 1 : 0 ;

      pos_reached= ((!cmp_result && master_log_pos >= (ulonglong)log_pos) ||
                    cmp_result > 0);
      if (pos_reached || thd->killed)
        break;
    }

    //wait for master update, with optional timeout.
    
    DBUG_PRINT("info",("Waiting for master update"));
    /*
      We are going to pthread_cond_(timed)wait(); if the SQL thread stops it
      will wake us up.
    */
    if (timeout > 0)
    {
      /*
        Note that pthread_cond_timedwait checks for the timeout
        before for the condition ; i.e. it returns ETIMEDOUT 
        if the system time equals or exceeds the time specified by abstime
        before the condition variable is signaled or broadcast, _or_ if
        the absolute time specified by abstime has already passed at the time
        of the call.
        For that reason, pthread_cond_timedwait will do the "timeoutting" job
        even if its condition is always immediately signaled (case of a loaded
        master).
      */
      error=pthread_cond_timedwait(&data_cond, &data_lock, &abstime);
    }
    else
      pthread_cond_wait(&data_cond, &data_lock);
    DBUG_PRINT("info",("Got signal of master update or timed out"));
    if (error == ETIMEDOUT || error == ETIME)
    {
      error= -1;
      break;
    }
    error=0;
    event_count++;
    DBUG_PRINT("info",("Testing if killed or SQL thread not running"));
  }

err:
  thd->exit_cond(msg);
  DBUG_PRINT("exit",("killed: %d  abort: %d  slave_running: %d \
improper_arguments: %d  timed_out: %d",
                     (int) thd->killed,
                     (int) (init_abort_pos_wait != abort_pos_wait),
                     (int) slave_running,
                     (int) (error == -2),
                     (int) (error == -1)));
  if (thd->killed || init_abort_pos_wait != abort_pos_wait ||
      !slave_running) 
  {
    error= -2;
  }
  DBUG_RETURN( error ? error : event_count );
}


static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type)
{
  DBUG_ENTER("init_slave_thread");
  thd->system_thread = (thd_type == SLAVE_THD_SQL) ?
    SYSTEM_THREAD_SLAVE_SQL : SYSTEM_THREAD_SLAVE_IO; 
  thd->bootstrap= 1;
  thd->host_or_ip= "";
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->net.read_timeout = slave_net_timeout;
  thd->master_access= ~0;
  thd->priv_user = 0;
  thd->slave_thread = 1;
  /* 
     It's nonsense to constraint the slave threads with max_join_size; if a
     query succeeded on master, we HAVE to execute it. So set
     OPTION_BIG_SELECTS. Setting max_join_size to HA_POS_ERROR is not enough
     (and it's not needed if we have OPTION_BIG_SELECTS) because an INSERT
     SELECT examining more than 4 billion rows would still fail (yes, because
     when max_join_size is 4G, OPTION_BIG_SELECTS is automatically set, but
     only for client threads.
  */
  thd->options = ((opt_log_slave_updates) ? OPTION_BIN_LOG:0) |
    OPTION_AUTO_IS_NULL | OPTION_BIG_SELECTS;
  thd->client_capabilities = CLIENT_LOCAL_FILES;
  thd->real_id=pthread_self();
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id = thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  if (init_thr_lock() || thd->store_globals())
  {
    thd->cleanup();
    delete thd;
    DBUG_RETURN(-1);
  }

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  if (thd_type == SLAVE_THD_SQL)
    thd->proc_info= "Waiting for the next event in relay log";
  else
    thd->proc_info= "Waiting for master update";
  thd->version=refresh_version;
  thd->set_time();
  DBUG_RETURN(0);
}


static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
		      void* thread_killed_arg)
{
  int nap_time;
  thr_alarm_t alarmed;
  thr_alarm_init(&alarmed);
  time_t start_time= time((time_t*) 0);
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
      return 1;
    start_time=time((time_t*) 0);
  }
  return 0;
}


static int request_dump(MYSQL* mysql, MASTER_INFO* mi,
			bool *suppress_warnings)
{
  char buf[FN_REFLEN + 10];
  int len;
  int binlog_flags = 0; // for now
  char* logname = mi->master_log_name;
  DBUG_ENTER("request_dump");

  // TODO if big log files: Change next to int8store()
  int4store(buf, (longlong) mi->master_log_pos);
  int2store(buf + 4, binlog_flags);
  int4store(buf + 6, server_id);
  len = (uint) strlen(logname);
  memcpy(buf + 10, logname,len);
  if (mc_simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
  {
    /*
      Something went wrong, so we will just reconnect and retry later
      in the future, we should do a better error analysis, but for
      now we just fill up the error log :-)
    */
    if (mc_mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
      *suppress_warnings= 1;			// Suppress reconnect warning
    else
      sql_print_error("Error on COM_BINLOG_DUMP: %d  %s, will retry in %d secs",
		      mc_mysql_errno(mysql), mc_mysql_error(mysql),
		      master_connect_retry);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


static int request_table_dump(MYSQL* mysql, const char* db, const char* table)
{
  char buf[1024];
  char * p = buf;
  uint table_len = (uint) strlen(table);
  uint db_len = (uint) strlen(db);
  if (table_len + db_len > sizeof(buf) - 2)
  {
    sql_print_error("request_table_dump: Buffer overrun");
    return 1;
  } 
  
  *p++ = db_len;
  memcpy(p, db, db_len);
  p += db_len;
  *p++ = table_len;
  memcpy(p, table, table_len);
  
  if (mc_simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
  {
    sql_print_error("request_table_dump: Error sending the table dump \
command");
    return 1;
  }

  return 0;
}


/*
  read one event from the master
  
  SYNOPSIS
    read_event()
    mysql		MySQL connection
    mi			Master connection information
    suppress_warnings	TRUE when a normal net read timeout has caused us to
			try a reconnect.  We do not want to print anything to
			the error log in this case because this a anormal
			event in an idle server.

    RETURN VALUES
    'packet_error'	Error
    number		Length of packet

*/

static ulong read_event(MYSQL* mysql, MASTER_INFO *mi, bool* suppress_warnings)
{
  ulong len;

  *suppress_warnings= 0;
  /*
    my_real_read() will time us out
    We check if we were told to die, and if not, try reading again

    TODO:  Move 'events_till_disconnect' to the MASTER_INFO structure
  */
#ifndef DBUG_OFF
  if (disconnect_slave_event_count && !(events_till_disconnect--))
    return packet_error;      
#endif
  
  len = mc_net_safe_read(mysql);
  if (len == packet_error || (long) len < 1)
  {
    if (mc_mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
    {
      /*
	We are trying a normal reconnect after a read timeout;
	we suppress prints to .err file as long as the reconnect
	happens without problems
      */
      *suppress_warnings= TRUE;
    }
    else
      sql_print_error("Error reading packet from server: %s (\
server_errno=%d)",
		      mc_mysql_error(mysql), mc_mysql_errno(mysql));
    return packet_error;
  }

  if (len == 1)
  {
     sql_print_error("Slave: received 0 length packet from server, apparent\
 master shutdown: %s",
		     mc_mysql_error(mysql));
     return packet_error;
  }
  
  DBUG_PRINT("info",( "len=%u, net->read_pos[4] = %d\n",
		      len, mysql->net.read_pos[4]));
  return len - 1;   
}


int check_expected_error(THD* thd, RELAY_LOG_INFO* rli, int expected_error)
{
  switch (expected_error) {
  case ER_NET_READ_ERROR:
  case ER_NET_ERROR_ON_WRITE:  
  case ER_SERVER_SHUTDOWN:  
  case ER_NEW_ABORTING_CONNECTION:
    return 1;
  default:
    return 0;
  }
}


static int exec_relay_log_event(THD* thd, RELAY_LOG_INFO* rli)
{
  DBUG_ASSERT(rli->sql_thd==thd);
  Log_event * ev = next_event(rli);
  DBUG_ASSERT(rli->sql_thd==thd);
  if (sql_slave_killed(thd,rli))
  {
    delete ev;
    return 1;
  }
  if (ev)
  {
    int type_code = ev->get_type_code();
    int exec_res;
    pthread_mutex_lock(&rli->data_lock);

    /*
      Skip queries originating from this server or number of
      queries specified by the user in slave_skip_counter
      We can't however skip event's that has something to do with the
      log files themselves.
    */

    /*
      In 4.1, we updated queue_event() to add a similar test for
      replicate_same_server_id, because in 4.1 the I/O thread is also filtering
      events based on the server id.
    */
    if ((ev->server_id == (uint32) ::server_id && !replicate_same_server_id) ||
	(rli->slave_skip_counter && type_code != ROTATE_EVENT))
    {
      /* TODO: I/O thread should not even log events with the same server id */
      rli->inc_pos(ev->get_event_len(),
		   type_code != STOP_EVENT ? ev->log_pos : LL(0),
		   1/* skip lock*/);
      flush_relay_log_info(rli);

      /*
	Protect against common user error of setting the counter to 1
	instead of 2 while recovering from an failed auto-increment insert
      */
      if (rli->slave_skip_counter && 
	  !((type_code == INTVAR_EVENT || type_code == STOP_EVENT) &&
	    rli->slave_skip_counter == 1))
        --rli->slave_skip_counter;
      pthread_mutex_unlock(&rli->data_lock);
      delete ev;     
      return 0;					// avoid infinite update loops
    }
    pthread_mutex_unlock(&rli->data_lock);
  
    thd->server_id = ev->server_id; // use the original server id for logging
    thd->set_time();				// time the query
    if (!ev->when)
      ev->when = time(NULL);
    ev->thd = thd;
    exec_res = ev->exec_event(rli);
    DBUG_ASSERT(rli->sql_thd==thd);
    delete ev;
    return exec_res;
  }
  else
  {
    slave_print_error(rli, 0, "\
Could not parse relay log event entry. The possible reasons are: the master's \
binary log is corrupted (you can check this by running 'mysqlbinlog' on the \
binary log), the slave's relay log is corrupted (you can check this by running \
'mysqlbinlog' on the relay log), a network problem, or a bug in the master's \
or slave's MySQL code. If you want to check the master's binary log or slave's \
relay log, you will be able to know their names by issuing 'SHOW SLAVE STATUS' \
on this slave.\
");
    return 1;
  }
}


/* slave I/O thread */
extern "C" pthread_handler_decl(handle_slave_io,arg)
{
  THD *thd; // needs to be first for thread_stack
  MYSQL *mysql;
  MASTER_INFO *mi = (MASTER_INFO*)arg; 
  char llbuff[22];
  uint retry_count;
  
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_io");

#ifndef DBUG_OFF
slave_begin:  
#endif  
  DBUG_ASSERT(mi->inited);
  mysql= NULL ;
  retry_count= 0;

  pthread_mutex_lock(&mi->run_lock);
  /* Inform waiting threads that slave has started */
  mi->slave_run_id++;

#ifndef DBUG_OFF  
  mi->events_till_abort = abort_slave_event_count;
#endif  
  
  thd= new THD; // note that contructor of THD uses DBUG_ !
  THD_CHECK_SENTRY(thd);

  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_IO))
  {
    pthread_cond_broadcast(&mi->start_cond);
    pthread_mutex_unlock(&mi->run_lock);
    sql_print_error("Failed during slave I/O thread initialization");
    goto err;
  }
  mi->io_thd = thd;
  thd->thread_stack = (char*)&thd; // remember where our stack is
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
  
  if (!(mi->mysql = mysql = mc_mysql_init(NULL)))
  {
    sql_print_error("Slave I/O thread: error in mc_mysql_init()");
    goto err;
  }
  

  thd->proc_info = "Connecting to master";
  // we can get killed during safe_connect
  if (!safe_connect(thd, mysql, mi))
    sql_print_error("Slave I/O thread: connected to master '%s@%s:%d',\
  replication started in log '%s' at position %s", mi->user,
		    mi->host, mi->port,
		    IO_RPL_LOG_NAME,
		    llstr(mi->master_log_pos,llbuff));
  else
  {
    sql_print_error("Slave I/O thread killed while connecting to master");
    goto err;
  }

connected:

  thd->slave_net = &mysql->net;
  thd->proc_info = "Checking master version";
  if (check_master_version(mysql, mi))
    goto err;
  if (!mi->old_format)
  {
    /*
      Register ourselves with the master.
      If fails, this is not fatal - we just print the error message and go
      on with life.
    */
    thd->proc_info = "Registering slave on master";
    if (register_slave_on_master(mysql) ||  update_slave_list(mysql, mi))
      goto err;
  }
  
  DBUG_PRINT("info",("Starting reading binary log from master"));
  while (!io_slave_killed(thd,mi))
  {
    bool suppress_warnings= 0;    
    thd->proc_info = "Requesting binlog dump";
    if (request_dump(mysql, mi, &suppress_warnings))
    {
      sql_print_error("Failed on request_dump()");
      if (io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed while requesting master \
dump");
	goto err;
      }
	  
      thd->proc_info= "Waiting to reconnect after a failed binlog dump request";
      mc_end_server(mysql);
      /*
	First time retry immediately, assuming that we can recover
	right away - if first time fails, sleep between re-tries
	hopefuly the admin can fix the problem sometime
      */
      if (retry_count++)
      {
	if (retry_count > master_retry_count)
	  goto err;				// Don't retry forever
	safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
		   (void*)mi);
      }
      if (io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed while retrying master \
dump");
	goto err;
      }

      thd->proc_info = "Reconnecting after a failed binlog dump request";
      if (!suppress_warnings)
	sql_print_error("Slave I/O thread: failed dump request, \
reconnecting to try again, log '%s' at postion %s", IO_RPL_LOG_NAME,
			llstr(mi->master_log_pos,llbuff));
      if (safe_reconnect(thd, mysql, mi, suppress_warnings) ||
	  io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed during or \
after reconnect");
	goto err;
      }

      goto connected;
    }

    while (!io_slave_killed(thd,mi))
    {
      bool suppress_warnings= 0;    
      /* 
         We say "waiting" because read_event() will wait if there's nothing to
         read. But if there's something to read, it will not wait. The important
         thing is to not confuse users by saying "reading" whereas we're in fact
         receiving nothing.
      */
      thd->proc_info = "Waiting for master to send event";
      ulong event_len = read_event(mysql, mi, &suppress_warnings);
      if (io_slave_killed(thd,mi))
      {
	if (global_system_variables.log_warnings)
	  sql_print_error("Slave I/O thread killed while reading event");
	goto err;
      }
	  	  
      if (event_len == packet_error)
      {
	uint mysql_error_number= mc_mysql_errno(mysql);
	if (mysql_error_number == ER_NET_PACKET_TOO_LARGE)
	{
	  sql_print_error("\
Log entry on master is longer than max_allowed_packet (%ld) on \
slave. If the entry is correct, restart the server with a higher value of \
max_allowed_packet",
			  thd->variables.max_allowed_packet);
	  goto err;
	}
	if (mysql_error_number == ER_MASTER_FATAL_ERROR_READING_BINLOG)
	{
	  sql_print_error(ER(mysql_error_number), mysql_error_number,
			  mc_mysql_error(mysql));
	  goto err;
	}
	thd->proc_info = "Waiting to reconnect after a failed master event \
read";
	mc_end_server(mysql);
	if (retry_count++)
	{
	  if (retry_count > master_retry_count)
	    goto err;				// Don't retry forever
	  safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
		     (void*) mi);
	}	    
	if (io_slave_killed(thd,mi))
	{
	  if (global_system_variables.log_warnings)
	    sql_print_error("Slave I/O thread killed while waiting to \
reconnect after a failed read");
	  goto err;
	}
	thd->proc_info = "Reconnecting after a failed master event read";
	if (!suppress_warnings)
	  sql_print_error("Slave I/O thread: Failed reading log event, \
reconnecting to retry, log '%s' position %s", IO_RPL_LOG_NAME,
			  llstr(mi->master_log_pos, llbuff));
	if (safe_reconnect(thd, mysql, mi, suppress_warnings) ||
	    io_slave_killed(thd,mi))
	{
	  if (global_system_variables.log_warnings)
	    sql_print_error("Slave I/O thread killed during or after a \
reconnect done to recover from failed read");
	  goto err;
	}
	goto connected;
      } // if (event_len == packet_error)
	  
      retry_count=0;			// ok event, reset retry counter
      thd->proc_info = "Queueing master event to the relay log";
      if (queue_event(mi,(const char*)mysql->net.read_pos + 1,
		      event_len))
      {
	sql_print_error("Slave I/O thread could not queue event from master");
	goto err;
      }
      flush_master_info(mi, 1);
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
                            llstr(mi->rli.log_space_limit,llbuf1),
                            llstr(mi->rli.log_space_total,llbuf2),
                            (int) mi->rli.ignore_log_space_limit)); 
      }
#endif

      if (mi->rli.log_space_limit && mi->rli.log_space_limit <
	  mi->rli.log_space_total &&
          !mi->rli.ignore_log_space_limit)
	if (wait_for_relay_log_space(&mi->rli))
	{
	  sql_print_error("Slave I/O thread aborted while waiting for relay \
log space");
	  goto err;
	}
      // TODO: check debugging abort code
#ifndef DBUG_OFF
      if (abort_slave_event_count && !--events_till_abort)
      {
	sql_print_error("Slave I/O thread: debugging abort");
	goto err;
      }
#endif
    } 
  }

  // error = 0;
err:
  // print the current replication position
  sql_print_error("Slave I/O thread exiting, read up to log '%s', position %s",
		  IO_RPL_LOG_NAME, llstr(mi->master_log_pos,llbuff));
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query = thd->db = 0; // extra safety
  thd->query_length= thd->db_length= 0;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  if (mysql)
  {
    mc_mysql_close(mysql);
    mi->mysql=0;
  }
  thd->proc_info = "Waiting for slave mutex on exit";
  pthread_mutex_lock(&mi->run_lock);
  mi->slave_running = 0;
  mi->io_thd = 0;
  // TODO: make rpl_status part of MASTER_INFO
  change_rpl_status(RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE);
  mi->abort_slave = 0; // TODO: check if this is needed
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because net.vio is 0
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_cond_broadcast(&mi->stop_cond);	// tell the world we are done
  pthread_mutex_unlock(&mi->run_lock);
#ifndef DBUG_OFF
  if (abort_slave_event_count && !events_till_abort)
    goto slave_begin;
#endif  
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}


/* slave SQL logic thread */

extern "C" pthread_handler_decl(handle_slave_sql,arg)
{
  THD *thd;			/* needs to be first for thread_stack */
  char llbuff[22],llbuff1[22];
  RELAY_LOG_INFO* rli = &((MASTER_INFO*)arg)->rli; 
  const char *errmsg;

  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  DBUG_ENTER("handle_slave_sql");

#ifndef DBUG_OFF
slave_begin:  
#endif  

  DBUG_ASSERT(rli->inited);
  pthread_mutex_lock(&rli->run_lock);
  DBUG_ASSERT(!rli->slave_running);
  errmsg= 0;
#ifndef DBUG_OFF  
  rli->events_till_abort = abort_slave_event_count;
#endif  

  thd = new THD; // note that contructor of THD uses DBUG_ !
  THD_CHECK_SENTRY(thd);
  /* Inform waiting threads that slave has started */
  rli->slave_run_id++;

  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_SQL))
  {
    /*
      TODO: this is currently broken - slave start and change master
      will be stuck if we fail here
    */
    pthread_cond_broadcast(&rli->start_cond);
    pthread_mutex_unlock(&rli->run_lock);
    sql_print_error("Failed during slave thread initialization");
    goto err;
  }
  rli->sql_thd= thd;
  thd->temporary_tables = rli->save_temporary_tables; // restore temp tables
  thd->thread_stack = (char*)&thd; // remember where our stack is
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  rli->slave_running = 1;
  rli->abort_slave = 0;
  pthread_mutex_unlock(&rli->run_lock);
  pthread_cond_broadcast(&rli->start_cond);
  // This should always be set to 0 when the slave thread is started
  rli->pending = 0;
  /*
    Reset errors for a clean start (otherwise, if the master is idle, the SQL
    thread may execute no Query_log_event, so the error will remain even
    though there's no problem anymore).
  */
  clear_last_slave_error(rli);

  //tell the I/O thread to take relay_log_space_limit into account from now on
  pthread_mutex_lock(&rli->log_space_lock);
  rli->ignore_log_space_limit= 0;
  pthread_mutex_unlock(&rli->log_space_lock);

  if (init_relay_log_pos(rli,
			 rli->relay_log_name,
			 rli->relay_log_pos,
			 1 /*need data lock*/, &errmsg))
  {
    sql_print_error("Error initializing relay log position: %s",
		    errmsg);
    goto err;
  }
  THD_CHECK_SENTRY(thd);
  DBUG_ASSERT(rli->relay_log_pos >= BIN_LOG_HEADER_SIZE);
  DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->relay_log_pos);
  DBUG_ASSERT(rli->sql_thd == thd);

  DBUG_PRINT("master_info",("log_file_name: %s  position: %s",
			    rli->master_log_name,
			    llstr(rli->master_log_pos,llbuff)));
  if (global_system_variables.log_warnings)
    sql_print_error("Slave SQL thread initialized, starting replication in \
log '%s' at position %s, relay log '%s' position: %s", RPL_LOG_NAME,
		    llstr(rli->master_log_pos,llbuff),rli->relay_log_name,
		    llstr(rli->relay_log_pos,llbuff1));

  /* Read queries from the IO/THREAD until this thread is killed */

  while (!sql_slave_killed(thd,rli))
  {
    thd->proc_info = "Reading event from the relay log"; 
    DBUG_ASSERT(rli->sql_thd == thd);
    THD_CHECK_SENTRY(thd);
    if (exec_relay_log_event(thd,rli))
    {
      // do not scare the user if SQL thread was simply killed or stopped
      if (!sql_slave_killed(thd,rli))
        sql_print_error("\
Error running query, slave SQL thread aborted. Fix the problem, and restart \
the slave SQL thread with \"SLAVE START\". We stopped at log \
'%s' position %s",
		      RPL_LOG_NAME, llstr(rli->master_log_pos, llbuff));
      goto err;
    }
  }

  /* Thread stopped. Print the current replication position to the log */
  sql_print_error("Slave SQL thread exiting, replication stopped in log \
 '%s' at position %s",
		  RPL_LOG_NAME, llstr(rli->master_log_pos,llbuff));

 err:
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query = thd->db = 0; // extra safety
  thd->query_length= thd->db_length= 0;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->proc_info = "Waiting for slave mutex on exit";
  pthread_mutex_lock(&rli->run_lock);
  /* We need data_lock, at least to wake up any waiting master_pos_wait() */
  pthread_mutex_lock(&rli->data_lock);
  DBUG_ASSERT(rli->slave_running == 1); // tracking buffer overrun
  /* When master_pos_wait() wakes up it will check this and terminate */
  rli->slave_running= 0; 
  /* 
     Going out of the transaction. Necessary to mark it, in case the user
     restarts replication from a non-transactional statement (with CHANGE
     MASTER).
  */
  rli->inside_transaction= 0;
  /* Wake up master_pos_wait() */
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_PRINT("info",("Signaling possibly waiting master_pos_wait() functions"));
  pthread_cond_broadcast(&rli->data_cond);
  rli->ignore_log_space_limit= 0; /* don't need any lock */
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
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_cond_broadcast(&rli->stop_cond);
  // tell the world we are done
  pthread_mutex_unlock(&rli->run_lock);
#ifndef DBUG_OFF // TODO: reconsider the code below
  if (abort_slave_event_count && !rli->events_till_abort)
    goto slave_begin;
#endif  
  my_thread_end(); // clean-up before broadcasting termination
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}


static int process_io_create_file(MASTER_INFO* mi, Create_file_log_event* cev)
{
  int error = 1;
  ulong num_bytes;
  bool cev_not_written;
  THD* thd;
  NET* net = &mi->mysql->net;
  DBUG_ENTER("process_io_create_file");

  if (unlikely(!cev->is_valid()))
    DBUG_RETURN(1);
  /*
    TODO: fix to honor table rules, not only db rules
  */
  if (!db_ok(cev->db, replicate_do_db, replicate_ignore_db))
  {
    skip_load_data_infile(net);
    DBUG_RETURN(0);
  }
  DBUG_ASSERT(cev->inited_from_old);
  thd = mi->io_thd;
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
       send_ok(net); /* 3.23 master wants it */
       /*
         If we wrote Create_file_log_event, then we need to write
         Execute_load_log_event. If we did not write Create_file_log_event,
         then this is an empty file and we can just do as if the LOAD DATA
         INFILE had not existed, i.e. write nothing.
       */
       if (unlikely(cev_not_written))
         break;
       Execute_load_log_event xev(thd,0,0);
       xev.log_pos = mi->master_log_pos;
       if (unlikely(mi->rli.relay_log.append(&xev)))
       {
         sql_print_error("Slave I/O: error writing Exec_load event to \
relay log");
         goto err;
       }
       mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
       break;
      }
      if (unlikely(cev_not_written))
      {
       cev->block = (char*)net->read_pos;
       cev->block_len = num_bytes;
       cev->log_pos = mi->master_log_pos;
       if (unlikely(mi->rli.relay_log.append(cev)))
       {
         sql_print_error("Slave I/O: error writing Create_file event to \
relay log");
         goto err;
       }
       cev_not_written=0;
       mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
      }
      else
      {
       aev.block = (char*)net->read_pos;
       aev.block_len = num_bytes;
       aev.log_pos = mi->master_log_pos;
       if (unlikely(mi->rli.relay_log.append(&aev)))
       {
         sql_print_error("Slave I/O: error writing Append_block event to \
relay log");
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
    mi			master_info for the slave
    rev			The rotate log event read from the binary log

  DESCRIPTION
    Updates the master info and relay data with the place in the next binary
    log where we should start reading.

  NOTES
    We assume we already locked mi->data_lock

  RETURN VALUES
    0		ok
    1	        Log event is illegal
*/

static int process_io_rotate(MASTER_INFO *mi, Rotate_log_event *rev)
{
  int return_val= 1;
  DBUG_ENTER("process_io_rotate");
  safe_mutex_assert_owner(&mi->data_lock);

  if (unlikely(!rev->is_valid()))
    DBUG_RETURN(1);

  memcpy(mi->master_log_name, rev->new_log_ident, rev->ident_len+1);
  mi->master_log_pos= rev->pos;
  DBUG_PRINT("info", ("master_log_pos: '%s' %d",
		      mi->master_log_name, (ulong) mi->master_log_pos));
#ifndef DBUG_OFF
  /*
    If we do not do this, we will be getting the first
    rotate event forever, so we need to not disconnect after one.
  */
  if (disconnect_slave_event_count)
    events_till_disconnect++;
#endif
  DBUG_RETURN(0);
}

/*
  TODO: 
    Test this code before release - it has to be tested on a separate
    setup with 3.23 master 
*/

static int queue_old_event(MASTER_INFO *mi, const char *buf,
			   ulong event_len)
{
  const char *errmsg = 0;
  ulong inc_pos;
  bool ignore_event= 0;
  char *tmp_buf = 0;
  RELAY_LOG_INFO *rli= &mi->rli;
  DBUG_ENTER("queue_old_event");

  /*
    If we get Load event, we need to pass a non-reusable buffer
    to read_log_event, so we do a trick
  */
  if (buf[EVENT_TYPE_OFFSET] == LOAD_EVENT)
  {
    if (unlikely(!(tmp_buf=(char*)my_malloc(event_len+1,MYF(MY_WME)))))
    {
      sql_print_error("Slave I/O: out of memory for Load event");
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
					    1 /*old format*/ );
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
		    errmsg);
    my_free((char*) tmp_buf, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
  pthread_mutex_lock(&mi->data_lock);
  ev->log_pos = mi->master_log_pos;
  switch (ev->get_type_code()) {
  case STOP_EVENT:
    ignore_event= mi->ignore_stop_event;
    mi->ignore_stop_event=0;
    inc_pos= event_len;
    break;
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    mi->ignore_stop_event=1;
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
    DBUG_ASSERT(tmp_buf);
    int error = process_io_create_file(mi,(Create_file_log_event*)ev);
    delete ev;
    /*
      We had incremented event_len, but now when it is used to calculate the
      position in the master's log, we must use the original value.
    */
    mi->master_log_pos += --event_len;
    DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
    pthread_mutex_unlock(&mi->data_lock);
    my_free((char*)tmp_buf, MYF(0));
    DBUG_RETURN(error);
  }
  default:
    mi->ignore_stop_event=0;
    inc_pos= event_len;
    break;
  }
  if (likely(!ignore_event))
  {
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
  DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(0);
}

/*
  TODO: verify the issue with stop events, see if we need them at all
  in the relay log
*/

int queue_event(MASTER_INFO* mi,const char* buf, ulong event_len)
{
  int error= 0;
  ulong inc_pos;
  bool ignore_event= 0;
  RELAY_LOG_INFO *rli= &mi->rli;
  DBUG_ENTER("queue_event");

  if (mi->old_format)
    DBUG_RETURN(queue_old_event(mi,buf,event_len));

  pthread_mutex_lock(&mi->data_lock);

  /*
    TODO: figure out if other events in addition to Rotate
    require special processing
  */
  switch (buf[EVENT_TYPE_OFFSET]) {
  case STOP_EVENT:
    ignore_event= mi->ignore_stop_event;
    mi->ignore_stop_event= 0;
    inc_pos= event_len;
    break;
  case ROTATE_EVENT:
  {
    Rotate_log_event rev(buf,event_len,0);
    if (unlikely(process_io_rotate(mi,&rev)))
    {
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(1);
    }
    mi->ignore_stop_event= 1;
    inc_pos= 0;
    break;
  }
  default:
    mi->ignore_stop_event= 0;
    inc_pos= event_len;
    break;
  }
  
  if (likely(!ignore_event &&
	     !(error= rli->relay_log.appendv(buf,event_len,0))))
  {
    mi->master_log_pos+= inc_pos;
    DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
    rli->relay_log.harvest_bytes_written(&rli->log_space_total);
  }
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(error);
}


void end_relay_log_info(RELAY_LOG_INFO* rli)
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
  /*
    Delete the slave's temporary tables from memory.
    In the future there will be other actions than this, to ensure persistance
    of slave's temp tables after shutdown.
  */
  rli->close_temporary_tables();
  DBUG_VOID_RETURN;
}

/* try to connect until successful or slave killed */
static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi)
{
  return connect_to_master(thd, mysql, mi, 0, 0);
}


/*
  Try to connect until successful or slave killed or we have retried
  master_retry_count times
*/

static int connect_to_master(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			     bool reconnect, bool suppress_warnings)
{
  int slave_was_killed;
  int last_errno= -2;				// impossible error
  ulong err_count=0;
  char llbuff[22];
  DBUG_ENTER("connect_to_master");

#ifndef DBUG_OFF
  events_till_disconnect = disconnect_slave_event_count;
#endif
  uint client_flag=0;
  if (opt_slave_compressed_protocol)
    client_flag=CLIENT_COMPRESS;		/* We will use compression */

  while (!(slave_was_killed = io_slave_killed(thd,mi)) &&
	 (reconnect ? mc_mysql_reconnect(mysql) != 0:
	  !mc_mysql_connect(mysql, mi->host, mi->user, mi->password, 0,
			    mi->port, 0, client_flag,
			    thd->variables.net_read_timeout)))
  {
    /* Don't repeat last error */
    if (mc_mysql_errno(mysql) != last_errno)
    {
      last_errno=mc_mysql_errno(mysql);
      suppress_warnings= 0;
      sql_print_error("Slave I/O thread: error %s to master \
'%s@%s:%d': \
Error: '%s'  errno: %d  retry-time: %d  retries: %d",
		      (reconnect ? "reconnecting" : "connecting"),
		      mi->user,mi->host,mi->port,
		      mc_mysql_error(mysql), last_errno,
		      mi->connect_retry,
		      master_retry_count);
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
    if (reconnect)
    { 
      if (!suppress_warnings && global_system_variables.log_warnings)
	sql_print_error("Slave: connected to master '%s@%s:%d',\
replication resumed in log '%s' at position %s", mi->user,
			mi->host, mi->port,
			IO_RPL_LOG_NAME,
			llstr(mi->master_log_pos,llbuff));
    }
    else
    {
      change_rpl_status(RPL_IDLE_SLAVE,RPL_ACTIVE_SLAVE);
      mysql_log.write(thd, COM_CONNECT_OUT, "%s@%s:%d",
		      mi->user, mi->host, mi->port);
    }
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->set_active_vio(mysql->net.vio);
#endif      
  }
  DBUG_PRINT("exit",("slave_was_killed: %d", slave_was_killed));
  DBUG_RETURN(slave_was_killed);
}


/*
  Try to connect until successful or slave killed or we have retried
  master_retry_count times
*/

static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			  bool suppress_warnings)
{
  return connect_to_master(thd, mysql, mi, 1, suppress_warnings);
}


/*
  Store the file and position where the execute-slave thread are in the
  relay log.

  SYNOPSIS
    flush_relay_log_info()
    rli			Relay log information

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
    0	ok
    1	write error
*/

bool flush_relay_log_info(RELAY_LOG_INFO* rli)
{
  bool error=0;
  IO_CACHE *file = &rli->info_file;
  char buff[FN_REFLEN*2+22*2+4], *pos;

  /* sql_thd is not set when calling from init_slave() */
  if ((rli->sql_thd && rli->sql_thd->options & OPTION_BEGIN))
    return 0;					// Wait for COMMIT

  my_b_seek(file, 0L);
  pos=strmov(buff, rli->relay_log_name);
  *pos++='\n';
  pos=longlong2str(rli->relay_log_pos, pos, 10);
  *pos++='\n';
  pos=strmov(pos, rli->master_log_name);
  *pos++='\n';
  pos=longlong2str(rli->master_log_pos, pos, 10);
  *pos='\n';
  if (my_b_write(file, (byte*) buff, (ulong) (pos-buff)+1))
    error=1;
  if (flush_io_cache(file))
    error=1;
  /* Flushing the relay log is done by the slave I/O thread */
  return error;
}


/*
  This function is called when we notice that the current "hot" log
  got rotated under our feet.
*/

static IO_CACHE *reopen_relay_log(RELAY_LOG_INFO *rli, const char **errmsg)
{
  DBUG_ASSERT(rli->cur_log != &rli->cache_buf);
  DBUG_ASSERT(rli->cur_log_fd == -1);
  DBUG_ENTER("reopen_relay_log");

  IO_CACHE *cur_log = rli->cur_log=&rli->cache_buf;
  if ((rli->cur_log_fd=open_binlog(cur_log,rli->relay_log_name,
				   errmsg)) <0)
    DBUG_RETURN(0);
  /*
    We want to start exactly where we was before:
    relay_log_pos	Current log pos
    pending		Number of bytes already processed from the event
  */
  my_b_seek(cur_log,rli->relay_log_pos + rli->pending);
  DBUG_RETURN(cur_log);
}


Log_event* next_event(RELAY_LOG_INFO* rli)
{
  Log_event* ev;
  IO_CACHE* cur_log = rli->cur_log;
  pthread_mutex_t *log_lock = rli->relay_log.get_log_lock();
  const char* errmsg=0;
  THD* thd = rli->sql_thd;
  DBUG_ENTER("next_event");
  DBUG_ASSERT(thd != 0);

  /*
    For most operations we need to protect rli members with data_lock,
    so we will hold it for the most of the loop below
    However, we will release it whenever it is worth the hassle, 
    and in the cases when we go into a pthread_cond_wait() with the
    non-data_lock mutex
  */
  pthread_mutex_lock(&rli->data_lock);
  
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
	if (!cur_log)				// No more log files
	  goto err;
	hot_log=0;				// Using old binary log
      }
    }
#ifndef DBUG_OFF
    {
      DBUG_ASSERT(my_b_tell(cur_log) >= BIN_LOG_HEADER_SIZE);
      /* The next assertion sometimes (very rarely) fails, let's try to track it */
      char llbuf1[22], llbuf2[22];
      /* Merging man, please be careful with this; in 4.1, the assertion below is
         replaced by
         DBUG_ASSERT(my_b_tell(cur_log) == rli->event_relay_log_pos);
         so you should not merge blindly (fortunately it won't build then), and
         instead modify the merged code. Thanks. */
      DBUG_PRINT("info", ("Before assert, my_b_tell(cur_log)=%s \
rli->relay_log_pos=%s rli->pending=%lu",
                          llstr(my_b_tell(cur_log),llbuf1), 
                          llstr(rli->relay_log_pos,llbuf2),
                          rli->pending));
      DBUG_ASSERT(my_b_tell(cur_log) == rli->relay_log_pos + rli->pending);
    }
#endif

    /*
      Relay log is always in new format - if the master is 3.23, the
      I/O thread will convert the format for us
    */
    if ((ev=Log_event::read_log_event(cur_log,0,(bool)0 /* new format */)))
    {
      DBUG_ASSERT(thd==rli->sql_thd);
      if (hot_log)
	pthread_mutex_unlock(log_lock);
      pthread_mutex_unlock(&rli->data_lock);
      DBUG_RETURN(ev);
    }
    DBUG_ASSERT(thd==rli->sql_thd);
    if (opt_reckless_slave)			// For mysql-test
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
	DBUG_ASSERT(rli->relay_log.get_open_count() == rli->cur_log_old_open_count);
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
          If the I/O thread is blocked, unblock it.
          Ok to broadcast after unlock, because the mutex is only destroyed in
          ~st_relay_log_info(), i.e. when rli is destroyed, and rli will not be
          destroyed before we exit the present function.
        */
        pthread_mutex_unlock(&rli->log_space_lock);
        pthread_cond_broadcast(&rli->log_space_cond);
        // Note that wait_for_update unlocks lock_log !
        rli->relay_log.wait_for_update(rli->sql_thd, 1);
        // re-acquire data lock since we released it earlier
        pthread_mutex_lock(&rli->data_lock);
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
	
      /*
	TODO: make skip_log_purge a start-up option. At this point this
	is not critical priority
      */
      if (!rli->skip_log_purge)
      {
	// purge_first_log will properly set up relay log coordinates in rli
	if (rli->relay_log.purge_first_log(rli))
	{
	  errmsg = "Error purging processed log";
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
	rli->relay_log_pos = BIN_LOG_HEADER_SIZE;
	rli->pending=0;
	strmake(rli->relay_log_name,rli->linfo.log_file_name,
		sizeof(rli->relay_log_name)-1);
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
	sql_print_error("next log '%s' is currently active",
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
      sql_print_error("next log '%s' is not active",
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
      my_b_seek(cur_log,rli->relay_log_pos+rli->pending);
      /* otherwise, we have had a partial read */
      errmsg = "Aborting slave SQL thread because of partial event read";
      break;					// To end of function
    }
  }
  if (!errmsg && global_system_variables.log_warnings)
    errmsg = "slave SQL thread was killed";

err:
  pthread_mutex_unlock(&rli->data_lock);
  if (errmsg)
    sql_print_error("Error reading relay log event: %s", errmsg);
  DBUG_RETURN(0);
}

/*
  Rotate a relay log (this is used only by FLUSH LOGS; the automatic rotation
  because of size is simpler because when we do it we already have all relevant
  locks; here we don't, so this function is mainly taking locks). 
  Returns nothing as we cannot catch any error (MYSQL_LOG::new_file() is void).
*/

void rotate_relay_log(MASTER_INFO* mi)
{
  DBUG_ENTER("rotate_relay_log");
  RELAY_LOG_INFO* rli= &mi->rli;

  lock_slave_threads(mi);
  pthread_mutex_lock(&rli->data_lock);
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
  rli->relay_log.new_file(1);

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
  pthread_mutex_unlock(&rli->data_lock);
  unlock_slave_threads(mi);
  DBUG_VOID_RETURN;
}


#ifdef __GNUC__
template class I_List_iterator<i_string>;
template class I_List_iterator<i_string_pair>;
#endif
