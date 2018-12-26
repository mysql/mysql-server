/*
   Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "connection_handler_impl.h"

#include "channel_info.h"                // Channel_info
#include "connection_handler_manager.h"  // Connection_handler_manager
#include "mysqld.h"                      // max_connections
#include "mysqld_error.h"                // ER_*
#include "mysqld_thd_manager.h"          // Global_THD_manager
#include "sql_audit.h"                   // mysql_audit_release
#include "sql_class.h"                   // THD
#include "sql_connect.h"                 // close_connection
#include "sql_parse.h"                   // do_command
#include "sql_thd_internal_api.h"        // thd_set_thread_stack
#include "log.h"                         // Error_log_throttle


// Initialize static members
ulong Per_thread_connection_handler::blocked_pthread_count= 0;
ulong Per_thread_connection_handler::slow_launch_threads = 0;
ulong Per_thread_connection_handler::max_blocked_pthreads= 0;
std::list<Channel_info*> *Per_thread_connection_handler
                            ::waiting_channel_info_list= NULL;
mysql_mutex_t Per_thread_connection_handler::LOCK_thread_cache;
mysql_cond_t Per_thread_connection_handler::COND_thread_cache;
mysql_cond_t Per_thread_connection_handler::COND_flush_thread_cache;

// Error log throttle for the thread creation failure in add_connection method.
static
Error_log_throttle create_thd_err_log_throttle(Log_throttle
                                               ::LOG_THROTTLE_WINDOW_SIZE,
                                               sql_print_error,
                                               "Error log throttle: %10lu"
                                               " 'Can't create thread to"
                                               " handle new connection'"
                                               " error(s) suppressed");

/*
  Number of pthreads currently being woken up to handle new connections.
  Protected by LOCK_thread_cache.
*/
static uint wake_pthread= 0;
/*
  Set if we are trying to kill of pthreads in the thread cache.
  Protected by LOCK_thread_cache.
*/
static uint kill_blocked_pthreads_flag= 0;


#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_thread_cache;

static PSI_mutex_info all_per_thread_mutexes[]=
{
  { &key_LOCK_thread_cache, "LOCK_thread_cache", PSI_FLAG_GLOBAL}
};

static PSI_cond_key key_COND_thread_cache;
static PSI_cond_key key_COND_flush_thread_cache;

static PSI_cond_info all_per_thread_conds[]=
{
  { &key_COND_thread_cache, "COND_thread_cache", PSI_FLAG_GLOBAL},
  { &key_COND_flush_thread_cache, "COND_flush_thread_cache", PSI_FLAG_GLOBAL}
};
#endif


void Per_thread_connection_handler::init()
{
#ifdef HAVE_PSI_INTERFACE
  int count= array_elements(all_per_thread_mutexes);
  mysql_mutex_register("sql", all_per_thread_mutexes, count);

  count= array_elements(all_per_thread_conds);
  mysql_cond_register("sql", all_per_thread_conds, count);
#endif

  mysql_mutex_init(key_LOCK_thread_cache, &LOCK_thread_cache,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_thread_cache, &COND_thread_cache);
  mysql_cond_init(key_COND_flush_thread_cache, &COND_flush_thread_cache);
  waiting_channel_info_list= new (std::nothrow) std::list<Channel_info*>;
  DBUG_ASSERT(waiting_channel_info_list != NULL);
}


void Per_thread_connection_handler::destroy()
{
  if (waiting_channel_info_list != NULL)
  {
    delete waiting_channel_info_list;
    waiting_channel_info_list= NULL;
    mysql_mutex_destroy(&LOCK_thread_cache);
    mysql_cond_destroy(&COND_thread_cache);
    mysql_cond_destroy(&COND_flush_thread_cache);
  }
}


/**
  Block the current pthread for reuse by new connections.

  @retval NULL   Too many pthreads blocked already or shutdown in progress.
  @retval !NULL  Pointer to Channel_info object representing the new connection
                 to be served by this pthread.
*/

