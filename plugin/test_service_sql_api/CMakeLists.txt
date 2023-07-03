# Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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
# 
# Test session API

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(test_session_in_thd
  test_session_in_thd.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_session_in_thd"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_x_sessions_init
  test_x_sessions_init.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_x_sessions_init"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_x_sessions_deinit
  test_x_sessions_deinit.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_x_sessions_deinit"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_session_detach
  test_session_detach.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_session_detach"
  TEST_ONLY
  )

MYSQL_ADD_PLUGIN(test_session_info
  test_session_info.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_session_info"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_session_attach
  test_session_attach.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_session_attach"
  TEST_ONLY
  )

# MYSQL_ADD_PLUGIN may have decided not to build it.
IF(WIN32 AND TARGET test_session_attach)
  MY_TARGET_LINK_OPTIONS(test_session_attach
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/test_session_attach.def"
    )
ENDIF()

# Test SQL API
MYSQL_ADD_PLUGIN(test_sql_complex
  test_sql_complex.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_complex"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_all_col_types
  test_sql_all_col_types.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_all_col_types"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_2_sessions
  test_sql_2_sessions.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_2_sessions"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_cmds_1
  test_sql_cmds_1.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_cmds_1"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_processlist
  test_sql_processlist.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_processlist"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_shutdown
  test_sql_shutdown.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_shutdown"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_sleep_is_connected
  test_sql_sleep_is_connected.cc
  TEST_ONLY
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_sleep_is_connected"
  )
MYSQL_ADD_PLUGIN(test_sql_stored_procedures_functions
  test_sql_stored_procedures_functions.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_stored_procedures_functions"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_views_triggers
  test_sql_views_triggers.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_views_triggers"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_commit
  test_sql_commit.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_commit"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_lock
  test_sql_lock.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_lock"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_sqlmode
  test_sql_sqlmode.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_sqlmode"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_replication
  test_sql_replication.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_replication"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_errors
  test_sql_errors.cc
  TEST_ONLY
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_errors"
  )
MYSQL_ADD_PLUGIN(test_sql_stmt
  test_sql_stmt.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_stmt"
  TEST_ONLY
  )
MYSQL_ADD_PLUGIN(test_sql_reset_connection
  test_sql_reset_connection.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "libtest_sql_reset_connection"
  TEST_ONLY
  )
