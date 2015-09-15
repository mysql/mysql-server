# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# 
# Test session API

MYSQL_ADD_PLUGIN(test_session_in_thd test_session_in_thd.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_session_in_thd")

MYSQL_ADD_PLUGIN(test_x_sessions_init test_x_sessions_init.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_x_sessions_init")

MYSQL_ADD_PLUGIN(test_x_sessions_deinit test_x_sessions_deinit.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_x_sessions_deinit")

MYSQL_ADD_PLUGIN(test_session_detach test_session_detach.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_session_detach")

MYSQL_ADD_PLUGIN(test_session_info test_session_info.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_session_info")

# Test SQL API
MYSQL_ADD_PLUGIN(test_sql_complex test_sql_complex.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_complex")

MYSQL_ADD_PLUGIN(test_sql_all_col_types test_sql_all_col_types.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_all_col_types")

MYSQL_ADD_PLUGIN(test_sql_2_sessions test_sql_2_sessions.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_2_sessions")

MYSQL_ADD_PLUGIN(test_sql_cmds_1 test_sql_cmds_1.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_cmds_1")

MYSQL_ADD_PLUGIN(test_sql_processlist test_sql_processlist.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_processlist")

MYSQL_ADD_PLUGIN(test_sql_shutdown test_sql_shutdown.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_shutdown")

MYSQL_ADD_PLUGIN(test_sql_stored_procedures_functions test_sql_stored_procedures_functions.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_stored_procedures_functions")

MYSQL_ADD_PLUGIN(test_sql_views_triggers test_sql_views_triggers.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_views_triggers")

MYSQL_ADD_PLUGIN(test_sql_commit test_sql_commit.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_commit")

MYSQL_ADD_PLUGIN(test_sql_lock test_sql_lock.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_lock")

MYSQL_ADD_PLUGIN(test_sql_sqlmode test_sql_sqlmode.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_sqlmode")

MYSQL_ADD_PLUGIN(test_sql_replication test_sql_replication.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_replication")

MYSQL_ADD_PLUGIN(test_sql_errors test_sql_errors.cc
  MODULE_ONLY MODULE_OUTPUT_NAME "libtest_sql_errors")
