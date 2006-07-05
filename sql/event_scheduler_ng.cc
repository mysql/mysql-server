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
#include "event_scheduler_ng.h"
#include "event_queue.h"

#ifdef __GNUC__
#if __GNUC__ >= 2
#define SCHED_FUNC __FUNCTION__
#endif
#else
#define SCHED_FUNC "<unknown>"
#endif

#define LOCK_SCHEDULER_DATA()   lock_data(SCHED_FUNC, __LINE__)
#define UNLOCK_SCHEDULER_DATA() unlock_data(SCHED_FUNC, __LINE__)

extern pthread_attr_t connection_attrib;

struct scheduler_param
{
  THD *thd;
  Event_scheduler_ng *scheduler;
};

struct scheduler_param scheduler_param_value;



static
LEX_STRING scheduler_states_names[] =
{
  { C_STRING_WITH_LEN("INITIALIZED")},
  { C_STRING_WITH_LEN("RUNNING")},
  { C_STRING_WITH_LEN("STOPPING")}
};


class Worker_thread_param
{
public:
  Event_timed *et;
  pthread_mutex_t LOCK_started;
  pthread_cond_t COND_started;
  bool started;

  Worker_thread_param(Event_timed *etn):et(etn), started(FALSE)
  {
    pthread_mutex_init(&LOCK_started, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&COND_started, NULL);  
  }

  ~Worker_thread_param()
  {
    pthread_mutex_destroy(&LOCK_started);
    pthread_cond_destroy(&COND_started);
  }
};


/*
  Prints the stack of infos, warnings, errors from thd to
  the console so it can be fetched by the logs-into-tables and
  checked later.

  SYNOPSIS
    evex_print_warnings
      thd    - thread used during the execution of the event
      et     - the event itself
*/

static void
evex_print_warnings(THD *thd, Event_timed *et)
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
  Inits an scheduler thread handler, both the main and a worker

  SYNOPSIS
    init_event_thread()
      thd - the THD of the thread. Has to be allocated by the caller.

  NOTES
    1. The host of the thead is my_localhost
    2. thd->net is initted with NULL - no communication.

  RETURN VALUE
    0  OK
   -1  Error
*/

static int
init_scheduler_thread(THD* thd)
{
  DBUG_ENTER("init_event_thread");
  thd->client_capabilities= 0;
  thd->security_ctx->master_access= 0;
  thd->security_ctx->db_access= 0;
  thd->security_ctx->host_or_ip= (char*)my_localhost;
  thd->security_ctx->set_user((char*)"event_scheduler");
  my_net_init(&thd->net, 0);
  thd->net.read_timeout= slave_net_timeout;
  thd->slave_thread= 0;
  thd->options|= OPTION_AUTO_IS_NULL;
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->thread_id= thread_id++;
  threads.append(thd);
  thread_count++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  /*
    Guarantees that we will see the thread in SHOW PROCESSLIST though its
    vio is NULL.
  */
  thd->system_thread= SYSTEM_THREAD_EVENT_SCHEDULER;

  thd->proc_info= "Initialized";
  thd->version= refresh_version;
  thd->set_time();

  DBUG_RETURN(0);
}


pthread_handler_t
event_scheduler_ng_thread(void *arg)
{
  /* needs to be first for thread_stack */
  THD *thd= (THD *)(*(struct scheduler_param *) arg).thd;                               

  thd->thread_stack= (char *)&thd;              // remember where our stack is
  DBUG_ENTER("event_scheduler_ng_thread");

  my_thread_init();
  pthread_detach_this_thread();
  thd->real_id=pthread_self();
  if (init_thr_lock() || thd->store_globals())
  {
    thd->cleanup();
    goto end;
  }

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  ((struct scheduler_param *) arg)->scheduler->run(thd);

end:
  thd->proc_info= "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net);
  DBUG_PRINT("exit", ("Scheduler thread finishing"));
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  thread_running--;
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);

  my_thread_end();
  DBUG_RETURN(0);                               // Against gcc warnings
}


/*
  Function that executes an event in a child thread. Setups the 
  environment for the event execution and cleans after that.

  SYNOPSIS
    event_worker_ng_thread()
      arg  The Event_timed object to be processed

  RETURN VALUE
    0  OK
*/

