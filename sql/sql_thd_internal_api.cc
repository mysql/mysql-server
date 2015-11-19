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


int thd_init(THD *thd, char *stack_start, bool bound, PSI_thread_key psi_key)
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
  PSI_thread *psi;
  psi= PSI_THREAD_CALL(new_thread)(psi_key, thd, thd->thread_id());
  if (bound)
  {
    PSI_THREAD_CALL(set_thread_os_id)(psi);
  }
  thd->set_psi(psi);
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


THD *create_thd(bool enable_plugins, bool background_thread, bool bound, PSI_thread_key psi_key)
{
  THD *thd= new THD(enable_plugins);
  if (background_thread)
    thd->system_thread= SYSTEM_THREAD_BACKGROUND;
  (void)thd_init(thd, reinterpret_cast<char*>(&thd), bound, psi_key);
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

bool is_mysql_datadir_path(const char *path)
{
  if (path == NULL || strlen(path) >= FN_REFLEN)
    return false;

  char mysql_data_dir[FN_REFLEN], path_dir[FN_REFLEN];
  convert_dirname(path_dir, path, NullS);
  convert_dirname(mysql_data_dir, mysql_unpacked_real_data_home, NullS);
  size_t mysql_data_home_len= dirname_length(mysql_data_dir);
  size_t path_len= dirname_length(path_dir);

  if (path_len < mysql_data_home_len)
    return true;

  if (!lower_case_file_system)
    return memcmp(mysql_data_dir, path_dir, mysql_data_home_len);

  return files_charset_info->coll->strnncoll(files_charset_info,
                                             reinterpret_cast<uchar*>(path_dir),
                                             path_len,
                                             reinterpret_cast<uchar*>(mysql_data_dir),
                                             mysql_data_home_len,
                                             TRUE);
}


int mysql_tmpfile_path(const char *path, const char *prefix)
{
  DBUG_ASSERT(path != NULL);
  DBUG_ASSERT((strlen(path) + strlen(prefix)) <= FN_REFLEN);

  char filename[FN_REFLEN];
  File fd = create_temp_file(filename, path, prefix,
#ifdef _WIN32
                             O_BINARY | O_TRUNC | O_SEQUENTIAL |
                             O_SHORT_LIVED |
#endif /* _WIN32 */
                             O_CREAT | O_EXCL | O_RDWR | O_TEMPORARY,
                             MYF(MY_WME));
  if (fd >= 0) {
#ifndef _WIN32
    /*
      This can be removed once the following bug is fixed:
      Bug #28903  create_temp_file() doesn't honor O_TEMPORARY option
                  (file not removed) (Unix)
    */
    unlink(filename);
#endif /* !_WIN32 */
  }

  return fd;
}
