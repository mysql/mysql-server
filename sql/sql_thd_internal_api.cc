/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_thd_internal_api.h"

#include "mysqld_thd_manager.h"   // Global_THD_manager
#include "sql_class.h"            // THD


int thd_init(THD *thd, char *stack_start)
{
  DBUG_ENTER("thd_new_connection_setup");
  thd->set_time();
  thd->thr_create_utime= thd->start_utime= my_micro_time();

  // TODO: Purge threads currently terminate too late for them to be added.
  // Note that P_S interprets all threads with thread_id != 0 as
  // foreground threads. And THDs need thread_id != 0 to be added
  // to the global THD list.
  if (thd->system_thread != SYSTEM_THREAD_BACKGROUND)
  {
    thd->set_new_thread_id();
    Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
    thd_manager->add_thd(thd);
  }
#ifdef HAVE_PSI_INTERFACE
  PSI_thread_key key_thread;
  if (thd->system_thread == SYSTEM_THREAD_BACKGROUND)
    key_thread= key_thread_background;
  else
    key_thread= key_thread_one_connection;
  thd_set_psi(thd,
              PSI_THREAD_CALL(new_thread)
              (key_thread, thd, thd->thread_id()));
#endif

  if (!thd->system_thread)
  {
    DBUG_PRINT("info", ("init new connection. thd: %p fd: %d",
                        thd, mysql_socket_getfd(
            thd->get_protocol_classic()->get_vio()->mysql_socket)));
  }
  thd_set_thread_stack(thd, stack_start);

  int retval= thd->store_globals();
  DBUG_RETURN(retval);
}


THD *create_thd(bool enable_plugins, bool background_thread)
{
  THD *thd= new THD(enable_plugins);
  if (background_thread)
    thd->system_thread= SYSTEM_THREAD_BACKGROUND;
  (void)thd_init(thd, reinterpret_cast<char*>(&thd));
  return thd;
}


void destroy_thd(THD *thd)
{
  thd->release_resources();
  // TODO: Purge threads currently terminate too late for them to be added.
  if (thd->system_thread != SYSTEM_THREAD_BACKGROUND)
  {
    Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
    thd_manager->remove_thd(thd);
  }
  delete thd;
}


void thd_set_thread_stack(THD *thd, const char *stack_start)
{
  thd->thread_stack= stack_start;
}
