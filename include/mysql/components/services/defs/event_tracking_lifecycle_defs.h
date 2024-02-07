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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_LIFECYCLE_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_LIFECYCLE_DEFS_H

/**
  @file mysql/components/services/defs/event_tracking_lifecycle_defs.h
  Data for program lifecycle events
*/

/** Occurs after all subsystem are initialized during system start. */
#define EVENT_TRACKING_STARTUP_STARTUP (1 << 0)

#define EVENT_TRACKING_STARTUP_ALL EVENT_TRACKING_STARTUP_STARTUP

/**
  @typedef mysql_event_tracking_startup_subclass_t

  Events for Startup event tracking.
*/
typedef unsigned long mysql_event_tracking_startup_subclass_t;

/**
  @struct mysql_event_tracking_startup_data

  Structure for Startup event tracking.
*/
struct mysql_event_tracking_startup_data {
  /** Event subclass. */
  mysql_event_tracking_startup_subclass_t event_subclass;
  /** Command line arguments. */
  const char **argv;
  /** Command line arguments count. */
  unsigned int argc;
};

/** Occurs when global variable is set. */
#define EVENT_TRACKING_SHUTDOWN_SHUTDOWN (1 << 0)

#define EVENT_TRACKING_SHUTDOWN_ALL EVENT_TRACKING_SHUTDOWN_SHUTDOWN

/**
  @typedef mysql_event_tracking_shutdown_subclass_t

  Events for Shutdown event tracking.
*/
typedef unsigned long mysql_event_tracking_shutdown_subclass_t;

/** User requested shut down. */
#define EVENT_TRACKING_SHUTDOWN_REASON_SHUTDOWN (1 << 0)
/** The server aborts. */
#define EVENT_TRACKING_SHUTDOWN_REASON_ABORT (1 << 1)

/**
  @typedef mysql_event_tracking_shutdown_reason_t

  Server shutdown reason.
*/
typedef int mysql_event_tracking_shutdown_reason_t;

/**
  @struct mysql_event_tracking_shutdown_data

  Structure for Shutdown event tracking.
*/
struct mysql_event_tracking_shutdown_data {
  /** Shutdown event. */
  mysql_event_tracking_shutdown_subclass_t event_subclass;
  /** Exit code associated with the shutdown event. */
  int exit_code;
  /** Shutdown reason. */
  mysql_event_tracking_shutdown_reason_t reason;
};

#endif  // !COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_LIFECYCLE_DEFS_H
