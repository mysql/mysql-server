/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

/**
  @file debug_vars.h

  @brief This header file contains the status of variables used by MySQL tests
  for debug operations. The variables are set to true by the MySQL server if
  the test pertaining to the variable is active. The variables are initialized
   with false (in binlog_event.cpp).
*/
#ifndef MYSQL_BINLOG_EVENT_DEBUG_VARS_H
#define MYSQL_BINLOG_EVENT_DEBUG_VARS_H

namespace mysql::binlog::event::debug {
extern bool debug_checksum_test;
extern bool debug_query_mts_corrupt_db_names;
extern bool debug_simulate_invalid_address;

// TODO(WL#7546):Add variables here as we move methods into libbinlogevent
// from the server while implementing the WL#7546(Moving binlog event
// encoding into a separate package)
}  // namespace mysql::binlog::event::debug

namespace [[deprecated]] binary_log_debug {
using namespace mysql::binlog::event::debug;
}  // namespace binary_log_debug

#endif  // MYSQL_BINLOG_EVENT_DEBUG_VARS_H
