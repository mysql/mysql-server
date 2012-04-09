/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_INFO_H
#define RPL_INFO_H

#include "sql_priv.h"
#include "sql_class.h"
#include "rpl_info_handler.h"
#include "rpl_reporting.h"

enum enum_info_repository
{
  INFO_REPOSITORY_FILE= 0,
  INFO_REPOSITORY_TABLE,
  INFO_REPOSITORY_DUMMY,
  /*
    Add new types of repository before this
    entry.
  */
  INVALID_INFO_REPOSITORY
};

class Rpl_info : public Slave_reporting_capability
{
public:
  virtual ~Rpl_info();

  /*
    standard lock acquisition order to avoid deadlocks:
    run_lock, data_lock, relay_log.LOCK_log, relay_log.LOCK_index
    run_lock, sleep_lock
  */
  mysql_mutex_t data_lock, run_lock, sleep_lock;
  /*
    start_cond is broadcast when SQL thread is started
    stop_cond  - when stopped
    data_cond  - when data protected by data_lock changes
    sleep_cond - when killed
  */
  mysql_cond_t data_cond, start_cond, stop_cond, sleep_cond;

#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key *key_info_run_lock, *key_info_data_lock, *key_info_sleep_lock;

  PSI_mutex_key *key_info_data_cond, *key_info_start_cond, *key_info_stop_cond,
                *key_info_sleep_cond;
#endif

  THD *info_thd;

/**
  Defines the fields that are used as primary keys in a table.
*/
  ulong *uidx;

/**
  The number of fields that are used as primary keys in a table.
*/
  uint nidx;

  bool inited;
  volatile bool abort_slave;
  volatile uint slave_running;
  volatile ulong slave_run_id;

#ifndef DBUG_OFF
  int events_until_exit;
#endif

  /**
    Defines the type of the repository that is used.

    @param param_rpl_info_type Type of repository.
  */
  void set_rpl_info_type(uint param_rpl_info_type)
  {
    rpl_info_type= param_rpl_info_type;
  }

  /**
    Gets the type of the repository that is used.

    @return Type of repository.
  */
  uint get_rpl_info_type()
  {
    return(rpl_info_type);
  }

  /**
    Sets the persistency component/handler.

    @param[in] hanlder Pointer to the handler.
  */ 
  void set_rpl_info_handler(Rpl_info_handler * param_handler)
  {
    handler= param_handler;
  }

  /**
    Gets the persistency component/handler.

    @return the handler if there is one.
  */ 
  Rpl_info_handler *get_rpl_info_handler()
  {
    return (handler);
  }

  enum_return_check check_info()
  {
    return (handler->check_info(uidx, nidx));
  }

  int remove_info()
  {
    return (handler->remove_info(uidx, nidx));
  }

  bool is_transactional()
  {
    return (handler->is_transactional());
  }

  bool update_is_transactional()
  {
    return (handler->update_is_transactional());
  }

  void set_idx_info(ulong *param_uidx, uint param_nidx)
  {
    uidx= param_uidx;
    nidx= param_nidx;
  }

  char *get_description_info()
  {
    return (handler->get_description_info());
  }

  bool copy_info(Rpl_info_handler *from, Rpl_info_handler *to)
  {
    if (read_info(from) || write_info(to))
      return(TRUE);

    return(FALSE);
  }

protected:
  Rpl_info_handler *handler;
  uint rpl_info_type;

  Rpl_info(const char* type
#ifdef HAVE_PSI_INTERFACE
           ,PSI_mutex_key *param_key_info_run_lock,
           PSI_mutex_key *param_key_info_data_lock,
           PSI_mutex_key *param_key_info_sleep_lock,
           PSI_mutex_key *param_key_info_data_cond,
           PSI_mutex_key *param_key_info_start_cond,
           PSI_mutex_key *param_key_info_stop_cond,
           PSI_mutex_key *param_key_info_sleep_cond
#endif
          );

private:
  virtual bool read_info(Rpl_info_handler *from)= 0;
  virtual bool write_info(Rpl_info_handler *to)= 0;

  Rpl_info(const Rpl_info& info);
  Rpl_info& operator=(const Rpl_info& info);
};
#endif /* RPL_INFO_H */
