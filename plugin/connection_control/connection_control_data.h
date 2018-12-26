/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CONNECTION_CONTROL_DATA_H
#define CONNECTION_CONTROL_DATA_H

#include <atomic>

#include "my_inttypes.h"

/**
  Enum for system variables : Must be in sync with
  members of Connection_control_variables.
*/
typedef enum opt_connection_control {
  OPT_FAILED_CONNECTIONS_THRESHOLD = 0,
  OPT_MIN_CONNECTION_DELAY,
  OPT_MAX_CONNECTION_DELAY,
  OPT_LAST /* Must be last */
} opt_connection_control;

/**
  Enum for status variables : Must be in sync with
  memebers of Connection_control_statistics.
*/
typedef enum stats_connection_control {
  STAT_CONNECTION_DELAY_TRIGGERED = 0,
  STAT_LAST /* Must be last */
} stats_connection_control;

namespace connection_control {
/** Structure to maintain system variables */
class Connection_control_variables {
 public:
  /* Various global variables */
  int64 failed_connections_threshold;
  int64 min_connection_delay;
  int64 max_connection_delay;
};

/** Structure to maintain statistics */
class Connection_control_statistics {
 public:
  Connection_control_statistics() {}
  /* Various statistics to be collected */
  std::atomic<int64> stats_array[STAT_LAST];
};
}  // namespace connection_control

extern connection_control::Connection_control_statistics g_statistics;
extern connection_control::Connection_control_variables g_variables;
#endif  // !CONNECTION_CONTROL_DATA_H
