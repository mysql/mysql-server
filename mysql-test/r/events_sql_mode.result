#
# Bug#34134794: SQL_MODE with ANSI_QUOTES affects events created
#               without ANSI_QUOTES
#

SET @@session.time_zone=@@global.log_timestamps;
SET @err_code=ER_CONDITIONAL_DEBUG;
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
INTO @pfs_errlog_latest
FROM performance_schema.global_status
WHERE VARIABLE_NAME LIKE "Error_log_latest_write";
SET @@global.debug="+d,log_event_query_string";
SET @@SESSION.sql_mode = '';
SET @@GLOBAL.sql_mode = '';
CREATE SCHEMA s;
CREATE TABLE s.modes (time TIMESTAMP, sess VARCHAR(256), glob VARCHAR (256));
# Create event.
CREATE EVENT s.ev
ON SCHEDULE EVERY 1 SECOND
ON COMPLETION PRESERVE
ENABLE
DO
INSERT INTO s.modes VALUES (now(), @@SESSION.sql_mode, @@GLOBAL.sql_mode);

# Show debug entry for execution while global ANSI_QUOTES was disabled.
SELECT data
FROM performance_schema.error_log
WHERE error_code=@err_code
AND logged>@pfs_errlog_latest
ORDER BY logged ASC LIMIT 1;
data
Query string to be compiled: ""/"" >>CREATE PROCEDURE `ev`() SQL SECURITY INVOKER INSERT INTO s.modes VALUES (now(), @@SESSION.sql_mode, @@GLOBAL.sql_mode)<< 

# Show any errors that were thrown. We expect none.
SELECT prio,error_code,data
FROM performance_schema.error_log
WHERE data LIKE "Event Scheduler: %"
   AND logged>@pfs_errlog_latest
LIMIT 3;
prio	error_code	data
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
INTO @pfs_errlog_latest
FROM performance_schema.global_status
WHERE VARIABLE_NAME LIKE "Error_log_latest_write";
# Change SQL-mode.
SET @@GLOBAL.sql_mode = 'ANSI_QUOTES';
TRUNCATE s.modes;

# Show debug entry for execution while global ANSI_QUOTES was enabled.
# Without patch for Bug#34134794, we shown sql_mode should be global
# (i.e., it should contain ANSI_QUOTES). With the patch, the sql_mode
# at create-time (without ANSI_QUOTES) should be shown.
SELECT data
FROM performance_schema.error_log
WHERE error_code=@err_code
AND logged>@pfs_errlog_latest
ORDER BY logged ASC LIMIT 1;
data
Query string to be compiled: ""/"" >>CREATE PROCEDURE `ev`() SQL SECURITY INVOKER INSERT INTO s.modes VALUES (now(), @@SESSION.sql_mode, @@GLOBAL.sql_mode)<< 

# Show any errors that were thrown after ANSI_QUOTES was enabled.
#
# Without patch for Bug#34134794, we expect some or all of:
# ER_EVENT_ERROR_DURING_COMPILATION
# ER_EVENT_MESSAGE_STACK
# ER_EVENT_EXECUTION_FAILED
#
# With patch, we expect none of the above.
SELECT prio,error_code,data
FROM performance_schema.error_log
WHERE data LIKE "Event Scheduler: %"
   AND logged>@pfs_errlog_latest
LIMIT 3;
prio	error_code	data
DROP EVENT s.ev;
SET @@SESSION.sql_mode = 'ANSI_QUOTES';
# Create event while ANSI_QUOTES is active in session.
CREATE EVENT s.ev
ON SCHEDULE EVERY 1 SECOND
ON COMPLETION PRESERVE
ENABLE
DO
INSERT INTO s.modes VALUES (now(), @@SESSION.sql_mode, @@GLOBAL.sql_mode);
TRUNCATE s.modes;

# Show debug entry for execution of event created with ANSI_QUOTES.
SELECT data
FROM performance_schema.error_log
WHERE error_code=@err_code
ORDER BY logged DESC LIMIT 1;
data
Query string to be compiled: "ANSI_QUOTES"/"ANSI_QUOTES" >>CREATE PROCEDURE "ev"() SQL SECURITY INVOKER INSERT INTO s.modes VALUES (now(), @@SESSION.sql_mode, @@GLOBAL.sql_mode)<< 
SET @@GLOBAL.sql_mode = DEFAULT;
DROP SCHEMA s;
SET @@global.debug="-d,log_event_query_string";
