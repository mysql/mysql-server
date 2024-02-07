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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_PARSE_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_PARSE_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_parse_defs.h
  Data for parse event tracking.
*/

/** occurs before the query parsing. */
#define EVENT_TRACKING_PARSE_PREPARSE (1 << 0)
/** occurs after the query parsing. */
#define EVENT_TRACKING_PARSE_POSTPARSE (1 << 1)

#define EVENT_TRACKING_PARSE_ALL \
  EVENT_TRACKING_PARSE_PREPARSE | EVENT_TRACKING_PARSE_POSTPARSE

/**
  @typedef mysql_event_tracking_parse_subclass_t

  Events for Parse event tracking.
*/
typedef unsigned long mysql_event_tracking_parse_subclass_t;

/** Default value of the flag */
#define EVENT_TRACKING_PARSE_REWRITE_NONE 0
/** Must be set by a plugin if the query is rewritten. */
#define EVENT_TRACKING_PARSE_REWRITE_QUERY_REWRITTEN (1 << 0)
/** set by the server if the query is prepared statement. */
#define EVENT_TRACKING_PARSE_REWRITE_IS_PREPARED_STATEMENT (1 << 1)

/**
  @typedef mysql_event_tracking_parse_rewrite_plugin_flag

  Query rewritting flags
*/
typedef unsigned int mysql_event_tracking_parse_rewrite_plugin_flag;

/**
  @struct mysql_event_tracking_parse_data

  Structure for the Parse event tracking
*/
struct mysql_event_tracking_parse_data {
  /** Connection id. */
  mysql_connection_id connection_id;

  /** MYSQL_AUDIT_[PRE|POST]_PARSE event id */
  mysql_event_tracking_parse_subclass_t event_subclass;

  /** one of FLAG_REWRITE_PLUGIN_* */
  mysql_event_tracking_parse_rewrite_plugin_flag *flags;

  /** input: the original query text */
  mysql_cstring_with_length query;

  /**
    output: returns the null-terminated
    rwritten query allocated by my_malloc()
  */
  mysql_cstring_with_length *rewritten_query;
};

#endif  // !COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_PARSE_DEFS_H
