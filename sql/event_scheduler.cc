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
LEX_STRING scheduler_states_names[] =
{
  { C_STRING_WITH_LEN("INITIALIZED")},
  { C_STRING_WITH_LEN("RUNNING")},
  { C_STRING_WITH_LEN("STOPPING")}
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
  thread_id= 0;
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

  DBUG_PRINT("info", ("Forking new thread for scheduduler. THD=0x%lx", new_thd));
  if (pthread_create(&th, &connection_attrib, event_scheduler_thread,
                    (void*)scheduler_param_value))
  {
    DBUG_PRINT("error", ("cannot create a new thread"));
    state= INITIALIZED;
    ret= TRUE;
  }
  DBUG_PRINT("info", ("Setting state go RUNNING"));
  state= RUNNING;
end:
  UNLOCK_DATA();

  if (ret && new_thd)
  {
    DBUG_PRINT("info", ("There was an error during THD creation. Clean up"));
    new_thd->proc_info= "Clearing";
    DBUG_ASSERT(new_thd->net.buff != 0);
    net_end(&new_thd->net);
    pthread_mutex_lock(&LOCK_thread_count);
    thread_count--;
    thread_running--;
    delete new_thd;
    pthread_mutex_unlock(&LOCK_thread_count);
  }
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

  LOCK_DATA();

  thread_id= thd->thread_id;
  sql_print_information("SCHEDULER: Manager thread started with id %lu",
                        thread_id);
  /*
    Recalculate the values in the queue because there could have been stops
    in executions of the scheduler and some times could have passed by.
  */
  queue->recalculate_activation_times(thd);
  while (state == RUNNING)
  {
    thd->end_time();
    /* Gets a minimized version */
    if (queue->get_top_for_execution_if_time(thd, thd->query_start(),
                                             &job_data, &abstime))
    {
      sql_print_information("SCHEDULER: Serious error during getting next"
                            " event to execute. Stopping");
      break;
    }

    DBUG_PRINT("info", ("get_top returned job_data=0x%lx now=%d "
                        "abs_time.tv_sec=%d",
                        job_data, thd->query_start(), abstime.tv_sec));
    if (!job_data && !abstime.tv_sec)
    {
      DBUG_PRINT("info", ("The queue is empty. Going to sleep"));
      COND_STATE_WAIT(thd, NULL, "Waiting on empty queue");
      DBUG_PRINT("info", ("Woke up. Got COND_state"));
    }
    else if (abstime.tv_sec)
    {
      DBUG_PRINT("info", ("Have to sleep some time %u s. till %u",
                 abstime.tv_sec - thd->query_start(), abstime.tv_sec));

      COND_STATE_WAIT(thd, &abstime, "Waiting for next activation");
      /*
        If we get signal we should recalculate the whether it's the right time
        because there could be :
        1. Spurious wake-up
        2. The top of the queue was changed (new one becase of create/update)
      */
      DBUG_PRINT("info", ("Woke up. Got COND_stat or time for execution."));
    }
    else
    {
      UNLOCK_DATA();
      res= execute_top(thd, job_data);
      LOCK_DATA();
      if (res)
        break;
      ++started_events;
    }
    DBUG_PRINT("info", ("state=%s", scheduler_states_names[state].str));
  }
  DBUG_PRINT("info", ("Signalling back to the stopper COND_state"));
  pthread_cond_signal(&COND_state);
error:
  state= INITIALIZED;
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

  state= STOPPING;

  DBUG_PRINT("info", ("Manager thread has id %d", thread_id));
  sql_print_information("SCHEDULER: Killing manager thread %lu", thread_id);
  
  pthread_cond_signal(&COND_state);

  /* Guarantee we don't catch spurious signals */
  sql_print_information("SCHEDULER: Waiting the manager thread to reply");
  do {
    DBUG_PRINT("info", ("Waiting for COND_started_or_stopped from the manager "
                        "thread.  Current value of state is %s . "
                        "workers count=%d", scheduler_states_names[state].str,
                        workers_count()));
    /* thd could be 0x0, when shutting down */
    COND_STATE_WAIT(thd, NULL, "Waiting scheduler to stop");
  } while (state == STOPPING);
  DBUG_PRINT("info", ("Manager thread has cleaned up. Set state to INIT"));

  thread_id= 0;
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
  {
    if (tmp->command == COM_DAEMON)
      continue;
    if (tmp->system_thread == SYSTEM_THREAD_EVENT_WORKER)
      ++count;
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("exit", ("%d", count));
  DBUG_RETURN(count);
}


/*
  Signals the main scheduler thread that the queue has changed
  its state.

  SYNOPSIS
    Event_scheduler::queue_changed()
*/

void
Event_scheduler::queue_changed()
{
  DBUG_ENTER("Event_scheduler::queue_changed");
  DBUG_PRINT("info", ("Sending COND_state. state (read wo lock)=%s ",
             scheduler_states_names[state].str));
  pthread_cond_signal(&COND_state);
  DBUG_VOID_RETURN;
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
  Returns the current state of the scheduler

  SYNOPSIS
    Event_scheduler::get_state()

  RETURN VALUE
    The state of the scheduler (INITIALIZED | RUNNING | STOPPING)
*/

enum Event_scheduler::enum_state
Event_scheduler::get_state()
{
  enum Event_scheduler::enum_state ret;
  DBUG_ENTER("Event_scheduler::get_state");
  LOCK_DATA();
  ret= state;
  UNLOCK_DATA();
  DBUG_RETURN(ret);
}


/*
  REMOVE THIS COMMENT AFTER PATCH REVIEW. USED TO HELP DIFF
  Returns whether the scheduler was initialized.
*/

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
      int_string.set((longlong) thread_id, scs);
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
