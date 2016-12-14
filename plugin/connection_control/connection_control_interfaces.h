/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CONNECTION_CONTROL_INTERFACES_H
#define CONNECTION_CONTROL_INTERFACES_H

#include <mysql/plugin_audit.h>         /* mysql_event_connection */

#include "connection_control_data.h"    /* Variables and Status */

#include <vector>                       /* std::vector */
#include <string>                       /* std::string */

namespace connection_control
{
  /* Typedefs for convenience */
  typedef std::string Sql_string;

  /* Forward declarations */
  class Connection_event_coordinator_services;

  class Error_handler
  {
  public:
    virtual void handle_error(const char * error_message)= 0;
    virtual ~Error_handler() {}
  };

  /**
    Interface for recording connection events
  */

  class Connection_event_records
  {
  public:
    virtual bool create_or_update_entry(const Sql_string &s)= 0;
    virtual bool remove_entry(const Sql_string &s)= 0;
    virtual bool match_entry(const Sql_string &s, void *value)= 0;
    virtual void reset_all()= 0;
    virtual ~Connection_event_records()
    { /* Emptiness! */ }
  };


  /**
    Interface for defining action on connection events
  */
  class Connection_event_observer
  {
  public:
    virtual bool notify_event(MYSQL_THD thd,
                              Connection_event_coordinator_services *coordinator,
                              const mysql_event_connection *connection_event,
                              Error_handler *error_handler)= 0;
    virtual bool notify_sys_var(Connection_event_coordinator_services *coordinator,
                                opt_connection_control variable,
                                void *new_value,
                                Error_handler *error_handler)= 0;
    virtual ~Connection_event_observer()
    { /* Nothing to see here! */ }
  };


  /* Status variable action enum */
  typedef enum status_var_action
  {
    ACTION_NONE=0,
    ACTION_INC,
    ACTION_RESET,
    ACTION_LAST /* Must be at the end */
  }status_var_action;


  /**
    Interface to provide service to observers
  */

  class Connection_event_coordinator_services
  {
  public:
    virtual bool notify_status_var(Connection_event_observer **observer,
                                   stats_connection_control status_var,
                                   status_var_action action)= 0;
    virtual bool register_event_subscriber(Connection_event_observer **subscriber,
                                           std::vector<opt_connection_control> *sys_vars,
                                           std::vector<stats_connection_control> *status_vars)= 0;
    virtual ~Connection_event_coordinator_services()
    { /* go away */ }
  };
}
#endif // !CONNECTION_CONTROL_INTERFACES_H
