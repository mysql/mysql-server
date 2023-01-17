/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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
  Plugin user to access the server
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
#define GROUP_REPLICATION_SERVICE_MESSAGE_INIT_FAILURE 9
#define GROUP_REPLICATION_RECOVERY_CHANNEL_STILL_RUNNING 10
#define GROUP_REPLICATION_STOP_WITH_RECOVERY_TIMEOUT 11

/* View timeout (seconds) */
#define VIEW_MODIFICATION_TIMEOUT 60

/* View timeout for Force Members (seconds) */
#define FORCE_MEMBERS_VIEW_MODIFICATION_TIMEOUT 120

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

/*
  Version from which group_replication_consistency system variable
  is supported.
*/
#define TRANSACTION_WITH_GUARANTEES_VERSION 0x080014

/*
  Version from which patch version number is also considered during member
  join, view changes or primary member selection.
*/
#define PRIMARY_ELECTION_PATCH_CONSIDERATION 0x080017

/*
  Version from which group replication and the server support cloning
*/
#define CLONE_GR_SUPPORT_VERSION 0x080017

/*
  Version from which group replication support timeout argument in UDF
  group_replication_set_as_primary
*/
#define MEMBER_VERSION_INTRODUCING_RUNNING_TRANSACTION_TIMEOUT 0x080029

#endif /* PLUGIN_CONSTANTS_INCLUDE */
