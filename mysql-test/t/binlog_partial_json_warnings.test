# ==== Implementation ====
#
# This test checks if the correct warnings are issued when the following
# incompatible configurations regarding binlog_row_value_options=PARTIAL_JSON
# are attempted:
#  1) binlog_format=STATEMENT
#  2) binlog_row_image=FULL
#  3) the binlog is disabled
#
# ==== Related Worklog ====
#
# WL#2955 RBR replication of partial JSON updates
#

--source include/force_restart.inc

call mtr.add_suppression("When binlog_format=STATEMENT, the option binlog_row_value_options=PARTIAL_JSON");
call mtr.add_suppression("When binlog_row_image=FULL, the option binlog_row_value_options=PARTIAL_JSON");
call mtr.add_suppression("When the binary log is disabled, the option binlog_row_value_options=PARTIAL_JSON");
call mtr.add_suppression("You need to use --log-bin to make --binlog-format work.");

--echo # binlog-format=stmt is not compatible with partial json
--let $restart_parameters= "restart: --log-bin --binlog-format=statement --binlog-row-value-options=PARTIAL_JSON --binlog-row-image=MINIMAL"
--source include/restart_mysqld.inc

--let $assert_file= $MYSQLTEST_VARDIR/log/mysqld.1.err
--let $assert_count= 1
--let $assert_select= When binlog_format=STATEMENT, the option binlog_row_value_options=PARTIAL_JSON
--let $assert_text= There shall be a warning when binlog_format=STATEMENT
--source include/assert_grep.inc

SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS= PARTIAL_JSON;

--echo # binlog-row-image=full causes partial json to be used only in after image
--let $restart_parameters= "restart: --log-bin --binlog-format=row --binlog-row-value-options=PARTIAL_JSON --binlog-row-image=FULL"
--source include/restart_mysqld.inc

--let $assert_select= When binlog_row_image=FULL, the option binlog_row_value_options=PARTIAL_JSON
--let $assert_text= There shall be a warning when binlog_row_image=FULL
--source include/assert_grep.inc

SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS= PARTIAL_JSON;

--echo # the binlog is disabled
--let $restart_parameters= "restart: --binlog-row-value-options=PARTIAL_JSON --binlog-row-image=MINIMAL --skip-log-bin --skip-log-replica-updates"
--source include/restart_mysqld.inc

--let $assert_select= When the binary log is disabled, the option binlog_row_value_options=PARTIAL_JSON
--let $assert_text= There shall be a warning when when the binary log is disabled
--source include/assert_grep.inc

SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS= PARTIAL_JSON;
