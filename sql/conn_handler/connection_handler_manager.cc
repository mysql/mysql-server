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

#include "connection_handler_manager.h"

#include "mysqld_error.h"              // ER_*
#include "channel_info.h"              // Channel_info
#include "connection_handler_impl.h"   // Per_thread_connection_handler
#include "plugin_connection_handler.h" // Plugin_connection_handler
#include "sql_callback.h"              // MYSQL_CALLBACK
#include "sql_class.h"                 // THD


// Initialize static members
ulong Connection_handler_manager::aborted_connects= 0;
uint Connection_handler_manager::connection_count= 0;
ulong Connection_handler_manager::max_used_connections= 0;
ulong Connection_handler_manager::thread_created= 0;
uint Connection_handler_manager::max_threads= 0;
THD_event_functions* Connection_handler_manager::event_functions= NULL;
THD_event_functions* Connection_handler_manager::saved_event_functions= NULL;
#ifndef EMBEDDED_LIBRARY
Connection_handler_manager* Connection_handler_manager::m_instance= NULL;
ulong Connection_handler_manager::thread_handling=
  SCHEDULER_ONE_THREAD_PER_CONNECTION;


/**
  Helper functions to allow mysys to call the thread scheduler when
  waiting for locks.
*/

static void scheduler_wait_lock_begin()
{
  MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                 thd_wait_begin, (current_thd, THD_WAIT_TABLE_LOCK));
}

static void scheduler_wait_lock_end()
{
  MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                 thd_wait_end, (current_thd));
}

static void scheduler_wait_sync_begin()
{
  MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                 thd_wait_begin, (current_thd, THD_WAIT_TABLE_LOCK));
}

static void scheduler_wait_sync_end()
{
  MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                 thd_wait_end, (current_thd));
}


bool Connection_handler_manager::check_and_incr_conn_count()
{
  mysql_mutex_lock(&LOCK_connection_count);
  if (connection_count >= max_connections + 1 || abort_loop)
  {
    mysql_mutex_unlock(&LOCK_connection_count);
    connection_errors_max_connection++;
    return false;
  }
  ++connection_count;

  if (connection_count > max_used_connections)
    max_used_connections= connection_count;
  mysql_mutex_unlock(&LOCK_connection_count);
  return true;
}


bool Connection_handler_manager::init()
{
  Per_thread_connection_handler::allocate_waiting_channel_info_list();

  Connection_handler *connection_handler= NULL;
  switch (Connection_handler_manager::thread_handling)
  {
  case SCHEDULER_ONE_THREAD_PER_CONNECTION:
    connection_handler= new (std::nothrow) Per_thread_connection_handler();
    break;
  case SCHEDULER_NO_THREADS:
    connection_handler= new (std::nothrow) One_thread_connection_handler();
    break;
  default:
    DBUG_ASSERT(false);
  }

  if (connection_handler == NULL)
    return true;

  m_instance= new (std::nothrow) Connection_handler_manager(connection_handler);

  if (m_instance == NULL)
  {
    delete connection_handler;
    return true;
  }

  max_threads= connection_handler->get_max_threads();

  // Init common callback functions.
  thr_set_lock_wait_callback(scheduler_wait_lock_begin,
                             scheduler_wait_lock_end);
  thr_set_sync_wait_callback(scheduler_wait_sync_begin,
                             scheduler_wait_sync_end);
  return false;
}


void Connection_handler_manager::destroy_instance()
{
  Per_thread_connection_handler::deallocate_waiting_channel_info_list();

  if (m_instance != NULL)
  {
    delete m_instance;
    m_instance= NULL;
  }
}


void Connection_handler_manager::load_connection_handler(
                                Connection_handler* conn_handler)
{
  // We don't support loading more than one dynamic connection handler
  DBUG_ASSERT(Connection_handler_manager::thread_handling !=
              SCHEDULER_TYPES_COUNT);
  m_saved_connection_handler= m_connection_handler;
  m_saved_thread_handling= Connection_handler_manager::thread_handling;
  m_connection_handler= conn_handler;
  Connection_handler_manager::thread_handling= SCHEDULER_TYPES_COUNT;
  max_threads= m_connection_handler->get_max_threads();
}


bool Connection_handler_manager::unload_connection_handler()
{
  DBUG_ASSERT(m_saved_connection_handler != NULL);
  if (m_saved_connection_handler == NULL)
    return true;
  delete m_connection_handler;
  m_connection_handler= m_saved_connection_handler;
  Connection_handler_manager::thread_handling= m_saved_thread_handling;
  m_saved_connection_handler= NULL;
  m_saved_thread_handling= 0;
  max_threads= m_connection_handler->get_max_threads();
  return false;
}


void
Connection_handler_manager::process_new_connection(Channel_info* channel_info)
{
  if (!check_and_incr_conn_count())
  {
    channel_info->send_error_and_close_channel(ER_CON_COUNT_ERROR, 0, true);
    delete channel_info;
    return;
  }

  if (m_connection_handler->add_connection(channel_info))
  {
    dec_connection_count();
    inc_aborted_connects();
    delete channel_info;
  }
}


THD* create_thd(Channel_info* channel_info)
{
  THD* thd= channel_info->create_thd();
  if (thd == NULL)
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);

  return thd;
}


void destroy_channel_info(Channel_info* channel_info)
{
  delete channel_info;
}


void dec_connection_count()
{
  mysql_mutex_lock(&LOCK_connection_count);
  Connection_handler_manager::connection_count--;
  mysql_mutex_unlock(&LOCK_connection_count);
}


void inc_thread_created()
{
  mysql_mutex_lock(&LOCK_thread_created);
  Connection_handler_manager::thread_created++;
  mysql_mutex_unlock(&LOCK_thread_created);
}


void inc_aborted_connects()
{
  Connection_handler_manager::aborted_connects++;
}
#endif // !EMBEDDED_LIBRARY


extern "C"
{
int my_connection_handler_set(Connection_handler_functions *chf,
                              THD_event_functions *tef)
{
  DBUG_ASSERT(chf != NULL && tef != NULL);
  if (chf == NULL || tef == NULL)
    return 1;

  Plugin_connection_handler *conn_handler=
    new (std::nothrow) Plugin_connection_handler(chf);
  if (conn_handler == NULL)
    return 1;

#ifndef EMBEDDED_LIBRARY
  Connection_handler_manager::get_instance()->
    load_connection_handler(conn_handler);
#endif
  Connection_handler_manager::saved_event_functions=
    Connection_handler_manager::event_functions;
  Connection_handler_manager::event_functions= tef;
  return 0;
}


int my_connection_handler_reset()
{
  Connection_handler_manager::event_functions=
    Connection_handler_manager::saved_event_functions;
#ifndef EMBEDDED_LIBRARY
  return Connection_handler_manager::get_instance()->
    unload_connection_handler();
#else
  return 0;
#endif
}
};
