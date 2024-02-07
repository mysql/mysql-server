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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_AUTHENTICATION_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_AUTHENTICATION_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_authentication_defs.h
  Data for authentication event tracking.

  This files defines following:
  A. Possible subevents of authentication events
  B. Information provided by producers of the event when
     authentication events are generated.

  @sa @ref EVENT_TRACKING_AUTHENTICATION_SERVICE
*/

/** Generated after FLUSH PRIVILEGES */
#define EVENT_TRACKING_AUTHENTICATION_FLUSH (1 << 0)
/** Generated after CREATE USER | CREATE ROLE */
#define EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE (1 << 1)
/**
  Generated after credential change through:
  - SET PASSWORD
  - ALTER USER
  - GRANT
*/
#define EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE (1 << 2)
/** Generated after RENAME USER */
#define EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME (1 << 3)
/** Generated after DROP USER */
#define EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP (1 << 4)

#define EVENT_TRACKING_AUTHENTICATION_ALL               \
  EVENT_TRACKING_AUTHENTICATION_FLUSH |                 \
      EVENT_TRACKING_AUTHENTICATION_AUTHID_CREATE |     \
      EVENT_TRACKING_AUTHENTICATION_CREDENTIAL_CHANGE | \
      EVENT_TRACKING_AUTHENTICATION_AUTHID_RENAME |     \
      EVENT_TRACKING_AUTHENTICATION_AUTHID_DROP

/**
  @typedef mysql_event_tracking_authentication_subclass_t

  Events for Authentication event tracking

  Event handler can not terminate an event unless stated
  explicitly.
*/
typedef unsigned long mysql_event_tracking_authentication_subclass_t;

/**
  @struct mysql_event_tracking_authentication_data

  Structure for Authentication event tracking.
*/
struct mysql_event_tracking_authentication_data {
  /** Event subclass. */
  mysql_event_tracking_authentication_subclass_t event_subclass;
  /** Event status */
  int status;
  /** Connection id. */
  mysql_connection_id connection_id;
  /** User name */
  mysql_cstring_with_length user;
  /** Host name */
  mysql_cstring_with_length host;
};

#endif  // !COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_AUTHENTICATION_DEFS_H
