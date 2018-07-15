# This test uses debug insertion points.
--source include/have_debug.inc

# Valgrind would complain about memory leaks when we crash on purpose.
--source include/not_valgrind.inc

# Only run these tests with 16k page size.
--source include/have_innodb_16k.inc

--disable_query_log
let $truncate_frequency_start_value = `SELECT @@global.innodb_purge_rseg_truncate_frequency`;
let $rollback_segments_start_value = `SELECT @@global.innodb_rollback_segments`;
let $max_undo_log_size_start_value = `SELECT @@global.innodb_max_undo_log_size`;
let $log_error_verbosity_start_value = `SELECT @@global.log_error_verbosity`;
let $undo_log_truncate_start_value = `SELECT @@global.innodb_undo_log_truncate`;
--enable_query_log

SET @@global.innodb_undo_log_truncate = 1;
SET @@global.innodb_purge_rseg_truncate_frequency = 1;
SET @@global.innodb_rollback_segments = 3;
SET @@global.innodb_max_undo_log_size = 11534336;
SET @@global.log_error_verbosity = 3;

--echo #
--echo # Create procedure to populate the test-bed for Undo Truncate Recover tests
--echo #
let SEARCH_FILE = $MYSQLTEST_VARDIR/log/mysqld.1.err;
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect

delimiter |;
CREATE PROCEDURE populate_t1()
BEGIN
        DECLARE i INT DEFAULT 1;
        while (i <= 40000) DO
                insert into t1 values (i, 'a', 'b');
                SET i = i + 1;
        END WHILE;
END |
delimiter ;|

--disable_query_log
call mtr.add_suppression("Header page consists of zero bytes in datafile");
call mtr.add_suppression("Datafile '.*' is corrupted. Cannot determine the space ID from the first 64 pages");
call mtr.add_suppression("Ignoring.*undo_002.*invalid tablespace ID in the header");
call mtr.add_suppression("Page 0 in the doublewrite buffer is not within space bounds");
call mtr.add_suppression("Doublewrite page .* could not be restored");
--enable_query_log

