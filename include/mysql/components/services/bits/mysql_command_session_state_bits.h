/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_BITS_MYSQL_COMMAND_SESSION_STATE_BITS_H
#define COMPONENTS_SERVICES_BITS_MYSQL_COMMAND_SESSION_STATE_BITS_H

#include <stdint.h>

typedef int mysql_command_session_state_type;

/**
  Status codes returned by mysql_command_session_state service methods.
*/
enum mysql_command_session_state_status : uint16_t {
  // Success
  MYSQL_COMMAND_SESSION_STATE_OK = 0,

  // Item not available or the iterator has no more items
  MYSQL_COMMAND_SESSION_STATE_END = 1,

  // Invalid input arg
  MYSQL_COMMAND_SESSION_STATE_INVALID = 2,

  // Caller buffer is NULL or too small. Required length is returned
  MYSQL_COMMAND_SESSION_STATE_BUFFER_TOO_SMALL = 3,

  // Internal error while reading or copying session-state data
  MYSQL_COMMAND_SESSION_STATE_ERROR = 4
};

/**
  Mirror enum_session_state_type values from mysql_com.h to keep this header
  independent from mysql.h so the service API does not expose the client C API.
*/
#define MYSQL_COMMAND_SESSION_TRACK_SYSTEM_VARIABLES 0
#define MYSQL_COMMAND_SESSION_TRACK_SCHEMA 1
#define MYSQL_COMMAND_SESSION_TRACK_STATE_CHANGE 2
#define MYSQL_COMMAND_SESSION_TRACK_GTIDS 3
#define MYSQL_COMMAND_SESSION_TRACK_TRANSACTION_CHARACTERISTICS 4
#define MYSQL_COMMAND_SESSION_TRACK_TRANSACTION_STATE 5

#define MYSQL_COMMAND_SESSION_TRACK_BEGIN \
  MYSQL_COMMAND_SESSION_TRACK_SYSTEM_VARIABLES
#define MYSQL_COMMAND_SESSION_TRACK_END \
  MYSQL_COMMAND_SESSION_TRACK_TRANSACTION_STATE

#endif /* COMPONENTS_SERVICES_BITS_MYSQL_COMMAND_SESSION_STATE_BITS_H */
