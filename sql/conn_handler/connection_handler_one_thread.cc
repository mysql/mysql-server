/*
   Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "mysqld_error.h"                // ER_*
#include "mysqld_thd_manager.h"          // Global_THD_manager
#include "sql_audit.h"                   // mysql_audit_release
#include "sql_connect.h"                 // close_connection
#include "sql_class.h"                   // THD
#include "sql_parse.h"                   // do_command
#include "sql_thd_internal_api.h"        // thd_set_thread_stack


bool One_thread_connection_handler::add_connection(Channel_info* channel_info)
{
  if (my_thread_init())
  {
    connection_errors_internal++;
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    Connection_handler_manager::dec_connection_count();
    return true;
  }

  THD* thd= channel_info->create_thd();
  if (thd == NULL)
  {
    connection_errors_internal++;
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    Connection_handler_manager::dec_connection_count();
    return true;
  }

  thd->set_new_thread_id();

  thd->start_utime= thd->thr_create_utime= my_micro_time();

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
    Connection_handler_manager::dec_connection_count();
    return true;
  }

  mysql_thread_set_psi_id(thd->thread_id());
  mysql_socket_set_thread_owner(
    thd->get_protocol_classic()->get_vio()->mysql_socket);

  Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
  thd_manager->add_thd(thd);

  bool error= false;
  if (thd_prepare_connection(thd))
    error= true; // Returning true causes inc_aborted_connects() to be called.
  else
  {
    delete channel_info;
    while (thd_connection_alive(thd))
    {
      if (do_command(thd))
        break;
    }
    end_connection(thd);
  }
  close_connection(thd, 0, false, false);
  thd->release_resources();
  thd_manager->remove_thd(thd);
  Connection_handler_manager::dec_connection_count();
  delete thd;
  return error;
}