Channel_info* Per_thread_connection_handler::block_until_new_connection()
{
  Channel_info *new_conn= NULL;
  mysql_mutex_lock(&LOCK_thread_cache);
  if (blocked_pthread_count < max_blocked_pthreads &&
      !kill_blocked_pthreads_flag)
  {
    /* Don't kill the pthread, just block it for reuse */
    DBUG_PRINT("info", ("Blocking pthread for reuse"));

    /*
      mysys_var is bound to the physical thread,
      so make sure mysys_var->dbug is reset to a clean state
      before picking another session in the thread cache.
    */
    DBUG_POP();
    DBUG_ASSERT( ! _db_is_pushed_());

    // Block pthread
    blocked_pthread_count++;
    while (!abort_loop && !wake_pthread && !kill_blocked_pthreads_flag)
      mysql_cond_wait(&COND_thread_cache, &LOCK_thread_cache);
    blocked_pthread_count--;

    if (kill_blocked_pthreads_flag)
      mysql_cond_signal(&COND_flush_thread_cache);
    else if (!abort_loop && wake_pthread)
    {
      wake_pthread--;
      DBUG_ASSERT(!waiting_channel_info_list->empty());
      new_conn= waiting_channel_info_list->front();
      waiting_channel_info_list->pop_front();
      DBUG_PRINT("info", ("waiting_channel_info_list->pop %p", new_conn));
    }
  }
  mysql_mutex_unlock(&LOCK_thread_cache);
  return new_conn;
}


/**
  Construct and initialize a THD object for a new connection.

  @param channel_info  Channel_info object representing the new connection.
                       Will be destroyed by this function.

  @retval NULL   Initialization failed.
  @retval !NULL  Pointer to new THD object for the new connection.
*/

static THD* init_new_thd(Channel_info *channel_info)
{
  THD *thd= channel_info->create_thd();
  if (thd == NULL)
  {
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    delete channel_info;
    return NULL;
  }

  thd->set_new_thread_id();

  thd->start_utime= thd->thr_create_utime= my_micro_time();
  if (channel_info->get_prior_thr_create_utime() != 0)
  {
    /*
      A pthread was created to handle this connection:
      increment slow_launch_threads counter if it took more than
      slow_launch_time seconds to create the pthread.
    */
    ulong launch_time= (ulong) (thd->thr_create_utime -
                                channel_info->get_prior_thr_create_utime());
    if (launch_time >= slow_launch_time * 1000000L)
      Per_thread_connection_handler::slow_launch_threads++;
  }
  delete channel_info;

  /*
    handle_one_connection() is normally the only way a thread would
    start and would always be on the very high end of the stack ,
    therefore, the thread stack always starts at the address of the
    first local variable of handle_one_connection, which is thd. We
    need to know the start of the stack so that we could check for
    stack overruns.
  */
  thd_set_thread_stack(thd, (char*) &thd);
  if (thd->store_globals())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES);
    thd->release_resources();
    delete thd;
    return NULL;
  }

  return thd;
}


/**
  Thread handler for a connection

  @param arg   Connection object (Channel_info)

  This function (normally) does the following:
  - Initialize thread
  - Initialize THD to be used with this thread
  - Authenticate user
  - Execute all queries sent on the connection
  - Take connection down
  - End thread  / Handle next connection using thread from thread cache
*/

extern "C" void *handle_connection(void *arg)
{
  Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
  Connection_handler_manager *handler_manager=
    Connection_handler_manager::get_instance();
  Channel_info* channel_info= static_cast<Channel_info*>(arg);
  bool pthread_reused MY_ATTRIBUTE((unused))= false;

  if (my_thread_init())
  {
    connection_errors_internal++;
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    handler_manager->inc_aborted_connects();
    Connection_handler_manager::dec_connection_count();
    delete channel_info;
    my_thread_exit(0);
    return NULL;
  }

  for (;;)
  {
    THD *thd= init_new_thd(channel_info);
    if (thd == NULL)
    {
      connection_errors_internal++;
      handler_manager->inc_aborted_connects();
      Connection_handler_manager::dec_connection_count();
      break; // We are out of resources, no sense in continuing.
    }

#ifdef HAVE_PSI_THREAD_INTERFACE
    if (pthread_reused)
    {
      /*
        Reusing existing pthread:
        Create new instrumentation for the new THD job,
        and attach it to this running pthread.
      */
      PSI_thread *psi= PSI_THREAD_CALL(new_thread)
        (key_thread_one_connection, thd, thd->thread_id());
      PSI_THREAD_CALL(set_thread_os_id)(psi);
      PSI_THREAD_CALL(set_thread)(psi);
    }
#endif

#ifdef HAVE_PSI_THREAD_INTERFACE
    /* Find the instrumented thread */
    PSI_thread *psi= PSI_THREAD_CALL(get_thread)();
    /* Save it within THD, so it can be inspected */
    thd->set_psi(psi);
#endif /* HAVE_PSI_THREAD_INTERFACE */
    mysql_thread_set_psi_id(thd->thread_id());
    mysql_thread_set_psi_THD(thd);
    mysql_socket_set_thread_owner(
      thd->get_protocol_classic()->get_vio()->mysql_socket);

    thd_manager->add_thd(thd);

    if (thd_prepare_connection(thd))
      handler_manager->inc_aborted_connects();
    else
    {
      while (thd_connection_alive(thd))
      {
        if (do_command(thd))
          break;
      }
      end_connection(thd);
    }
    close_connection(thd, 0, false, false);

    thd->get_stmt_da()->reset_diagnostics_area();
    thd->release_resources();

    // Clean up errors now, before possibly waiting for a new connection.
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_remove_thread_state(0);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

    thd_manager->remove_thd(thd);
    Connection_handler_manager::dec_connection_count();

#ifdef HAVE_PSI_THREAD_INTERFACE
    /*
      Delete the instrumentation for the job that just completed.
    */
    thd->set_psi(NULL);
    PSI_THREAD_CALL(delete_current_thread)();
#endif /* HAVE_PSI_THREAD_INTERFACE */

    delete thd;

    if (abort_loop) // Server is shutting down so end the pthread.
      break;

    channel_info= Per_thread_connection_handler::block_until_new_connection();
    if (channel_info == NULL)
      break;
    pthread_reused= true;
  }

  my_thread_end();
  my_thread_exit(0);
  return NULL;
}


