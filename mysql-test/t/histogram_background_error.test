--source include/have_debug.inc

-- echo #
-- echo # Errors during background histogram updates should show up in the error log.
-- echo #

# Note: this test is in a separate file because we want to use
# the loose-debug='+d,update_histograms_failure' startup option
# to trigger an error during histogram updates from the background thread.
# If we instead use "SET GLOBAL DEBUG = ..." we run seem to run into some
# flakiness in terms of when this setting is picked up by the background thread.

CREATE TABLE t1 (x INT);
SET SESSION DEBUG = '-d,update_histograms_failure';
ANALYZE TABLE t1 UPDATE HISTOGRAM ON x AUTO UPDATE;
--source include/save_error_log_position.inc
INSERT INTO t1 VALUES (1), (2), (3);

-- echo # Wait for the expected error code to show up in the error log.
let $wait_condition = SELECT COUNT(*) > 0 FROM performance_schema.error_log WHERE error_code = 'MY-015116';
let $wait_timeout = 15;
--source include/wait_condition.inc

-- echo # Verify that the error code comes from the background histogram update.
--let $error_pattern = Background histogram update on test.t1: Unable to build histogram statistics for column 'field' in table 'schema'.'table'
--source include/assert_error_log.inc

-- echo #
-- echo # Bug#38983545: Stale diagnostic area conditions should not be re-logged
-- echo #               after subsequent background histogram updates.
-- echo #

# Disable the debug point so subsequent background histogram updates succeed.
SET GLOBAL DEBUG = '-d,update_histograms_failure';

# Give time for background thread to pick up new debug settings
-- sleep 5

# Save the current histogram last-updated timestamp.
let $last_updated = `SELECT histogram->>'$."last-updated"' FROM INFORMATION_SCHEMA.COLUMN_STATISTICS WHERE table_name = 't1'`;

--source include/save_error_log_position.inc

# Insert more data to trigger another background histogram update.
INSERT INTO t1 VALUES (4), (5), (6);

-- echo # Wait for the background histogram update to complete successfully.
let $wait_condition = SELECT histogram->>'\\$."last-updated"' <> '$last_updated' FROM INFORMATION_SCHEMA.COLUMN_STATISTICS WHERE table_name = 't1';
let $wait_timeout = 15;
--source include/wait_condition.inc

-- echo # Verify that no stale errors were re-logged.
--let $error_pattern = NONE
--source include/assert_error_log.inc

# Restore the debug point for MTR's internal check.
SET GLOBAL DEBUG = '+d,update_histograms_failure';

DROP TABLE t1;
