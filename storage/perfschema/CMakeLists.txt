# Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
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

# Gen_pfs_lex_token
ADD_EXECUTABLE(gen_pfs_lex_token gen_pfs_lex_token.cc)
# gen_pfs_lex_token itself depends on ${CMAKE_CURRENT_BINARY_DIR}/sql/sql_yacc.h
ADD_DEPENDENCIES(gen_pfs_lex_token GenServerSource)

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pfs_lex_token.h
  COMMAND gen_pfs_lex_token > pfs_lex_token.h
  DEPENDS gen_pfs_lex_token
)

SET(PFS_GEN_SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/pfs_lex_token.h
)

SET_SOURCE_FILES_PROPERTIES(${PFS_GEN_SOURCES} PROPERTIES GENERATED 1)

#
# Maintainer: keep this list sorted, to avoid merge collisions.
# Tip: ls -1 *.h, ls -1 *.cc
#
SET(PERFSCHEMA_SOURCES
${PFS_GEN_SOURCES}
ha_perfschema.h
cursor_by_account.h
cursor_by_host.h
cursor_by_thread.h
cursor_by_user.h
pfs.h
pfs_account.h
pfs_atomic.h
pfs_column_types.h
pfs_column_values.h
pfs_con_slice.h
pfs_defaults.h
pfs_digest.h
pfs_engine_table.h
pfs_events.h
pfs_events_stages.h
pfs_events_statements.h
pfs_events_waits.h
pfs_global.h
pfs_host.h
pfs_instr.h
pfs_instr_class.h
pfs_lock.h
pfs_server.h
pfs_setup_actor.h
pfs_setup_object.h
pfs_stat.h
pfs_timer.h
pfs_user.h
pfs_visitor.h
table_accounts.h
table_all_instr.h
table_esgs_by_account_by_event_name.h
table_esgs_by_host_by_event_name.h
table_esgs_by_thread_by_event_name.h
table_esgs_by_user_by_event_name.h
table_esgs_global_by_event_name.h
table_esms_by_account_by_event_name.h
table_esms_by_host_by_event_name.h
table_esms_by_digest.h
table_esms_by_thread_by_event_name.h
table_esms_by_user_by_event_name.h
table_esms_global_by_event_name.h
table_events_stages.h
table_events_statements.h
table_events_waits.h
table_events_waits_summary.h
table_ews_by_account_by_event_name.h
table_ews_by_host_by_event_name.h
table_ews_by_thread_by_event_name.h
table_ews_by_user_by_event_name.h
table_ews_global_by_event_name.h
table_file_instances.h
table_file_summary_by_instance.h
table_file_summary_by_event_name.h
table_socket_instances.h
table_socket_summary_by_instance.h
table_socket_summary_by_event_name.h
table_helper.h
table_host_cache.h
table_hosts.h
table_os_global_by_type.h
table_performance_timers.h
table_setup_actors.h
table_setup_consumers.h
table_setup_instruments.h
table_setup_objects.h
table_setup_timers.h
table_sync_instances.h
table_threads.h
table_tiws_by_index_usage.h
table_tiws_by_table.h
table_tlws_by_table.h
table_users.h
cursor_by_thread_connect_attr.h
table_session_connect.h
table_session_connect_attrs.h
table_session_account_connect_attrs.h
cursor_by_account.cc
cursor_by_host.cc
cursor_by_thread.cc
cursor_by_user.cc
ha_perfschema.cc
pfs.cc
pfs_account.cc
pfs_atomic.cc
pfs_autosize.cc
pfs_check.cc
pfs_column_values.cc
pfs_con_slice.cc
pfs_defaults.cc
pfs_digest.cc
pfs_engine_table.cc
pfs_events_stages.cc
pfs_events_statements.cc
pfs_events_waits.cc
pfs_global.cc
pfs_host.cc
pfs_instr.cc
pfs_instr_class.cc
pfs_server.cc
pfs_setup_actor.cc
pfs_setup_object.cc
pfs_timer.cc
pfs_user.cc
pfs_visitor.cc
table_accounts.cc
table_all_instr.cc
table_esgs_by_account_by_event_name.cc
table_esgs_by_host_by_event_name.cc
table_esgs_by_thread_by_event_name.cc
table_esgs_by_user_by_event_name.cc
table_esgs_global_by_event_name.cc
table_esms_by_account_by_event_name.cc
table_esms_by_host_by_event_name.cc
table_esms_by_digest.cc
table_esms_by_thread_by_event_name.cc
table_esms_by_user_by_event_name.cc
table_esms_global_by_event_name.cc
table_events_stages.cc
table_events_statements.cc
table_events_waits.cc
table_events_waits_summary.cc
table_ews_by_account_by_event_name.cc
table_ews_by_host_by_event_name.cc
table_ews_by_thread_by_event_name.cc
table_ews_by_user_by_event_name.cc
table_ews_global_by_event_name.cc
table_file_instances.cc
table_file_summary_by_instance.cc
table_file_summary_by_event_name.cc
table_socket_instances.cc
table_socket_summary_by_instance.cc
table_socket_summary_by_event_name.cc
table_helper.cc
table_host_cache.cc
table_hosts.cc
table_os_global_by_type.cc
table_performance_timers.cc
table_setup_actors.cc
table_setup_consumers.cc
table_setup_instruments.cc
table_setup_objects.cc
table_setup_timers.cc
table_sync_instances.cc
table_threads.cc
table_tiws_by_index_usage.cc
table_tiws_by_table.cc
table_tlws_by_table.cc
table_users.cc
cursor_by_thread_connect_attr.cc
table_session_connect.cc
table_session_connect_attrs.cc
table_session_account_connect_attrs.cc
)

MYSQL_ADD_PLUGIN(perfschema ${PERFSCHEMA_SOURCES} STORAGE_ENGINE DEFAULT STATIC_ONLY)
IF(WITH_PERFSCHEMA_STORAGE_ENGINE AND WITH_UNIT_TESTS)
  ENABLE_TESTING()
  ADD_SUBDIRECTORY(unittest)
ENDIF()
