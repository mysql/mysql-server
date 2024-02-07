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

#ifndef COMPONENTS_TEST_EVENT_TRACKING_TEST_EVENT_TRACKING_EXAMPLE_SERVICE_DEFS
#define COMPONENTS_TEST_EVENT_TRACKING_TEST_EVENT_TRACKING_EXAMPLE_SERVICE_DEFS

#define EVENT_TRACKING_EXAMPLE_FIRST (1 << 0)
#define EVENT_TRACKING_EXAMPLE_SECOND (1 << 1)
#define EVENT_TRACKING_EXAMPLE_THIRD (1 << 2)

#define EVENT_TRACKING_EXAMPLE_ALL                               \
  EVENT_TRACKING_EXAMPLE_FIRST | EVENT_TRACKING_EXAMPLE_SECOND | \
      EVENT_TRACKING_EXAMPLE_THIRD

typedef unsigned int mysql_event_tracking_example_subclass_t;

/** Data for example event tracking service */
typedef struct mysql_event_tracking_example_data {
  /* Sub event */
  mysql_event_tracking_example_subclass_t event_subclass;
  /* Integer data */
  unsigned int id;
  /* String data */
  const char *name;
} event_tracking_example_service_data;

#endif  // !COMPONENTS_TEST_EVENT_TRACKING_TEST_EVENT_TRACKING_EXAMPLE_SERVICE_DEFS
