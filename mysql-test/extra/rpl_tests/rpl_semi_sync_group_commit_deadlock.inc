################################################################################
# WL#6969    Additional concurrency tests for BGC
#
# Auxiliary file for group commit deadlock
#
#################################################################################
--echo #
--echo # Verify it on AFTER_SYNC wait point
--echo # ==============================================================
--echo # Flush error happens before AFTER_SYNC wait point. So
--echo #
--source include/rpl_connection_master.inc
SET GLOBAL rpl_semi_sync_source_wait_point= AFTER_SYNC;

--echo # Disable diff_table test
--let $enable_diff_table= 0
--echo # Test tables with MyISAM engine when disabling diff_table and
--echo # simulating flush error
--source include/have_myisam.inc
--let $engine = MyISAM
--source extra/rpl_tests/rpl_stress_test.inc

--echo # Enable diff_table test
--let $enable_diff_table= 1
--echo # Test tables with InnoDB engine when enableing diff_table
--echo # and simulating flush error
--let $engine = InnoDB
--source extra/rpl_tests/rpl_stress_test.inc

--echo #
--echo # Verify it on AFTER_COMMIT wait point
--echo # ==============================================================
--echo #
--source include/rpl_connection_master.inc
SET GLOBAL rpl_semi_sync_source_wait_point= AFTER_COMMIT;

--echo # Disable diff_table test
--let $enable_diff_table= 0
--echo # Test tables with MyISAM engine when disabling diff_table and
--echo # simulating flush error
--let $engine = MyISAM
--source extra/rpl_tests/rpl_stress_test.inc

--echo # Enable diff_table test
--let $enable_diff_table= 1
--echo # Test tables with InnoDB engine when enableing diff_table
--echo # and simulating flush error
--let $engine = InnoDB
--source extra/rpl_tests/rpl_stress_test.inc
