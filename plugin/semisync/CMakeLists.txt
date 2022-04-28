# Copyright (c) 2006, 2022, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

ADD_DEFINITIONS(-DMYSQL_SERVER)
ADD_DEFINITIONS(-DLOG_SUBSYSTEM_TAG="Repl")
ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="semisync")

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(semisync_source
  semisync.cc
  semisync_source.cc
  semisync_source_ack_receiver.cc
  semisync_source_plugin.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "semisync_source"
  VISIBILITY_HIDDEN
  )

MYSQL_ADD_PLUGIN(semisync_replica
  semisync.cc
  semisync_replica.cc
  semisync_replica_plugin.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "semisync_replica"
  VISIBILITY_HIDDEN
  )

MYSQL_ADD_PLUGIN(semisync_master
  semisync.cc
  semisync_source.cc
  semisync_source_ack_receiver.cc
  semisync_source_plugin_old.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "semisync_master"
  VISIBILITY_HIDDEN
  )

MYSQL_ADD_PLUGIN(semisync_slave
  semisync.cc
  semisync_replica.cc
  semisync_replica_plugin_old.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "semisync_slave"
  VISIBILITY_HIDDEN
  )
