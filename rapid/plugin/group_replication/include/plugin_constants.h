/* Copyright (c) 2016, 2017 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PLUGIN_CONSTANTS_INCLUDE
#define PLUGIN_CONSTANTS_INCLUDE

/*
  Plugin user to acess the server
*/
#define GROUPREPL_USER      "mysql.session"
#define GROUPREPL_HOST      "localhost"
#define GROUPREPL_ACCOUNT   GROUPREPL_USER "@" GROUPREPL_HOST

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

/* View timeout (seconds) */
#define VIEW_MODIFICATION_TIMEOUT 60

/*
  Transaction wait timeout before kill (seconds)
  This value comes from innodb_lock_wait_timeout that can make some of the
  transactions fail for their own
*/
#define TRANSACTION_KILL_TIMEOUT 50

#endif /* PLUGIN_CONSTANTS_INCLUDE */
