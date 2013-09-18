/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "my_pthread.h"                  // pthread_handler_t
#include "channel_info.h"                // Channel_info
#include "connection_handler_manager.h"  // Connection_handler_manager
#include "global_threads.h"              // LOCK_thread_count
#include "mysqld.h"                      // max_connections
#include "mysqld_error.h"                // ER_*
#include "sql_audit.h"                   // mysql_audit_release
#include "sql_class.h"                   // THD
#include "sql_connect.h"                 // init_new_connection_handler_thread
#include "sql_parse.h"                   // do_command


// Initialize static members
ulong Per_thread_connection_handler::blocked_pthread_count= 0;
ulong Per_thread_connection_handler::slow_launch_threads = 0;
ulong Per_thread_connection_handler::max_blocked_pthreads= 0;
std::list<Channel_info*> *Per_thread_connection_handler
                            ::waiting_channel_info_list= NULL;

/*
  Number of pthreads currently being woken up to handle new connections.
  Protected by LOCK_thread_count.
*/
static uint wake_pthread= 0;
/*
  Set if we are trying to kill of pthreads in the thread cache.
  Protected by LOCK_thread_count.
*/
static uint kill_blocked_pthreads_flag= 0;


/**
  Block the current pthread for reuse by new connections.

  @retval NULL   Too many pthreads blocked already or shutdown in progress.
  @retval !NULL  Pointer to Channel_info object representing the new connection
                 to be served by this pthread.
*/

Channel_info* Per_thread_connection_handler::block_until_new_connection()
{
  Channel_info *new_conn= NULL;
  mysql_mutex_lock(&LOCK_thread_count);
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

#ifdef HAVE_PSI_THREAD_INTERFACE
    /*
      Delete the instrumentation for the job that just completed,
      before blocking this pthread (blocked on COND_thread_cache).
    */
    PSI_THREAD_CALL(delete_current_thread)();
#endif

    // Block pthread
    blocked_pthread_count++;
    while (!abort_loop && !wake_pthread && !kill_blocked_pthreads_flag)
      mysql_cond_wait(&COND_thread_cache, &LOCK_thread_count);
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
  mysql_mutex_unlock(&LOCK_thread_count);
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
    connection_errors_internal++;
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    delete channel_info;
    goto error;
  }

  mysql_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  mysql_mutex_unlock(&LOCK_thread_count);

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
  thd->thread_stack= (char*) &thd;
  if (thd->store_globals())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES);
    thd->release_resources();
    delete thd;
    goto error;
  }

  /*
    THD::mysys_var::abort is associated with physical thread rather
    than with THD object. So we need to reset this flag before using
    this thread for handling of new THD object/connection.
  */
  thd->mysys_var->abort= 0;
  return thd;

error:
  inc_aborted_connects();
  dec_connection_count();
  return NULL;
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

pthread_handler_t handle_connection(void *arg)
{
  Channel_info* channel_info= static_cast<Channel_info*>(arg);
  THD *thd= NULL;

  if (init_new_connection_handler_thread())
  {
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    inc_aborted_connects();
    dec_connection_count();
    delete channel_info;
    goto end_thread;
  }

  thd= init_new_thd(channel_info);
  if (thd == NULL)
    goto end_thread;

  for (;;)
  {
    mysql_thread_set_psi_id(thd->thread_id);
    mysql_socket_set_thread_owner(thd->net.vio->mysql_socket);

    add_global_thread(thd);

    if (thd_prepare_connection(thd))
      inc_aborted_connects();
    else
    {
      while (thd_is_connection_alive(thd))
      {
        mysql_audit_release(thd);
        if (do_command(thd))
          break;
      }
      end_connection(thd);
    }
    close_connection(thd);
    Connection_handler_manager::get_instance()->remove_connection(thd);

    if (abort_loop) // Server is shutting down so end the pthread.
      break;

    channel_info= Per_thread_connection_handler::block_until_new_connection();
    if (channel_info == NULL)
      break;

    thd= init_new_thd(channel_info);
    if (thd == NULL)
      break;

#ifdef HAVE_PSI_THREAD_INTERFACE
    /*
      Reusing existing pthread:
      Create new instrumentation for the new THD job,
      and attach it to this running pthread.
    */
    PSI_thread *psi= PSI_THREAD_CALL(new_thread)
      (key_thread_one_connection, thd, thd->thread_id);
    PSI_THREAD_CALL(set_thread)(psi);
#endif
  }

end_thread:
  my_thread_end();
  mysql_cond_broadcast(&COND_thread_count);
  pthread_exit(0);
  return NULL;
}


void Per_thread_connection_handler::kill_blocked_pthreads()
{
  mysql_mutex_lock(&LOCK_thread_count);
  kill_blocked_pthreads_flag++;
  while (Per_thread_connection_handler::blocked_pthread_count)
  {
    mysql_cond_broadcast(&COND_thread_cache);
    mysql_cond_wait(&COND_flush_thread_cache, &LOCK_thread_count);
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

  mysql_mutex_unlock(&LOCK_thread_count);
}


bool Per_thread_connection_handler::check_idle_thread_and_enqueue_connection(
                                                  Channel_info* channel_info)
{
  bool res= true;

  mysql_mutex_lock(&LOCK_thread_count);
  if (Per_thread_connection_handler::blocked_pthread_count > wake_pthread)
  {
    DBUG_PRINT("info",("waiting_channel_info_list->push %p", channel_info));
    waiting_channel_info_list->push_back(channel_info);
    wake_pthread++;
    mysql_cond_signal(&COND_thread_cache);
    res= false;
  }
  mysql_mutex_unlock(&LOCK_thread_count);

  return res;
}


bool Per_thread_connection_handler::add_connection(Channel_info* channel_info)
{
  int error= 0;
  pthread_t id;

  DBUG_ENTER("Per_thread_connection_handler::add_connection");
  /*
    TBD  Need to be refactored and access to caching functionality
    via class interface (WL#6407 is currently doing this).
  */

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
    channel_info->send_error_and_close_channel(ER_CANT_CREATE_THREAD,
                                               error, true);
    DBUG_RETURN(true);
  }

  inc_thread_created();
  DBUG_PRINT("info",("Thread created"));
  DBUG_RETURN(false);
}


void Per_thread_connection_handler::remove_connection(THD* thd)
{
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->release_resources();

  // Clean up errors now, before possibly waiting for a new connection.
  ERR_remove_state(0);

  remove_global_thread(thd);

  delete thd;
}


uint Per_thread_connection_handler::get_max_threads() const
{
  return max_connections;
}
