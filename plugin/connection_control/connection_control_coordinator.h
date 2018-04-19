/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CONNECTION_CONTROL_COORDINATOR_H
#define CONNECTION_CONTROL_COORDINATOR_H

#include "plugin/connection_control/connection_control_interfaces.h" /* Connection_event_coordinator_services */
#include "plugin/connection_control/connection_control_memory.h" /* Connection_control_alloc */

namespace connection_control {
/**
  Connection event coordinator.
  This class will keep list of subscribers for different  subevents
  and notify them based on their preference.
*/

class Connection_event_coordinator
    : public Connection_event_coordinator_services,
      public Connection_control_alloc {
 public:
  Connection_event_coordinator() { reset(); }
  ~Connection_event_coordinator() { reset(); }

  /* Functions to receive notification from server */
  void notify_event(MYSQL_THD thd, Error_handler *error_handler,
                    const mysql_event_connection *connection_event);
  void notify_sys_var(Error_handler *error_handler,
                      opt_connection_control variable, void *new_value);

  /* Services provided to observers */
  bool register_event_subscriber(
      Connection_event_observer **subscriber,
      std::vector<opt_connection_control> *sys_vars,
      std::vector<stats_connection_control> *status_vars);

  bool notify_status_var(Connection_event_observer **observer,
                         stats_connection_control status_var,
                         status_var_action action);

 private:
  void reset();
  class Connection_event_subscriber {
   public:
    Connection_event_observer *m_subscriber;
    bool m_sys_vars[OPT_LAST];
  };

  std::vector<Connection_event_subscriber> m_subscribers;
  Connection_event_observer *m_status_vars_subscription[STAT_LAST];
};
}  // namespace connection_control
#endif  // !CONNECTION_CONTROL_COORDINATOR_H
