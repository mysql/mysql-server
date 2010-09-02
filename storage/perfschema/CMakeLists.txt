# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}
                    ${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

ADD_DEFINITIONS(-DMYSQL_SERVER)

SET(PERFSCHEMA_SOURCES ha_perfschema.h
  pfs_column_types.h
  pfs_column_values.h
  pfs_events_waits.h
  pfs_global.h
  pfs.h
  pfs_instr.h
  pfs_instr_class.h
  pfs_lock.h
  pfs_atomic.h
  pfs_server.h
  pfs_stat.h
  pfs_engine_table.h
  pfs_timer.h
  table_all_instr.h
  table_events_waits.h
  table_events_waits_summary.h
  table_ews_global_by_event_name.h
  table_file_instances.h
  table_file_summary.h
  table_performance_timers.h
  table_setup_consumers.h
  table_setup_instruments.h
  table_setup_timers.h
  table_sync_instances.h
  table_threads.h
  ha_perfschema.cc
  pfs.cc
  pfs_column_values.cc
  pfs_events_waits.cc
  pfs_global.cc
  pfs_instr.cc
  pfs_instr_class.cc
  pfs_server.cc
  pfs_engine_table.cc
  pfs_timer.cc
  table_all_instr.cc
  table_events_waits.cc
  table_events_waits_summary.cc
  table_ews_global_by_event_name.cc
  table_file_instances.cc
  table_file_summary.cc
  table_performance_timers.cc
  table_setup_consumers.cc
  table_setup_instruments.cc
  table_setup_timers.cc
  table_sync_instances.cc
  table_threads.cc
  pfs_atomic.cc
  pfs_check.cc
)

MYSQL_ADD_PLUGIN(perfschema ${PERFSCHEMA_SOURCES} STORAGE_ENGINE DEFAULT STATIC_ONLY)
IF(WITH_PERFSCHEMA_STORAGE_ENGINE AND WITH_UNIT_TESTS)
  ENABLE_TESTING()
  ADD_SUBDIRECTORY(unittest)
ENDIF()
