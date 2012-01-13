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
uint threadpool_oversubscribe;


extern "C" pthread_key(struct st_my_thread_var*, THR_KEY_mysys);

/*
  Worker threads contexts, and THD contexts.
  =====================================
  
  Both worker threads and connections have their sets of thread local variables 
  At the moment it is mysys_var (which has e.g dbug my_error and similar 
  goodies inside), and PSI per-client structure.

  Whenever query is executed following needs to be done:

  1. Save worker thread context.
  2. Change TLS variables to connection specific ones using thread_attach(THD*).
     This function does some additional work , e.g setting up 
     thread_stack/thread_ends_here pointers.
  3. Process query
  4. Restore worker thread context.

  Connection login and termination follows similar schema w.r.t saving and 
  restoring contexts. 

  For both worker thread, and for the connection, mysys variables are created 
  using my_thread_init() and freed with my_thread_end().

*/
struct Worker_thread_context
{
  PSI_thread *psi_thread;
  st_my_thread_var* mysys_var;

  void save()
  {
    psi_thread=  PSI_server?PSI_server->get_thread():0;
    mysys_var= (st_my_thread_var *)pthread_getspecific(THR_KEY_mysys);
  }

  void restore()
  {
    if (PSI_server)
      PSI_server->set_thread(psi_thread);
    pthread_setspecific(THR_KEY_mysys,mysys_var);
    pthread_setspecific(THR_THD, 0);
    pthread_setspecific(THR_MALLOC, 0);
  }
};


/*
  Attach/associate the connection with the OS thread,
*/
static inline bool thread_attach(THD* thd)
{
  pthread_setspecific(THR_KEY_mysys,thd->mysys_var);
  thd->thread_stack=(char*)&thd;
  thd->store_globals();
  if (PSI_server)
    PSI_server->set_thread(thd->event_scheduler.m_psi);
  return 0;
}


int threadpool_add_connection(THD *thd)
{
  int retval=1;
  Worker_thread_context worker_context;
  worker_context.save();

  /*
    Create a new connection context: mysys_thread_var and PSI thread 
    Store them in thd->mysys_var and thd->scheduler.m_psi.
  */

  /* Use my_thread_init() to create new mysys_thread_var. */
  pthread_setspecific(THR_KEY_mysys, 0);
  my_thread_init();
  thd->mysys_var= (st_my_thread_var *)pthread_getspecific(THR_KEY_mysys);
  if (!thd->mysys_var)
  {
    /* Out of memory? */
    worker_context.restore();
    return 1;
  }

  /* Create new PSI thread for use with the THD. */
  if (PSI_server)
  {
    thd->event_scheduler.m_psi = 
      PSI_server->new_thread(key_thread_one_connection, thd, thd->thread_id);
  }


  /* Login. */
  thread_attach(thd);
  ulonglong now= microsecond_interval_timer();
  thd->prior_thr_create_utime= now;
  thd->start_utime= now;
  thd->thr_create_utime= now;

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

  worker_context.restore();
  return retval;
}

void threadpool_remove_connection(THD *thd)
{

  Worker_thread_context worker_context;
  worker_context.save();

  thread_attach(thd);

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

  /* Free resources (thread_var and PSI connection specific struct)*/
  my_thread_end();

  worker_context.restore();

}

int threadpool_process_request(THD *thd)
{
  int retval= 0;
  Worker_thread_context  worker_context;
  worker_context.save();

  thread_attach(thd);

  if (thd->killed >= KILL_CONNECTION)
  {
    /* 
      kill flag can be set have been killed by 
      timeout handler or by a KILL command
    */
    worker_context.restore();
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
  if (!retval)
    thd->net.reading_or_writing= 1;

  worker_context.restore();
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
