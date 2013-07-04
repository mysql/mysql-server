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
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_SYNC;

--echo # disable diff_table test
--let $enable_diff_table= 0
--echo # simulate flush error and session attach error at the same time
--let $flush_error_session_start= 1
--let $flush_error_session_end= 6
--let $attach_error_session_start= 6
--let $attach_error_session_end= $connections
--echo # Test tables with InnoDB engine when disabling diff_table and
--echo # simulating flush error and session attach error
--let $engine = InnoDB
--source extra/rpl_tests/rpl_stress_test.inc

--echo # Test tables with MyISAM engine when disabling diff_table and
--echo # simulating flush error and session attach error
--let $engine = MyISAM
--source extra/rpl_tests/rpl_stress_test.inc

--echo # enable diff_table test
--let $enable_diff_table= 1
--echo # simulate flush error only
--let $flush_error_session_start= 1
--let $flush_error_session_end= $connections
--let $attach_error_session_start= $connections
--let $attach_error_session_end= $connections
--echo # Test tables with InnoDB engine when enableing diff_table
--echo # and simulating flush error
--let $engine = InnoDB
--source extra/rpl_tests/rpl_stress_test.inc

--echo #
--echo # Verify it on AFTER_COMMIT wait point
--echo # ==============================================================
--echo #
--source include/rpl_connection_master.inc
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_COMMIT;

--echo # disable diff_table test
--let $enable_diff_table= 0
--echo # simulate flush error and session attach error at the same time
--let $flush_error_session_start= 1
--let $flush_error_session_end= 6
--let $attach_error_session_start= 6
--let $attach_error_session_end= $connections
--echo # Test tables with InnoDB engine when disabling diff_table and
--echo # simulating flush error and session attach error
--let $engine = InnoDB
--source extra/rpl_tests/rpl_stress_test.inc

--echo # Test tables with MyISAM engine when disabling diff_table and
--echo # simulating flush error and session attach error
--let $engine = MyISAM
--source extra/rpl_tests/rpl_stress_test.inc

--echo # enable diff_table test
--let $enable_diff_table= 1
--echo # simulate flush error only
--let $flush_error_session_start= 1
--let $flush_error_session_end= $connections
--let $attach_error_session_start= $connections
--let $attach_error_session_end= $connections
--echo # Test tables with InnoDB engine when enableing diff_table
--echo # and simulating flush error
--let $engine = InnoDB
--source extra/rpl_tests/rpl_stress_test.inc
