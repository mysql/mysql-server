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

#include "channel_info.h"                // Channel_info
#include "connection_handler_manager.h"  // Connection_handler_manager
#include "global_threads.h"              // LOCK_thread_count
#include "mysqld_error.h"                // ER_*
#include "sql_audit.h"                   // mysql_audit_release
#include "sql_connect.h"                 // init_new_connection_handler_thread
#include "sql_class.h"                   // THD
#include "sql_parse.h"                   // do_command


bool One_thread_connection_handler::add_connection(Channel_info* channel_info)
{
  if (init_new_connection_handler_thread())
  {
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    return true;
  }

  THD* thd= channel_info->create_thd();
  if (thd == NULL)
  {
    connection_errors_internal++;
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    return true;
  }

  mysql_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  mysql_mutex_unlock(&LOCK_thread_count);

  thd->start_utime= thd->thr_create_utime= my_micro_time();

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
    return true;
  }

  mysql_thread_set_psi_id(thd->thread_id);
  mysql_socket_set_thread_owner(thd->net.vio->mysql_socket);

  add_global_thread(thd);

  if (thd_prepare_connection(thd))
  {
    close_connection(thd);
    thd->release_resources();
    remove_global_thread(thd);
    delete thd;
    return true;
  }

  delete channel_info;
  while (thd_is_connection_alive(thd))
  {
    mysql_audit_release(thd);
    if (do_command(thd))
      break;
  }

  end_connection(thd);
  close_connection(thd);
  dec_connection_count();
  thd->release_resources();
  remove_global_thread(thd);
  delete thd;
  return false;
}
