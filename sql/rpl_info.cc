/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <sql_priv.h>
#include "rpl_info.h"

Rpl_info::Rpl_info(const char* type,
                   PSI_mutex_key *param_key_info_run_lock,
                   PSI_mutex_key *param_key_info_data_lock,
                   PSI_mutex_key *param_key_info_data_cond,
                   PSI_mutex_key *param_key_info_start_cond,
                   PSI_mutex_key *param_key_info_stop_cond)
  :Slave_reporting_capability(type),
  key_info_run_lock(param_key_info_run_lock),
  key_info_data_lock(param_key_info_data_lock),
  key_info_data_cond(param_key_info_data_cond),
  key_info_start_cond(param_key_info_start_cond),
  key_info_stop_cond(param_key_info_stop_cond),
  info_thd(0), inited(0), abort_slave(0),
  slave_running(0), slave_run_id(0),
  handler(0)
{
  mysql_mutex_init(*key_info_run_lock, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(*key_info_data_lock,
                   &data_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(*key_info_data_cond, &data_cond, NULL);
  mysql_cond_init(*key_info_start_cond, &start_cond, NULL);
  mysql_cond_init(*key_info_stop_cond, &stop_cond, NULL);
}

Rpl_info::~Rpl_info()
{
  DBUG_ENTER("Rpl_info::~Rpl_info");

  mysql_mutex_destroy(&run_lock);
  mysql_mutex_destroy(&data_lock);
  mysql_cond_destroy(&data_cond);
  mysql_cond_destroy(&start_cond);
  mysql_cond_destroy(&stop_cond);

  if (handler)
    delete handler;

  DBUG_VOID_RETURN;
}

void Rpl_info::set_rpl_info_handler(Rpl_info_handler * param_handler)
{
  handler= param_handler;
}
