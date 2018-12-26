/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PLUGIN_CONSTANTS_INCLUDE
#define PLUGIN_CONSTANTS_INCLUDE

/*
  Plugin user to acess the server
*/
#define GROUPREPL_USER "mysql.session"
#define GROUPREPL_HOST "localhost"
#define GROUPREPL_ACCOUNT GROUPREPL_USER "@" GROUPREPL_HOST

/*
  Plugin errors
*/
#define GROUP_REPLICATION_CONFIGURATION_ERROR 1
#define GROUP_REPLICATION_ALREADY_RUNNING 2
#define GROUP_REPLICATION_REPLICATION_APPLIER_INIT_ERROR 3
#define GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR 4
#define GROUP_REPLICATION_COMMUNICATION_LAYER_JOIN_ERROR 5
#define GROUP_REPLICATION_APPLIER_STOP_TIMEOUT 6
#define GROUP_REPLICATION_MAX_GROUP_SIZE 7
#define GROUP_REPLICATION_COMMAND_FAILURE 8

/* View timeout (seconds) */
#define VIEW_MODIFICATION_TIMEOUT 60

/*
  Transaction wait timeout before kill (seconds)
  This value comes from innodb_lock_wait_timeout that can make some of the
  transactions fail for their own
*/
#define TRANSACTION_KILL_TIMEOUT 50

/*
  Default name of debug and trace file that will be created
  by GCS.
*/
#define GCS_DEBUG_TRACE_FILE "GCS_DEBUG_TRACE"
#endif /* PLUGIN_CONSTANTS_INCLUDE */
