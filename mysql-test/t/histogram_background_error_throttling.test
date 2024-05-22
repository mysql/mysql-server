--source include/have_debug.inc

-- echo #
-- echo # Bug#36574298: Background histogram update warnings flood error log
-- echo #

# Note: this test is in a separate file because we want to use
# the loose-debug='+d,background_histogram_update_errors' startup option
# to trigger multiple errors during histogram updates from the background thread.
# If we instead use "SET GLOBAL DEBUG = ..." we run seem to run into some
# flakiness in terms of when this setting is picked up by the background thread.

# Create two tables with automatic histograms and insert values to trigger
# automatic updates from the background thread. This will raise multiple
# ER_BACKGROUND_HISTOGRAM_UPDATE warnings due to the
# loose-debug='+d,background_histogram_update_errors' startup option.
CREATE TABLE t1 (x INT);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON x AUTO UPDATE;
INSERT INTO t1 VALUES (1), (2), (3);

CREATE TABLE t2 (x INT);
ANALYZE TABLE t2 UPDATE HISTOGRAM ON x AUTO UPDATE;
INSERT INTO t2 VALUES (1), (2), (3);

# Wait for a background histogram update error to show up in the error log.
let $wait_condition = SELECT COUNT(*) > 0 FROM performance_schema.error_log WHERE ERROR_CODE = 'MY-015116';
let $wait_timeout = 15;
--source include/wait_condition.inc

# The wait_condition only waits for at least one error to show up.
# We give the system an additional second to synchronize and flush the error logs.
# This should ensure that all of the error messages have been written to the log.
# It would be preferable to use DEBUG_SYNC for this, but it does not seem straightforward
# to get it to work with a background thread.
DO SLEEP(1);
FLUSH ERROR LOGS;

--echo # With throttling of background histogram update warnings we should only
--echo # see a single warning in the error log even though we have raised
--echo # multiple ER_BACKGROUND_HISTOGRAM_UPDATE warnings.
SELECT COUNT(*) AS should_be_one FROM performance_schema.error_log WHERE ERROR_CODE = 'MY-015116';

DROP TABLE t1;
DROP TABLE t2;