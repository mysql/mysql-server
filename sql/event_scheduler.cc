/* Copyright (C) 2004-2006 MySQL AB

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
#include "events.h"
#include "event_data_objects.h"
#include "event_scheduler.h"
#include "event_queue.h"

#ifdef __GNUC__
#if __GNUC__ >= 2
#define SCHED_FUNC __FUNCTION__
#endif
#else
#define SCHED_FUNC "<unknown>"
#endif

#define LOCK_DATA()       lock_data(SCHED_FUNC, __LINE__)
#define UNLOCK_DATA()     unlock_data(SCHED_FUNC, __LINE__)
#define COND_STATE_WAIT(mythd, abstime, msg) \
        cond_wait(mythd, abstime, msg, SCHED_FUNC, __LINE__)

extern pthread_attr_t connection_attrib;

static
const LEX_STRING scheduler_states_names[] =
{
  { C_STRING_WITH_LEN("INITIALIZED") },
  { C_STRING_WITH_LEN("RUNNING") },
  { C_STRING_WITH_LEN("STOPPING") }
};

struct scheduler_param {
  THD *thd;
  Event_scheduler *scheduler;
};


/*
  Prints the stack of infos, warnings, errors from thd to
  the console so it can be fetched by the logs-into-tables and
  checked later.

  SYNOPSIS
    evex_print_warnings
      thd  Thread used during the execution of the event
      et   The event itself
*/

static void
evex_print_warnings(THD *thd, Event_job_data *et)
{
  MYSQL_ERROR *err;
  DBUG_ENTER("evex_print_warnings");
  if (!thd->warn_list.elements)
    DBUG_VOID_RETURN;

  char msg_buf[10 * STRING_BUFFER_USUAL_SIZE];
  char prefix_buf[5 * STRING_BUFFER_USUAL_SIZE];
  String prefix(prefix_buf, sizeof(prefix_buf), system_charset_info);
  prefix.length(0);
  prefix.append("SCHEDULER: [");

  append_identifier(thd, &prefix, et->definer.str, et->definer.length);
  prefix.append("][", 2);
  append_identifier(thd,&prefix, et->dbname.str, et->dbname.length);
  prefix.append('.');
  append_identifier(thd,&prefix, et->name.str, et->name.length);
  prefix.append("] ", 2);

  List_iterator_fast<MYSQL_ERROR> it(thd->warn_list);
  while ((err= it++))
  {
    String err_msg(msg_buf, sizeof(msg_buf), system_charset_info);
    /* set it to 0 or we start adding at the end. That's the trick ;) */
    err_msg.length(0);
    err_msg.append(prefix);
    err_msg.append(err->msg, strlen(err->msg), system_charset_info);
    err_msg.append("]");
    DBUG_ASSERT(err->level < 3);
    (sql_print_message_handlers[err->level])("%*s", err_msg.length(),
                                              err_msg.c_ptr());
  }
  DBUG_VOID_RETURN;
}


/*
  Performs post initialization of structures in a new thread.

  SYNOPSIS
    post_init_event_thread()
      thd  Thread
*/

bool
post_init_event_thread(THD *thd)
{
  my_thread_init();
  pthread_detach_this_thread();
  thd->real_id= pthread_self();
  if (init_thr_lock() || thd->store_globals())
  {
    thd->cleanup();
    return TRUE;
  }

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
    VOID(sigemptyset(&set));                    // Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  thread_count++;
  thread_running++;
  pthread_mutex_unlock(&LOCK_thread_count);

  return FALSE;
}


/*
  Cleans up the THD and the threaded environment of the thread.

  SYNOPSIS
    deinit_event_thread()
      thd  Thread
*/

void
deinit_event_thread(THD *thd)
{
  thd->proc_info= "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net);
  DBUG_PRINT("exit", ("Event thread finishing"));
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  thread_running--;
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);

  my_thread_end();
}