void Per_thread_connection_handler::kill_blocked_pthreads()
{
  mysql_mutex_lock(&LOCK_thread_cache);
  kill_blocked_pthreads_flag++;
  while (Per_thread_connection_handler::blocked_pthread_count)
  {
    mysql_cond_broadcast(&COND_thread_cache);
    mysql_cond_wait(&COND_flush_thread_cache, &LOCK_thread_cache);
  }
  kill_blocked_pthreads_flag--;

  // Drain off the channel info list.
  while (!waiting_channel_info_list->empty())
  {
    Channel_info* channel_info= waiting_channel_info_list->front();
    waiting_channel_info_list->pop_front();
    // close the channel.
    channel_info->send_error_and_close_channel(ER_SERVER_SHUTDOWN, 0, false);
    delete channel_info;
  }
  mysql_mutex_unlock(&LOCK_thread_cache);
}


bool Per_thread_connection_handler::check_idle_thread_and_enqueue_connection(
                                                  Channel_info* channel_info)
{
  bool res= true;

  mysql_mutex_lock(&LOCK_thread_cache);
  if (Per_thread_connection_handler::blocked_pthread_count > wake_pthread)
  {
    DBUG_PRINT("info",("waiting_channel_info_list->push %p", channel_info));
    waiting_channel_info_list->push_back(channel_info);
    wake_pthread++;
    mysql_cond_signal(&COND_thread_cache);
    res= false;
  }
  mysql_mutex_unlock(&LOCK_thread_cache);

  return res;
}


bool Per_thread_connection_handler::add_connection(Channel_info* channel_info)
{
  int error= 0;
  my_thread_handle id;

  DBUG_ENTER("Per_thread_connection_handler::add_connection");

  // Simulate thread creation for test case before we check thread cache
  DBUG_EXECUTE_IF("fail_thread_create", error= 1; goto handle_error;);

  if (!check_idle_thread_and_enqueue_connection(channel_info))
    DBUG_RETURN(false);

  /*
    There are no idle threads avaliable to take up the new
    connection. Create a new thread to handle the connection
  */
  channel_info->set_prior_thr_create_utime();
  error= mysql_thread_create(key_thread_one_connection, &id,
                             &connection_attrib,
                             handle_connection,
                             (void*) channel_info);
#ifndef DBUG_OFF
handle_error:
#endif // !DBUG_OFF

  if (error)
  {
    connection_errors_internal++;
    if (!create_thd_err_log_throttle.log())
      sql_print_error("Can't create thread to handle new connection(errno= %d)",
                      error);
    channel_info->send_error_and_close_channel(ER_CANT_CREATE_THREAD,
                                               error, true);
    Connection_handler_manager::dec_connection_count();
    DBUG_RETURN(true);
  }

  Global_THD_manager::get_instance()->inc_thread_created();
  DBUG_PRINT("info",("Thread created"));
  DBUG_RETURN(false);
}


uint Per_thread_connection_handler::get_max_threads() const
{
  return max_connections;
}
