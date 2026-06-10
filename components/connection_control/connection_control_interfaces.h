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

#ifndef CONNECTION_CONTROL_INTERFACES_H
#define CONNECTION_CONTROL_INTERFACES_H

#include <string> /* std::string */
#include <vector> /* std::vector */
#include "connection_control.h"
#include "connection_control_data.h" /* Variables and Status */

namespace connection_control {
/* Typedefs for convenience */
typedef std::string Sql_string;

/* Forward declaration */
class Connection_event_coordinator;

/**
  Interface for defining action on connection events
*/
class Connection_event_observer {
 public:
  virtual bool notify_event(
      MYSQL_THD thd, Connection_event_coordinator *coordinator,
      const mysql_event_tracking_connection_data *connection_event) = 0;
  virtual bool notify_sys_var(Connection_event_coordinator *coordinator,
                              opt_connection_control variable,
                              void *new_value) = 0;
  virtual ~Connection_event_observer() = default;
};

/* Status variable action enum */
typedef enum status_var_action {
  ACTION_NONE = 0,
  ACTION_INC,
  ACTION_RESET,
  ACTION_LAST /* Must be at the end */
} status_var_action;
}  // namespace connection_control
#endif  // !CONNECTION_CONTROL_INTERFACES_H