/*
  Performs pre- pthread_create() initialisation of THD. Do this
  in the thread that will pass THD to the child thread. In the
  child thread call post_init_event_thread().

  SYNOPSIS
    pre_init_event_thread()
      thd  The THD of the thread. Has to be allocated by the caller.

  NOTES
    1. The host of the thead is my_localhost
    2. thd->net is initted with NULL - no communication.
*/

void
pre_init_event_thread(THD* thd)
{
  DBUG_ENTER("pre_init_event_thread");
  thd->client_capabilities= 0;
  thd->security_ctx->master_access= 0;
  thd->security_ctx->db_access= 0;
  thd->security_ctx->host_or_ip= (char*)my_localhost;
  my_net_init(&thd->net, NULL);
  thd->security_ctx->set_user((char*)"event_scheduler");
  thd->net.read_timeout= slave_net_timeout;
  thd->slave_thread= 0;
  thd->options|= OPTION_AUTO_IS_NULL;
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  /*
    Guarantees that we will see the thread in SHOW PROCESSLIST though its
    vio is NULL.
  */

  thd->proc_info= "Initialized";
  thd->version= refresh_version;
  thd->set_time();

  DBUG_VOID_RETURN;
}


/*
  Function that executes the scheduler,

  SYNOPSIS
    event_scheduler_thread()
      arg  Pointer to `struct scheduler_param`

  RETURN VALUE
    0  OK
*/

pthread_handler_t
event_scheduler_thread(void *arg)
{
  /* needs to be first for thread_stack */
  THD *thd= (THD *)((struct scheduler_param *) arg)->thd;
  Event_scheduler *scheduler= ((struct scheduler_param *) arg)->scheduler;

  my_free((char*)arg, MYF(0));

  thd->thread_stack= (char *)&thd;              // remember where our stack is

  DBUG_ENTER("event_scheduler_thread");

  if (!post_init_event_thread(thd))
    scheduler->run(thd);

  deinit_event_thread(thd);

  DBUG_RETURN(0);                               // Against gcc warnings
}


/*
  Function that executes an event in a child thread. Setups the 
  environment for the event execution and cleans after that.

  SYNOPSIS
    event_worker_thread()
      arg  The Event_job_data object to be processed

  RETURN VALUE
    0  OK
*/

pthread_handler_t
event_worker_thread(void *arg)
{
  /* needs to be first for thread_stack */
  THD *thd; 
  Event_job_data *event= (Event_job_data *)arg;
  int ret;

  thd= event->thd;

  thd->thread_stack= (char *) &thd;             // remember where our stack is
  DBUG_ENTER("event_worker_thread");

  if (!post_init_event_thread(thd))
  {
    DBUG_PRINT("info", ("Baikonur, time is %d, BURAN reporting and operational."
               "THD=0x%lx", time(NULL), thd));

    sql_print_information("SCHEDULER: [%s.%s of %s] executing in thread %lu. "
                          "Execution %u",
                          event->dbname.str, event->name.str,
                          event->definer.str, thd->thread_id,
                          event->execution_count);

    thd->enable_slow_log= TRUE;

    ret= event->execute(thd);

    evex_print_warnings(thd, event);

    sql_print_information("SCHEDULER: [%s.%s of %s] executed in thread %lu. "
                          "RetCode=%d", event->dbname.str, event->name.str,
                          event->definer.str, thd->thread_id, ret);
    if (ret == EVEX_COMPILE_ERROR)
      sql_print_information("SCHEDULER: COMPILE ERROR for event %s.%s of %s",
                            event->dbname.str, event->name.str,
                            event->definer.str);
    else if (ret == EVEX_MICROSECOND_UNSUP)
      sql_print_information("SCHEDULER: MICROSECOND is not supported");
  }
end:
  DBUG_PRINT("info", ("BURAN %s.%s is landing!", event->dbname.str,
             event->name.str));
  delete event;

  deinit_event_thread(thd);

  DBUG_RETURN(0);                               // Can't return anything here
}


