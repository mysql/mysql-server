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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_CONNECTION_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_CONNECTION_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_connection_defs.h
  Data for connection event tracking.
*/

/** occurs after authentication phase is completed. */
#define EVENT_TRACKING_CONNECTION_CONNECT (1 << 0)
/** occurs after connection is terminated. */
#define EVENT_TRACKING_CONNECTION_DISCONNECT (1 << 1)
/** occurs after COM_CHANGE_USER RPC is completed. */
#define EVENT_TRACKING_CONNECTION_CHANGE_USER (1 << 2)
/** occurs before authentication. */
#define EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE (1 << 3)

#define EVENT_TRACKING_CONNECTION_ALL                                        \
  EVENT_TRACKING_CONNECTION_CONNECT | EVENT_TRACKING_CONNECTION_DISCONNECT | \
      EVENT_TRACKING_CONNECTION_CHANGE_USER |                                \
      EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE

/**
  @typedef mysql_event_tracking_connection_subclass_t

  Events for Connection event tracking.
*/
typedef unsigned long mysql_event_tracking_connection_subclass_t;

/**
  @struct mysql_event_tracking_connection_data

  Structure for Connection event tracking.
*/
struct mysql_event_tracking_connection_data {
  /** Event subclass */
  mysql_event_tracking_connection_subclass_t event_subclass;
  /** Current status of the connection */
  int status;
  /** Connection id */
  mysql_connection_id connection_id;
  /** User name of this connection */
  mysql_cstring_with_length user;
  /** Priv user */
  mysql_cstring_with_length priv_user;
  /** External user name */
  mysql_cstring_with_length external_user;
  /** Proxy user used for the connection */
  mysql_cstring_with_length proxy_user;
  /** Connection host */
  mysql_cstring_with_length host;
  /** IP of the connection */
  mysql_cstring_with_length ip;
  /**
    Database name specified at connection time - System charset (defaults to
    utf8mb4) Please use @ref s_mysql_mysql_charset to obtain charset
  */
  mysql_cstring_with_length database;
  /** Connection type:
        - 0 Undefined
        - 1 TCP/IP
        - 2 Socket
        - 3 Named pipe
        - 4 SSL
        - 5 Shared memory
  */
  int connection_type;
};

#endif  // !COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_CONNECTION_DEFS_H
