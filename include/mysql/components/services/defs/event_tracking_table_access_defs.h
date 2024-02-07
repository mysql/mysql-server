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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_TABLE_ACCESS_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_TABLE_ACCESS_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_table_access_defs.h
  Data for query event tracking.
*/

/** Occurs when table data are read. */
#define EVENT_TRACKING_TABLE_ACCESS_READ (1 << 0)
/** Occurs when table data are inserted. */
#define EVENT_TRACKING_TABLE_ACCESS_INSERT (1 << 1)
/** Occurs when table data are updated. */
#define EVENT_TRACKING_TABLE_ACCESS_UPDATE (1 << 2)
/** Occurs when table data are deleted. */
#define EVENT_TRACKING_TABLE_ACCESS_DELETE (1 << 3)

#define EVENT_TRACKING_TABLE_ACCESS_ALL                                   \
  EVENT_TRACKING_TABLE_ACCESS_READ | EVENT_TRACKING_TABLE_ACCESS_INSERT | \
      EVENT_TRACKING_TABLE_ACCESS_UPDATE | EVENT_TRACKING_TABLE_ACCESS_DELETE

/**
  @typedef mysql_event_tracking_table_access_subclass_t

  Events for Table access event tracking.
*/
typedef unsigned long mysql_event_tracking_table_access_subclass_t;

/**
  @struct mysql_event_tracking_table_access_data

  Structure for Table access event tracking.
*/
struct mysql_event_tracking_table_access_data {
  /** Event subclass. */
  mysql_event_tracking_table_access_subclass_t event_subclass;
  /** Connection id. */
  mysql_connection_id connection_id;
  /**
    Database name - System charset (defaults to utf8mb4)
    Please use @ref s_mysql_mysql_charset to obtain charset
  */
  mysql_cstring_with_length table_database;
  /**
    Table name - System charset (defaults to utf8mb4)
    Please use @ref s_mysql_mysql_charset to obtain charset
  */
  mysql_cstring_with_length table_name;
};

#endif  // !COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_TABLE_ACCESS_DEFS_H