/*
  Performs initialization of the scheduler data, outside of the
  threading primitives.

  SYNOPSIS
    Event_scheduler::init_scheduler()
*/

void
Event_scheduler::init_scheduler(Event_queue *q)
{
  LOCK_DATA();
  queue= q;
  started_events= 0;
  scheduler_thd= NULL;
  state= INITIALIZED;
  UNLOCK_DATA();
}


void
Event_scheduler::deinit_scheduler() {}


/*
  Inits scheduler's threading primitives.

  SYNOPSIS
    Event_scheduler::init_mutexes()
*/

void
Event_scheduler::init_mutexes()
{
  pthread_mutex_init(&LOCK_scheduler_state, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_state, NULL);
}


/*
  Deinits scheduler's threading primitives.

  SYNOPSIS
    Event_scheduler::deinit_mutexes()
*/

void
Event_scheduler::deinit_mutexes()
{
  pthread_mutex_destroy(&LOCK_scheduler_state);
  pthread_cond_destroy(&COND_state);
}


/*
  Starts the scheduler (again). Creates a new THD and passes it to
  a forked thread. Does not wait for acknowledgement from the new
  thread that it has started. Asynchronous starting. Most of the
  needed initializations are done in the current thread to minimize
  the chance of failure in the spawned thread.

  SYNOPSIS
    Event_scheduler::start()

  RETURN VALUE
    FALSE  OK
    TRUE   Error (not reported)
*/

bool
Event_scheduler::start()
{
  THD *new_thd= NULL;
  bool ret= FALSE;
  pthread_t th;
  struct scheduler_param *scheduler_param_value;
  DBUG_ENTER("Event_scheduler::start");

  LOCK_DATA();
  DBUG_PRINT("info", ("state before action %s", scheduler_states_names[state]));
  if (state > INITIALIZED)
    goto end;

  if (!(new_thd= new THD))
  {
    sql_print_error("SCHEDULER: Cannot init manager event thread");
    ret= TRUE;
    goto end;
  }
  pre_init_event_thread(new_thd);
  new_thd->system_thread= SYSTEM_THREAD_EVENT_SCHEDULER;
  new_thd->command= COM_DAEMON;

  scheduler_param_value=
    (struct scheduler_param *)my_malloc(sizeof(struct scheduler_param), MYF(0));
  scheduler_param_value->thd= new_thd;
  scheduler_param_value->scheduler= this;

  scheduler_thd= new_thd;
  DBUG_PRINT("info", ("Setting state go RUNNING"));
  state= RUNNING;
  DBUG_PRINT("info", ("Forking new thread for scheduduler. THD=0x%lx", new_thd));
  if (pthread_create(&th, &connection_attrib, event_scheduler_thread,
                    (void*)scheduler_param_value))
  {
    DBUG_PRINT("error", ("cannot create a new thread"));
    state= INITIALIZED;
    scheduler_thd= NULL;
    ret= TRUE;

    new_thd->proc_info= "Clearing";
    DBUG_ASSERT(new_thd->net.buff != 0);
    net_end(&new_thd->net);
    pthread_mutex_lock(&LOCK_thread_count);
    thread_count--;
    thread_running--;
    delete new_thd;
    pthread_mutex_unlock(&LOCK_thread_count);
  }
end:
  UNLOCK_DATA();

  DBUG_RETURN(ret);
}


/*
  The main loop of the scheduler.

  SYNOPSIS
    Event_scheduler::run()
      thd  Thread

  RETURN VALUE
    FALSE  OK
    TRUE   Error (Serious error)
*/