pthread_handler_t
event_worker_ng_thread(void *arg)
{
  /* needs to be first for thread_stack */
  THD *thd; 
  Event_timed *event= (Event_timed *)arg;
  int ret;

  thd= event->thd;
  thd->thread_stack= (char *) &thd;

  DBUG_ENTER("event_worker_thread");
  DBUG_PRINT("enter", ("event=[%s.%s]", event->dbname.str, event->name.str));

  my_thread_init();
  pthread_detach_this_thread();
  thd->real_id=pthread_self();
  if (init_thr_lock() || thd->store_globals())
  {
    thd->cleanup();
    goto end;
  }

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK, &set, &thd->block_signals));
#endif
  sql_print_information("SCHEDULER: [%s.%s of %s] executing in thread %lu",
                        event->dbname.str, event->name.str,
                        event->definer.str, thd->thread_id);

  thd->init_for_queries();
  thd->enable_slow_log= TRUE;

  ret= event->execute(thd, thd->mem_root);

  evex_print_warnings(thd, event);

  sql_print_information("SCHEDULER: [%s.%s of %s] executed. RetCode=%d",
                        event->dbname.str, event->name.str,
                        event->definer.str, ret);
  if (ret == EVEX_COMPILE_ERROR)
    sql_print_information("SCHEDULER: COMPILE ERROR for event %s.%s of %s",
                          event->dbname.str, event->name.str,
                          event->definer.str);
  else if (ret == EVEX_MICROSECOND_UNSUP)
    sql_print_information("SCHEDULER: MICROSECOND is not supported");
  
  DBUG_PRINT("info", ("master_access=%d db_access=%d",
             thd->security_ctx->master_access, thd->security_ctx->db_access));

end:
  thd->proc_info= "Clearing";
  DBUG_ASSERT(thd->net.buff != 0);
  /*
    Free it here because net.vio is NULL for us => THD::~THD will check it
    and won't call net_end(&net); See also replication code.
  */
  net_end(&thd->net);
  DBUG_PRINT("info", ("Worker thread %lu exiting", thd->thread_id));
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thread_count--;
  thread_running--;
  delete thd;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  delete event;

  my_thread_end();
  DBUG_RETURN(0);                               // Against gcc warnings
}


bool
Event_scheduler_ng::init_scheduler(Event_queue *q)
{
  thread_id= 0;
  state= INITIALIZED;
  queue= q;
  return FALSE;
}


void
Event_scheduler_ng::deinit_scheduler() {}


void
Event_scheduler_ng::init_mutexes()
{
  pthread_mutex_init(&LOCK_scheduler_state, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_state, NULL);
}


void
Event_scheduler_ng::deinit_mutexes()
{
  pthread_mutex_destroy(&LOCK_scheduler_state);
  pthread_cond_destroy(&COND_state);
}


bool
Event_scheduler_ng::start()
{
  THD *new_thd= NULL;
  bool ret= FALSE;
  pthread_t th;
  DBUG_ENTER("Event_scheduler_ng::start");

  LOCK_SCHEDULER_DATA();
  if (state > INITIALIZED)
    goto end;

  if (!(new_thd= new THD) || init_scheduler_thread(new_thd))
  {
    sql_print_error("SCHEDULER: Cannot init manager event thread.");
    ret= TRUE;
    goto end;
  }

  scheduler_param_value.thd= new_thd;
  scheduler_param_value.scheduler= this;

  if (pthread_create(&th, &connection_attrib, event_scheduler_ng_thread,
                    (void*)&scheduler_param_value))
  {
    DBUG_PRINT("error", ("cannot create a new thread"));
    state= INITIALIZED;
    ret= TRUE;
  }

  state= RUNNING;
end:
  UNLOCK_SCHEDULER_DATA();

  if (ret && new_thd)
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
  DBUG_RETURN(ret);
}


bool
Event_scheduler_ng::stop()
{
  THD *thd= current_thd;
  DBUG_ENTER("Event_scheduler_ng::stop");
  DBUG_PRINT("enter", ("thd=%p", current_thd));

  LOCK_SCHEDULER_DATA();
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
    pthread_cond_wait(&COND_state, &LOCK_scheduler_state);
  } while (state == STOPPING);
  DBUG_PRINT("info", ("Manager thread has cleaned up. Set state to INIT"));
end:
  UNLOCK_SCHEDULER_DATA();
  DBUG_RETURN(FALSE);
}


