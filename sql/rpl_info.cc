/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_info.h"

#include "m_string.h"      // strmake

Rpl_info::Rpl_info(const char* type
#ifdef HAVE_PSI_INTERFACE
                   ,PSI_mutex_key *param_key_info_run_lock,
                   PSI_mutex_key *param_key_info_data_lock,
                   PSI_mutex_key *param_key_info_sleep_lock,
                   PSI_mutex_key *param_key_info_thd_lock,
                   PSI_mutex_key *param_key_info_data_cond,
                   PSI_mutex_key *param_key_info_start_cond,
                   PSI_mutex_key *param_key_info_stop_cond,
                   PSI_mutex_key *param_key_info_sleep_cond
#endif
                   ,uint param_id, const char *param_channel
                 )
  :Slave_reporting_capability(type),
#ifdef HAVE_PSI_INTERFACE
  key_info_run_lock(param_key_info_run_lock),
  key_info_data_lock(param_key_info_data_lock),
  key_info_sleep_lock(param_key_info_sleep_lock),
  key_info_thd_lock(param_key_info_thd_lock),
  key_info_data_cond(param_key_info_data_cond),
  key_info_start_cond(param_key_info_start_cond),
  key_info_stop_cond(param_key_info_stop_cond),
  key_info_sleep_cond(param_key_info_sleep_cond),
#endif
  info_thd(0), inited(0), abort_slave(0),
  slave_running(0), slave_run_id(0),
  handler(0), internal_id(param_id)
{
#ifdef HAVE_PSI_INTERFACE
  mysql_mutex_init(*key_info_run_lock,
                    &run_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(*key_info_data_lock,
                   &data_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(*key_info_sleep_lock,
                    &sleep_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(*key_info_thd_lock,
                    &info_thd_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(*key_info_data_cond, &data_cond);
  mysql_cond_init(*key_info_start_cond, &start_cond);
  mysql_cond_init(*key_info_stop_cond, &stop_cond);
  mysql_cond_init(*key_info_sleep_cond, &sleep_cond);
#else
  mysql_mutex_init(NULL, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(NULL, &data_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(NULL, &sleep_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(NULL, &info_thd_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(NULL, &data_cond);
  mysql_cond_init(NULL, &start_cond);
  mysql_cond_init(NULL, &stop_cond);
  mysql_cond_init(NULL, &sleep_cond);
#endif

  if (param_channel)
    strmake(channel, param_channel, sizeof(channel) -1);
  else
    /*create a default empty channel*/
    strmake(channel, "", sizeof(channel) -1);

  is_stopping.atomic_set(0);
}

Rpl_info::~Rpl_info()
{
  delete handler;

  mysql_mutex_destroy(&run_lock);
  mysql_mutex_destroy(&data_lock);
  mysql_mutex_destroy(&sleep_lock);
  mysql_mutex_destroy(&info_thd_lock);
  mysql_cond_destroy(&data_cond);
  mysql_cond_destroy(&start_cond);
  mysql_cond_destroy(&stop_cond);
  mysql_cond_destroy(&sleep_cond);
}
