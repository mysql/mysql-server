#include <my_global.h>
#include <violite.h>
#include <sql_priv.h>
#include <sql_class.h>
#include <my_pthread.h>
#include <scheduler.h>
#include <sql_connect.h>
#include <sql_audit.h>
#include <debug_sync.h>


extern bool login_connection(THD *thd);
extern bool do_command(THD *thd);
extern void prepare_new_connection_state(THD* thd);
extern void end_connection(THD *thd);
extern void thd_cleanup(THD *thd);
extern void delete_thd(THD *thd);

/* Threadpool parameters */

uint threadpool_min_threads;
uint threadpool_idle_timeout;
uint threadpool_size;
uint threadpool_stall_limit;
uint threadpool_max_threads;


/*
  Attach/associate the connection with the OS thread, for command processing.
*/
static inline bool thread_attach(THD* thd, char *stack_start, PSI_thread **save_psi_thread)
{
  DBUG_ENTER("thread_attach");

  if (PSI_server)
  {
    *save_psi_thread= PSI_server->get_thread();
    PSI_server->set_thread(thd->event_scheduler.m_psi);
  }
  else
    *save_psi_thread= NULL;
   
  /*
    We need to know the start of the stack so that we could check for
    stack overruns.
  */
  thd->thread_stack= stack_start;


  /* Calls close_connection() on failure */
  if (setup_connection_thread_globals(thd))
  {
    DBUG_RETURN(TRUE);
  }

  /* clear errors from processing the previous THD */
  my_errno= 0;
  thd->mysys_var->abort= 0;

#ifndef DBUG_OFF
  if (thd->event_scheduler.set_explain)
    DBUG_SET(thd->event_scheduler.dbug_explain);
#endif

  DBUG_RETURN(FALSE);
}

/*
  Detach/disassociate the connection with the OS thread.
*/
static inline void thread_detach(THD* thd, PSI_thread *restore_psi_thread)
{
  DBUG_ENTER("thread_detach");
  thd->mysys_var = NULL;
#ifndef DBUG_OFF
  /*
    If during the session @@session.dbug was assigned, the
    dbug options/state has been pushed. Check if this is the
    case, to be able to restore the state when we attach this
    logical connection to a physical thread.
  */
  if (_db_is_pushed_())
  {
    thd->event_scheduler.set_explain= TRUE;
    if (DBUG_EXPLAIN(thd->event_scheduler.dbug_explain, sizeof(thd->event_scheduler.dbug_explain)))
      sql_print_error("thd_scheduler: DBUG_EXPLAIN buffer is too small");
  }
  /* DBUG_POP() is a no-op in case there is no session state */
  DBUG_POP();
#endif
  if (PSI_server)
    PSI_server->set_thread(restore_psi_thread);
  pthread_setspecific(THR_THD, NULL);
  DBUG_VOID_RETURN;
}



int threadpool_add_connection(THD *thd)
{
  int retval=1;
  PSI_thread *psi_thread;
#ifndef DBUG_OFF
  thd->event_scheduler.set_explain = 0;
#endif
  thread_attach(thd, (char *)&thd, &psi_thread);
  ulonglong now= microsecond_interval_timer();
  thd->prior_thr_create_utime= now;
  thd->start_utime= now;
  thd->thr_create_utime= now;

  if (PSI_server)
  {
    thd->event_scheduler.m_psi = 
      PSI_server->new_thread(key_thread_one_connection, thd, thd->thread_id);
    PSI_server->set_thread(thd->event_scheduler.m_psi);
  }
  
  if (setup_connection_thread_globals(thd) == 0)
  {
    if (login_connection(thd) == 0)
    {
       prepare_new_connection_state(thd);
       retval = thd_is_connection_alive(thd)?0:-1;
       thd->net.reading_or_writing= 1;
    }
  }
  thd->skip_wait_timeout= true;
  thread_detach(thd, psi_thread);
  return retval;
}

void threadpool_remove_connection(THD *thd)
{
  PSI_thread *save_psi_thread;

  thread_attach(thd, (char *)&thd, &save_psi_thread);
  thd->killed= KILL_CONNECTION;

  thd->net.reading_or_writing= 0;

  end_connection(thd);
  close_connection(thd, 0);

  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->event_scheduler.data= NULL;
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  unlink_thd(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  mysql_cond_broadcast(&COND_thread_count);
  DBUG_POP();
  if (PSI_server)
    PSI_server->delete_current_thread();
  pthread_setspecific(THR_THD, NULL);
}

int threadpool_process_request(THD *thd)
{
  int retval= 0;
  PSI_thread *psi_thread;
  thread_attach(thd, (char *)&thd, &psi_thread);

  if (thd->killed == KILL_CONNECTION)
  {
    /* 
      kill flag can be set have been killed by 
      timeout handler or by a KILL command
    */
    thread_detach(thd, psi_thread);
    return 1;
  }

  for(;;)
  {
    Vio *vio;
    thd->net.reading_or_writing= 0;
    mysql_audit_release(thd);

    if ((retval= do_command(thd)) != 0)
      break ;

    if (!thd_is_connection_alive(thd))
    {
      retval= 1;
      break;
    }

    vio= thd->net.vio;
    if (!vio->has_data(vio))
    { 
      /*
        More info on this debug sync is in sql_parse.cc
      */
      DEBUG_SYNC(thd, "before_do_command_net_read");
      break;
    }
  }
  thread_detach(thd, psi_thread);
  if (!retval)
    thd->net.reading_or_writing= 1;
  return retval;
}


/*
  Scheduler struct, individual functions are implemented
  in threadpool_unix.cc or threadpool_win.cc
*/

extern bool tp_init();
extern void tp_add_connection(THD*);
extern void tp_wait_begin(THD *, int);
extern void tp_wait_end(THD*);
extern void tp_post_kill_notification(THD *thd);
extern void tp_end(void);

static scheduler_functions tp_scheduler_functions=
{
  0,                                  // max_threads
  NULL,
  NULL,
  tp_init,                            // init
  NULL,                               // init_new_connection_thread
  tp_add_connection,                  // add_connection
  tp_wait_begin,                      // thd_wait_begin
  tp_wait_end,                        // thd_wait_end
  tp_post_kill_notification,          // post_kill_notification
  NULL,                               // end_thread
  tp_end                              // end
};

extern void scheduler_init();

void pool_of_threads_scheduler(struct scheduler_functions *func,
    ulong *arg_max_connections,
    uint *arg_connection_count)
{
  *func = tp_scheduler_functions;
  func->max_threads= *arg_max_connections + 1;
  func->max_connections= arg_max_connections;
  func->connection_count= arg_connection_count;
  scheduler_init();
}
