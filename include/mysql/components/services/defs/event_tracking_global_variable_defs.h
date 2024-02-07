/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_GLOBAL_VARIABLE_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_GLOBAL_VARIABLE_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_global_variable_defs.h
  Data for global variable event tracking.
*/

/** Occurs when global variable is retrieved. */
#define EVENT_TRACKING_GLOBAL_VARIABLE_GET (1 << 0)
/** Occurs when global variable is set. */
#define EVENT_TRACKING_GLOBAL_VARIABLE_SET (1 << 1)

#define EVENT_TRACKING_GLOBAL_VARIABLE_ALL \
  EVENT_TRACKING_GLOBAL_VARIABLE_GET | EVENT_TRACKING_GLOBAL_VARIABLE_SET

/**
  @typedef mysql_event_tracking_global_variable_subclass_t

  Events for Global variable event tracking.
*/
typedef unsigned long mysql_event_tracking_global_variable_subclass_t;

/** Structure for Global variable event tracking. */
struct mysql_event_tracking_global_variable_data {
  /** Event subclass. */
  mysql_event_tracking_global_variable_subclass_t event_subclass;
  /** Connection id. */
  mysql_connection_id connection_id;
  /** SQL command string */
  mysql_sql_command_t sql_command;
  /** Variable name. */
  mysql_cstring_with_length variable_name;
  /** Variable value. */
  mysql_cstring_with_length variable_value;
};

#endif  // !COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_GLOBAL_VARIABLE_DEFS_H
