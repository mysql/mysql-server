/*
Copyright (c) 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

/**
  @file debug_vars.h

  @brief This header file contains the status of variables used by MySQL tests
  for debug operations. The variables are set to true by the MySQL server if
  the test pertaining to the variable is active. The variables are initialized
   with false (in binlog_event.cpp).
*/
#ifndef DEBUG_VARS
#define DEBUG_VARS

namespace binary_log_debug
{
  extern bool debug_checksum_test;
  extern bool debug_query_mts_corrupt_db_names;
  extern bool debug_simulate_invalid_address;
  extern bool debug_pretend_version_50034_in_binlog;
  //TODO:Add variables here as we move methods into libbinlogapi from the server
}
#endif
