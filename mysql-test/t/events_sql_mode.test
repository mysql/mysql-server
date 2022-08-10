--echo #
--echo # Bug#34134794: SQL_MODE with ANSI_QUOTES affects events created
--echo #               without ANSI_QUOTES
--echo #
--echo

# This test is an example for wrapping LogErr() in a DBUG_EXECUTE_IF()
# to conditionally log debug information to performance_schema.error_log
# (using the error-symbol ER_CONDITIONAL_DEBUG and SYSTEM_LEVEL priority).
#
# This can come in handy in situations like this where we're interested
# in internals that we can't query using SQL, and/or where we want info
# not from the current session, but from a different thread, like an EVENT.
#
# Keep in mind that the error log is just that -- an error log, not a
# debug log, so this method should be used sparingly, and logging of
# debug info should always be disabled by default.


# Needs "debug" -- our logging is wrapped in a DBUG_EXECUTE_IF().
--source include/have_debug.inc
# Let's have performance_schema.error_log to ourselves.
--source include/not_parallel.inc

# Make session and performance_schema.error_log use the same time-zone.
SET @@session.time_zone=@@global.log_timestamps;

# Find error-number for the error symbol ER_CONDITIONAL_DEBUG we'll be using
# (and normalize to symbol in .result file).
--let $err_code= convert_error(ER_CONDITIONAL_DEBUG)
--replace_regex /=.*/=ER_CONDITIONAL_DEBUG/
--eval SET @err_code=CONCAT("MY-",LPAD($err_code,6,"0"));


# Get the timestamp of the latest entry in performance_schema.error_log.
# This will exclude log entries from earlier tests / --repeat=...
#
# (We could also do something like
#
#   SELECT logged INTO @latest_timestamp
#     FROM performance_schema.error_log
#     ORDER BY logged DESC
#     LIMIT 1;
#
# but the status variable "Error_log_latest_write" tracks the exact info
# we want, so the below is the canonical form.)
#
# By way of explanation, the variable provides an integer count of
# microseconds (for lack of a suitable time/data type in SHOW),
# while the column `logged` in performance_schema.error_log uses
# a TIMESTAMP (which is the reason we made sure above that timezones
# will agree). Thus, we divide the microsecond count by 1000000 to
# get a float-value of seconds.
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
  INTO @pfs_errlog_latest
  FROM performance_schema.global_status
 WHERE VARIABLE_NAME LIKE "Error_log_latest_write";

# Set some variables.

# NOTE: As in this .test the interesting things happen in a separate
#       event-thread, we need to set the global variables where normally
#       we would choose the session ones.

# Enable logging of quoted queries on EVENT execution
# (to confirm the type of quoting compliant with the SQL-mode active at
# the event's creation is used).
SET @@global.debug="+d,log_event_query_string";


# The following statements are specific to this test-case.
# If you use this file as a guide to develop your own
# debug-logging-to-error-log test case, your SQL code
# goes below.
#
# You'll probably wish to take a look further below however
# where we read what we logged from the performance_schema
# table, error_log.

# Set SQL-mode.
SET @@SESSION.sql_mode = '';
SET @@GLOBAL.sql_mode = '';

# Create a table that our EVENT can log its sql_mode to.
CREATE SCHEMA s;
CREATE TABLE s.modes (time TIMESTAMP, sess VARCHAR(256), glob VARCHAR (256));

--echo # Create event.
CREATE EVENT s.ev
  ON SCHEDULE EVERY 1 SECOND
  ON COMPLETION PRESERVE
  ENABLE
  DO
    INSERT INTO s.modes VALUES (now(), @@SESSION.sql_mode, @@GLOBAL.sql_mode);

# Wait for event to fire.
let $wait_condition=
  SELECT COUNT(*)>0 FROM s.modes;
--source include/wait_condition.inc

# Show rows in error log that are of ER_CONDITIONAL_DEBUG type
# (and were logged after this .test started).

--echo
--echo # Show debug entry for execution while global ANSI_QUOTES was disabled.
SELECT data
  FROM performance_schema.error_log
 WHERE error_code=@err_code
   AND logged>@pfs_errlog_latest
 ORDER BY logged ASC LIMIT 1;

--echo
--echo # Show any errors that were thrown. We expect none.
SELECT prio,error_code,data
  FROM performance_schema.error_log
 WHERE data LIKE "Event Scheduler: %"
   AND logged>@pfs_errlog_latest
 LIMIT 3;

# Save timestamp.
# Below, we'll only want to see errors thrown after we enabled ANSI_QUOTES.
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
  INTO @pfs_errlog_latest
  FROM performance_schema.global_status
 WHERE VARIABLE_NAME LIKE "Error_log_latest_write";

--echo # Change SQL-mode.
SET @@GLOBAL.sql_mode = 'ANSI_QUOTES';

# Empty the table the event logs to.
TRUNCATE s.modes;

# Wait for event to run after sql_mode changed.
let $wait_condition=
  SELECT COUNT(*)>0 FROM s.modes;
--source include/wait_condition.inc

--echo
--echo # Show debug entry for execution while global ANSI_QUOTES was enabled.
--echo # Without patch for Bug#34134794, we shown sql_mode should be global
--echo # (i.e., it should contain ANSI_QUOTES). With the patch, the sql_mode
--echo # at create-time (without ANSI_QUOTES) should be shown.
SELECT data
  FROM performance_schema.error_log
 WHERE error_code=@err_code
   AND logged>@pfs_errlog_latest
 ORDER BY logged ASC LIMIT 1;

--echo
--echo # Show any errors that were thrown after ANSI_QUOTES was enabled.
--echo #
--echo # Without patch for Bug#34134794, we expect some or all of:
--echo # ER_EVENT_ERROR_DURING_COMPILATION
--echo # ER_EVENT_MESSAGE_STACK
--echo # ER_EVENT_EXECUTION_FAILED
--echo #
--echo # With patch, we expect none of the above.
SELECT prio,error_code,data
  FROM performance_schema.error_log
 WHERE data LIKE "Event Scheduler: %"
   AND logged>@pfs_errlog_latest
 LIMIT 3;


DROP EVENT s.ev;

SET @@SESSION.sql_mode = 'ANSI_QUOTES';

--echo # Create event while ANSI_QUOTES is active in session.
CREATE EVENT s.ev
  ON SCHEDULE EVERY 1 SECOND
  ON COMPLETION PRESERVE
  ENABLE
  DO
    INSERT INTO s.modes VALUES (now(), @@SESSION.sql_mode, @@GLOBAL.sql_mode);


# Empty the table the event logs to.
TRUNCATE s.modes;

# Wait for event to run after sql_mode changed.
let $wait_condition=
  SELECT COUNT(*)>0 FROM s.modes;
--source include/wait_condition.inc

--echo
--echo # Show debug entry for execution of event created with ANSI_QUOTES.
SELECT data
  FROM performance_schema.error_log
 WHERE error_code=@err_code
 ORDER BY logged DESC LIMIT 1;

# Clean up.
SET @@GLOBAL.sql_mode = DEFAULT;
DROP SCHEMA s;

# Turn off debug-logging.
SET @@global.debug="-d,log_event_query_string";

# EOF