bool
Event_scheduler::run(THD *thd)
{
  int res= FALSE;
  struct timespec abstime;
  Event_job_data *job_data;
  DBUG_ENTER("Event_scheduler::run");

  sql_print_information("SCHEDULER: Manager thread started with id %lu",
                        thd->thread_id);
  /*
    Recalculate the values in the queue because there could have been stops
    in executions of the scheduler and some times could have passed by.
  */
  queue->recalculate_activation_times(thd);

  while (is_running())
  {
    /* Gets a minimized version */
    if (queue->get_top_for_execution_if_time(thd, &job_data))
    {
      sql_print_information("SCHEDULER: Serious error during getting next "
                            "event to execute. Stopping");
      break;
    }

    DBUG_PRINT("info", ("get_top returned job_data=0x%lx", job_data));
    if (job_data)
    {
      if ((res= execute_top(thd, job_data)))
        break;
    }
    else
    {
      DBUG_ASSERT(thd->killed);
      DBUG_PRINT("info", ("job_data is NULL, the thread was killed"));
    }
    DBUG_PRINT("info", ("state=%s", scheduler_states_names[state].str));
  }
  LOCK_DATA();
  DBUG_PRINT("info", ("Signalling back to the stopper COND_state"));
  state= INITIALIZED;
  pthread_cond_signal(&COND_state);
  UNLOCK_DATA();
  sql_print_information("SCHEDULER: Stopped");

  DBUG_RETURN(res);
}


/*
  Creates a new THD instance and then forks a new thread, while passing
  the THD pointer and job_data to it.

  SYNOPSIS
    Event_scheduler::execute_top()

  RETURN VALUE
    FALSE  OK
    TRUE   Error (Serious error)
*/

bool
Event_scheduler::execute_top(THD *thd, Event_job_data *job_data)
{
  THD *new_thd;
  pthread_t th;
  int res= 0;
  DBUG_ENTER("Event_scheduler::execute_top");
  if (!(new_thd= new THD()))
    goto error;

  pre_init_event_thread(new_thd);
  new_thd->system_thread= SYSTEM_THREAD_EVENT_WORKER;
  job_data->thd= new_thd;
  DBUG_PRINT("info", ("BURAN %s@%s ready for start t-3..2..1..0..ignition",
             job_data->dbname.str, job_data->name.str));

  /* Major failure */
  if ((res= pthread_create(&th, &connection_attrib, event_worker_thread,
                           job_data)))
    goto error;

  ++started_events;

  DBUG_PRINT("info", ("Launch succeeded. BURAN is in THD=0x%lx", new_thd));
  DBUG_RETURN(FALSE);

error:
  DBUG_PRINT("error", ("Baikonur, we have a problem! res=%d", res));
  if (new_thd)
  {
    new_thd->proc_info= "Clearing";
    DBUG_ASSERT(new_thd->net.buff != 0);
    net_end(&new_thd->net);
    pthread_mutex_lock(&LOCK_thread_count);
    thread_count--;
    thread_running--;
    delete new_thd;
    pthread_mutex_unlock(&LOCK_thread_count);
  }
  delete job_data;
  DBUG_RETURN(TRUE);
}


/*
  Checks whether the state of the scheduler is RUNNING

  SYNOPSIS
    Event_scheduler::is_running()

  RETURN VALUE
    TRUE   RUNNING
    FALSE  Not RUNNING
*/

inline bool
Event_scheduler::is_running()
{
  LOCK_DATA();
  bool ret= (state == RUNNING);
  UNLOCK_DATA();
  return ret;
}


/*
  Stops the scheduler (again). Waits for acknowledgement from the
  scheduler that it has stopped - synchronous stopping.

  SYNOPSIS
    Event_scheduler::stop()

  RETURN VALUE
    FALSE  OK
    TRUE   Error (not reported)
*/

