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

#ifndef COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_MESSAGE_DEFS_H
#define COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_MESSAGE_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_message_defs.h
  Data for message event tracking.
*/

/** Internally generated message. */
#define EVENT_TRACKING_MESSAGE_INTERNAL (1 << 0)
/** User generated message. */
#define EVENT_TRACKING_MESSAGE_USER (1 << 1)

#define EVENT_TRACKING_MESSAGE_ALL \
  EVENT_TRACKING_MESSAGE_INTERNAL | EVENT_TRACKING_MESSAGE_USER

/**
  @typedef mysql_event_tracking_message_subclass_t

  Events for Message event tracking.
*/
typedef unsigned long mysql_event_tracking_message_subclass_t;

/** Value is of the string type. */
#define EVENT_TRACKING_MESSAGE_VALUE_TYPE_STR (1 << 0)
/** Value is of the numeric type. */
#define EVENT_TRACKING_MESSAGE_VALUE_TYPE_NUM (1 << 1)

/**
  @typedef mysql_event_tracking_message_value_type_t

  Type of the value element of the key-value pair.
*/

typedef int mysql_event_tracking_message_value_type_t;

/**
  @struct mysql_event_tracking_message_key_value_t

  Structure that stores key-value pair of the Message event
*/
struct mysql_event_tracking_message_key_value_t {
  /** Key element. */
  mysql_cstring_with_length key;
  /** Value element type. */
  mysql_event_tracking_message_value_type_t value_type;
  /** Value element. */
  union {
    /** String element. */
    mysql_cstring_with_length str;
    /** Numeric element. */
    long long num;
  } value;
};

/**
  @struct mysql_event_tracking_message_data

  Structure for Message event tracking.
*/
struct mysql_event_tracking_message_data {
  /** Connection id. */
  mysql_connection_id connection_id;
  /** Event subclass. */
  mysql_event_tracking_message_subclass_t event_subclass;
  /** Component. */
  mysql_cstring_with_length component;
  /** Producer */
  mysql_cstring_with_length producer;
  /** Message */
  mysql_cstring_with_length message;
  /** Key value map pointer. */
  mysql_event_tracking_message_key_value_t *key_value_map;
  /** Key value map length. */
  size_t key_value_map_length;
};

#endif  // !COMPONENTS_SERVICES_DEFS_EVENT_TRACKING_MESSAGE_DEFS_H
