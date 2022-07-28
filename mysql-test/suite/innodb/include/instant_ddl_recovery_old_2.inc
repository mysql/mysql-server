##########################################################################
# Test script to test RECOVERY of a crashed server in older release with
# table having INSTANT ADD columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo #########
--echo # SETUP #
--echo #########

--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_metadata.inc

eval SELECT * FROM t1_$row_format;

# Make sure nothing is flush and REDO log has entries for INSERT transaction
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;

eval INSERT INTO t1_$row_format VALUES ("r2c1", "r2c2", "r2c3");

--echo # Restart running server.
# It should apply logged entries for INSERT transaction
--source include/restart_mysqld_no_echo.inc

eval SELECT * FROM t1_$row_format;

--source suite/innodb/include/print_instant_metadata.inc

--echo ###########
--echo # CLEANUP #
--echo ###########
eval DROP TABLE t1_$row_format;