bool
Event_scheduler::stop()
{
  THD *thd= current_thd;
  DBUG_ENTER("Event_scheduler::stop");
  DBUG_PRINT("enter", ("thd=0x%lx", current_thd));

  LOCK_DATA();
  DBUG_PRINT("info", ("state before action %s", scheduler_states_names[state]));
  if (state != RUNNING)
    goto end;

  /* Guarantee we don't catch spurious signals */
  do {
    DBUG_PRINT("info", ("Waiting for COND_started_or_stopped from the manager "
                        "thread.  Current value of state is %s . "
                        "workers count=%d", scheduler_states_names[state].str,
                        workers_count()));
    /*
      NOTE: We don't use kill_one_thread() because it can't kill COM_DEAMON
      threads. In addition, kill_one_thread() requires THD but during shutdown
      current_thd is NULL. Hence, if kill_one_thread should be used it has to
      be modified to kill also daemons, by adding a flag, and also we have to
      create artificial THD here. To save all this work, we just do what
      kill_one_thread() does to kill a thread. See also sql_repl.cc for similar
      usage.
    */

    state= STOPPING;
    DBUG_PRINT("info", ("Manager thread has id %d", scheduler_thd->thread_id));
    /* Lock from delete */
    pthread_mutex_lock(&scheduler_thd->LOCK_delete);
    /* This will wake up the thread if it waits on Queue's conditional */
    sql_print_information("SCHEDULER: Killing manager thread %lu",
                          scheduler_thd->thread_id);
    scheduler_thd->awake(THD::KILL_CONNECTION);
    pthread_mutex_unlock(&scheduler_thd->LOCK_delete);

    /* thd could be 0x0, when shutting down */
    sql_print_information("SCHEDULER: Waiting the manager thread to reply");
    COND_STATE_WAIT(thd, NULL, "Waiting scheduler to stop");
  } while (state == STOPPING);
  DBUG_PRINT("info", ("Manager thread has cleaned up. Set state to INIT"));
  /*
    The rationale behind setting it to NULL here but not destructing it
    beforehand is because the THD will be deinited in event_scheduler_thread().
    It's more clear when the post_init and the deinit is done in one function.
    Here we just mark that the scheduler doesn't have a THD anymore. Though for
    milliseconds the old thread could exist we can't use it anymore. When we
    unlock the mutex in this function a little later the state will be
    INITIALIZED. Therefore, a connection thread could enter the critical section
    and will create a new THD object.
  */
  scheduler_thd= NULL;
end:
  UNLOCK_DATA();
  DBUG_RETURN(FALSE);
}


/*
  Returns the number of living event worker threads.

  SYNOPSIS
    Event_scheduler::workers_count()
*/

uint
Event_scheduler::workers_count()
{
  THD *tmp;
  uint count= 0;
  
  DBUG_ENTER("Event_scheduler::workers_count");
  pthread_mutex_lock(&LOCK_thread_count);       // For unlink from list
  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
    if (tmp->system_thread == SYSTEM_THREAD_EVENT_WORKER)
      ++count;
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("exit", ("%d", count));
  DBUG_RETURN(count);
}


/*
  Auxiliary function for locking LOCK_scheduler_state. Used
  by the LOCK_DATA macro.

  SYNOPSIS
    Event_scheduler::lock_data()
      func  Which function is requesting mutex lock
      line  On which line mutex lock is requested
*/

void
Event_scheduler::lock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler::lock_data");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  pthread_mutex_lock(&LOCK_scheduler_state);
  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_scheduler_data_locked= TRUE;
  DBUG_VOID_RETURN;
}


/*
  Auxiliary function for unlocking LOCK_scheduler_state. Used
  by the UNLOCK_DATA macro.

  SYNOPSIS
    Event_scheduler::unlock_data()
      func  Which function is requesting mutex unlock
      line  On which line mutex unlock is requested
*/

void
Event_scheduler::unlock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler::unlock_data");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  mutex_last_unlocked_at_line= line;
  mutex_scheduler_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;
  pthread_mutex_unlock(&LOCK_scheduler_state);
  DBUG_VOID_RETURN;
}


/*
  Wrapper for pthread_cond_wait/timedwait

  SYNOPSIS
    Event_scheduler::cond_wait()
      thd     Thread (Could be NULL during shutdown procedure)
      abstime If not null then call pthread_cond_timedwait()
      msg     Message for thd->proc_info
      func    Which function is requesting cond_wait
      line    On which line cond_wait is requested
*/

