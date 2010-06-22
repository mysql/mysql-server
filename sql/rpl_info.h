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

#ifndef RPL_INFO_H
#define RPL_INFO_H

#include "mysql_priv.h"
#include "rpl_info_handler.h"
#include "rpl_reporting.h"

class Rpl_info : public Slave_reporting_capability
{
public:
  /*
    standard lock acquisition order to avoid deadlocks:
    run_lock, data_lock, relay_log.LOCK_log, relay_log.LOCK_index
  */
  pthread_mutex_t data_lock,run_lock;
  /*
    start_cond is broadcast when SQL thread is started
    stop_cond - when stopped
    data_cond - when data protected by data_lock changes
  */
  pthread_cond_t data_cond,start_cond,stop_cond;

  THD *info_thd;

  bool inited;
  volatile bool abort_slave;
  volatile uint slave_running;
  volatile ulong slave_run_id;

#ifndef DBUG_OFF
  int events_until_exit;
#endif

  Rpl_info(const char* type);
  virtual ~Rpl_info();

  int check_info()
  {
    return (handler->check_info());
  }

  int reset_info()
  {
    return (handler->reset_info());
  }

  bool is_transactional()
  {
    return (handler->is_transactional());
  }

  char *get_description_info()
  {
    return (handler->get_description_info());
  }

  /**
    Sets the persistency component/handler.

    @param[in] hanlder Pointer to the handler.
  */ 
  void set_rpl_info_handler(Rpl_info_handler * handler);

protected:

  Rpl_info_handler *handler;

private:
  Rpl_info& operator=(const Rpl_info& info);
  Rpl_info(const Rpl_info& info);
};
#endif /* RPL_INFO_H */
