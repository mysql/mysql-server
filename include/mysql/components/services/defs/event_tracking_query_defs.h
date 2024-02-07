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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_QUERY_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_QUERY_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_query_defs.h
  Data for query event tracking.
*/

/** Query start event. */
#define EVENT_TRACKING_QUERY_START (1 << 0)
/** Nested query start event. */
#define EVENT_TRACKING_QUERY_NESTED_START (1 << 1)
/** Query post parse event. */
#define EVENT_TRACKING_QUERY_STATUS_END (1 << 2)
/** Nested query status end event. */
#define EVENT_TRACKING_QUERY_NESTED_STATUS_END (1 << 3)

#define EVENT_TRACKING_QUERY_ALL                                   \
  EVENT_TRACKING_QUERY_START | EVENT_TRACKING_QUERY_NESTED_START | \
      EVENT_TRACKING_QUERY_STATUS_END | EVENT_TRACKING_QUERY_NESTED_STATUS_END

/**
  @typedef mysql_event_tracking_query_subclass_t

  Events for Query event tracking.
*/
typedef unsigned long mysql_event_tracking_query_subclass_t;

/**
  @struct mysql_event_tracking_query_data

  Structure for Query event tracking.
*/
struct mysql_event_tracking_query_data {
  /** Event subclass. */
  mysql_event_tracking_query_subclass_t event_subclass;
  /** Event status. */
  int status;
  /** Connection id. */
  mysql_connection_id connection_id;
  /** SQL command string - ASCII */
  mysql_sql_command_t sql_command;
  /** SQL query. */
  mysql_cstring_with_length query;
  /** SQL query charset. */
  const char *query_charset;
};

#endif  // COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_QUERY_DEFS_H