void
Event_scheduler::cond_wait(THD *thd, struct timespec *abstime, const char* msg,
                           const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler::cond_wait");
  waiting_on_cond= TRUE;
  mutex_last_unlocked_at_line= line;
  mutex_scheduler_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;
  if (thd)
    thd->enter_cond(&COND_state, &LOCK_scheduler_state, msg);

  DBUG_PRINT("info", ("pthread_cond_%swait", abstime? "timed":""));
  if (!abstime)
    pthread_cond_wait(&COND_state, &LOCK_scheduler_state);
  else
    pthread_cond_timedwait(&COND_state, &LOCK_scheduler_state, abstime);
  if (thd)
  {
    /*
      This will free the lock so we need to relock. Not the best thing to
      do but we need to obey cond_wait()
    */
    thd->exit_cond("");
    LOCK_DATA();
  }
  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_scheduler_data_locked= TRUE;
  waiting_on_cond= FALSE;
  DBUG_VOID_RETURN;
}


/*
  Dumps the internal status of the scheduler

  SYNOPSIS
    Event_scheduler::dump_internal_status()
      thd  Thread

  RETURN VALUE
    FALSE  OK
    TRUE   Error
*/

bool
Event_scheduler::dump_internal_status(THD *thd)
{
  int ret= 0;
  DBUG_ENTER("Event_scheduler::dump_internal_status");

#ifndef DBUG_OFF
  CHARSET_INFO *scs= system_charset_info;
  Protocol *protocol= thd->protocol;
  char tmp_buff[5*STRING_BUFFER_USUAL_SIZE];
  char int_buff[STRING_BUFFER_USUAL_SIZE];
  String tmp_string(tmp_buff, sizeof(tmp_buff), scs);
  String int_string(int_buff, sizeof(int_buff), scs);
  tmp_string.length(0);
  int_string.length(0);

  do
  {
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler state"), scs);
    protocol->store(scheduler_states_names[state].str,
                    scheduler_states_names[state].length, scs);

    if ((ret= protocol->write()))
      break;

    /* thread_id */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("thread_id"), scs);
    if (thread_id)
    {
      int_string.set((longlong) scheduler_thd->thread_id, scs);
      protocol->store(&int_string);
    }
    else
      protocol->store_null();
    if ((ret= protocol->write()))
      break;

    /* last locked at*/
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler last locked at"), scs);
    tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                          tmp_string.alloced_length(), "%s::%d",
                                          mutex_last_locked_in_func,
                                          mutex_last_locked_at_line));
    protocol->store(&tmp_string);
    if ((ret= protocol->write()))
      break;

    /* last unlocked at*/
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler last unlocked at"), scs);
    tmp_string.length(scs->cset->snprintf(scs, (char*) tmp_string.ptr(),
                                          tmp_string.alloced_length(), "%s::%d",
                                          mutex_last_unlocked_in_func,
                                          mutex_last_unlocked_at_line));
    protocol->store(&tmp_string);
    if ((ret= protocol->write()))
      break;

    /* waiting on */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler waiting on condition"), scs);
    int_string.set((longlong) waiting_on_cond, scs);
    protocol->store(&int_string);
    if ((ret= protocol->write()))
      break;

    /* workers_count */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler workers count"), scs);
    int_string.set((longlong) workers_count(), scs);
    protocol->store(&int_string);
    if ((ret= protocol->write()))
      break;

    /* workers_count */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler executed events"), scs);
    int_string.set((longlong) started_events, scs);
    protocol->store(&int_string);
    if ((ret= protocol->write()))
      break;

    /* scheduler_data_locked */
    protocol->prepare_for_resend();
    protocol->store(STRING_WITH_LEN("scheduler data locked"), scs);
    int_string.set((longlong) mutex_scheduler_data_locked, scs);
    protocol->store(&int_string);
    ret= protocol->write();
  } while (0);
#endif

  DBUG_RETURN(ret);
}
