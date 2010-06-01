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
#include <mysql_priv.h>
#include "rpl_info.h"

Rpl_info::Rpl_info(const char* type)
    :Slave_reporting_capability(type),
    info_thd(0), inited(0), abort_slave(0),
    slave_running(0), slave_run_id(0), handler(0)
{
  pthread_mutex_init(&run_lock, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&data_lock, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&data_cond, NULL);
  pthread_cond_init(&start_cond, NULL);
  pthread_cond_init(&stop_cond, NULL);
}

Rpl_info::~Rpl_info()
{
  DBUG_ENTER("Rpl_info::~Rpl_info");

  pthread_mutex_destroy(&run_lock);
  pthread_mutex_destroy(&data_lock);
  pthread_cond_destroy(&data_cond);
  pthread_cond_destroy(&start_cond);
  pthread_cond_destroy(&stop_cond);

  if (handler)
    delete handler;

  DBUG_VOID_RETURN;
}

void Rpl_info::set_rpl_info_handler(Rpl_info_handler * param_handler)
{
  handler= param_handler;
}
