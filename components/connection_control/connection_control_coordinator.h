/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CONNECTION_CONTROL_COORDINATOR_H
#define CONNECTION_CONTROL_COORDINATOR_H

#include "connection_control_interfaces.h"
#include "connection_control_memory.h"
#include "connection_control_pfs_table.h"

namespace connection_control {
class Connection_event_subscriber {
 public:
  Connection_event_subscriber(
      Connection_event_observer **subscriber,
      std::vector<opt_connection_control,
                  CustomAllocator<opt_connection_control>> *sys_vars) {
    {
      m_subscriber = *subscriber;

      /* Reset the list first */
      for (uint i = static_cast<uint>(OPT_FAILED_CONNECTIONS_THRESHOLD);
           i < static_cast<uint>(OPT_LAST); ++i) {
        m_sys_vars[i] = false;
      }

      /* Now set the bits which are requested by subscriber */
      if (sys_vars != nullptr) {
        for (const auto &var : *sys_vars) {
          m_sys_vars[var] = true;
        }
      }
    }
  }
  Connection_event_observer *m_subscriber;
  bool m_sys_vars[OPT_LAST];
};

/**
  Connection event coordinator.
  This class will keep list of subscribers for different  subevents
  and notify them based on their preference.
*/

class Connection_event_coordinator : public Connection_control_alloc {
 public:
  Connection_event_coordinator() { reset(); }

  /* Functions to receive notification from server */
  void notify_event(
      MYSQL_THD thd,
      const mysql_event_tracking_connection_data *connection_event);
  void notify_sys_var(opt_connection_control variable, void *new_value);

  /* Services provided to observers */
  bool register_event_subscriber(
      Connection_event_observer **subscriber,
      std::vector<opt_connection_control,
                  CustomAllocator<opt_connection_control>> *sys_vars,
      std::vector<stats_connection_control,
                  CustomAllocator<stats_connection_control>> *status_vars);

  bool notify_status_var(Connection_event_observer **observer,
                         stats_connection_control status_var,
                         status_var_action action);

 private:
  void reset();
  std::vector<Connection_event_subscriber,
              CustomAllocator<Connection_event_subscriber>>
      m_subscribers;
  Connection_event_observer *m_status_vars_subscription[STAT_LAST];
};
}  // namespace connection_control
#endif  // !CONNECTION_CONTROL_COORDINATOR_H