bool
Event_scheduler_ng::run(THD *thd)
{
  struct timespec abstime;
  Event_timed *job_data;

  LOCK_SCHEDULER_DATA();

  thread_id= thd->thread_id;
  sql_print_information("SCHEDULER: Manager thread started with id %lu",
                        thread_id);
  while (state == RUNNING)
  {
    thd->end_time();
    /* Gets a minimized version */
    job_data= queue->get_top_for_execution_if_time(thd, thd->query_start(),
                                                   &abstime);
    DBUG_PRINT("info", ("get_top returned job_data=%p now=%d abs_time.tv_sec=%d",
               job_data, thd->query_start(), abstime.tv_sec));
    if (!job_data && !abstime.tv_sec)
    {
      thd->enter_cond(&COND_state, &LOCK_scheduler_state,
                      "Waiting on empty queue");
      pthread_cond_wait(&COND_state, &LOCK_scheduler_state);
      thd->exit_cond("");
      DBUG_PRINT("info", ("Woke up. Got COND_state"));
      LOCK_SCHEDULER_DATA();
    }
    else if (abstime.tv_sec)
    {
      thd->enter_cond(&COND_state, &LOCK_scheduler_state,
                      "Waiting for next activation");
      pthread_cond_timedwait(&COND_state, &LOCK_scheduler_state, &abstime);
      /*
        If we get signal we should recalculate the whether it's the right time
        because there could be :
        1. Spurious wake-up
        2. The top of the queue was changed (new one becase of create/update)
      */
      /* This will do implicit UNLOCK_SCHEDULER_DATA() */
      thd->exit_cond("");
      DBUG_PRINT("info", ("Woke up. Got COND_stat or time for execution."));
      LOCK_SCHEDULER_DATA();
    }
    else
    {
      int res;
      UNLOCK_SCHEDULER_DATA();
      res= execute_top(thd, job_data);
      LOCK_SCHEDULER_DATA();
      if (res)
        break;
    }
    DBUG_PRINT("info", ("state=%s", scheduler_states_names[state].str));
  }
  DBUG_PRINT("info", ("Signalling back to the stopper COND_state"));
  pthread_cond_signal(&COND_state);
error:
  state= INITIALIZED;
  UNLOCK_SCHEDULER_DATA();
  sql_print_information("SCHEDULER: Stopped");

  return FALSE;
}


bool
Event_scheduler_ng::execute_top(THD *thd, Event_timed *job_data)
{
  THD *new_thd;
  pthread_t th;
  DBUG_ENTER("Event_scheduler_ng::execute_top");
  if (!(new_thd= new THD) || init_scheduler_thread(new_thd))
    goto error;

  /* Major failure */
  job_data->thd= new_thd;
  DBUG_PRINT("info", ("Starting new thread for %s@%s",
             job_data->dbname.str, job_data->name.str));
  if (pthread_create(&th, &connection_attrib, event_worker_ng_thread, job_data))
    goto error;

  DBUG_RETURN(FALSE);

error:
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
  DBUG_RETURN(TRUE);
}


enum Event_scheduler_ng::enum_state
Event_scheduler_ng::get_state()
{
  enum Event_scheduler_ng::enum_state ret;
  LOCK_SCHEDULER_DATA();
  ret= state;
  UNLOCK_SCHEDULER_DATA();
  return ret;
}


int
Event_scheduler_ng::dump_internal_status(THD *thd)
{
  return 1;

}


uint
Event_scheduler_ng::workers_count()
{
  THD *tmp;
  uint count= 0;
  
  DBUG_ENTER("Event_scheduler_ng::workers_count");
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    if (tmp->command == COM_DAEMON)
      continue;
    if (tmp->system_thread == SYSTEM_THREAD_EVENT_WORKER)
      ++count;
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  DBUG_PRINT("exit", ("%d", count));
  DBUG_RETURN(count);
}


/*
  Signals the main scheduler thread that the queue has changed
  its state.

  SYNOPSIS
    Event_scheduler_ng::queue_changed()
*/

void
Event_scheduler_ng::queue_changed()
{
  DBUG_ENTER("Event_scheduler_ng::queue_changed");
  DBUG_PRINT("info", ("Sending COND_state"));
  pthread_cond_signal(&COND_state);
  DBUG_VOID_RETURN;
}


void
Event_scheduler_ng::lock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler_ng::lock_mutex");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  pthread_mutex_lock(&LOCK_scheduler_state);
  mutex_last_locked_in_func= func;
  mutex_last_locked_at_line= line;
  mutex_scheduler_data_locked= TRUE;
  DBUG_VOID_RETURN;
}


void
Event_scheduler_ng::unlock_data(const char *func, uint line)
{
  DBUG_ENTER("Event_scheduler_ng::unlock_mutex");
  DBUG_PRINT("enter", ("func=%s line=%u", func, line));
  mutex_last_unlocked_at_line= line;
  mutex_scheduler_data_locked= FALSE;
  mutex_last_unlocked_in_func= func;
  pthread_mutex_unlock(&LOCK_scheduler_state);
  DBUG_VOID_RETURN;
}
