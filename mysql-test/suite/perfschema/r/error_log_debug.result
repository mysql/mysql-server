#
# Various tests to help QA WL#13681 - performance_schema.error_log
#


# Use a sink (and only that sink) that has no pfs.error_log support
INSTALL COMPONENT 'file://component_log_sink_test';
SET @@global.log_error_services="log_filter_internal;log_sink_test";
Warnings:
Note	4002	None of the log-sinks selected in @@global.log_error_services supports writing to the performance schema. The server will not be able to make the current runs' error events available in performance_schema.error_log. To change this, add a log-sink that supports the performance schema to @@global.log_error_services.
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";
SELECT "Hello World";
Hello World
Hello World
# Must have been unable to add to pfs.error_log
SELECT COUNT(*)=0
FROM performance_schema.error_log
WHERE logged>=@startup AND data='Parser saw: SELECT "Hello World"';
COUNT(*)=0
1

# when starting with only a sink that does not support adding to
# pfs.error_log and that has no log-parser, the table should be empty
# after start-up:
# restart: --log-error-services=log_filter_internal;log_sink_test --log-error=LOG_FILE
SELECT COUNT(*)=0
FROM performance_schema.error_log;
COUNT(*)=0
1
SET @@global.log_error_services=DEFAULT;
DELETE FROM mysql.component
WHERE component_urn='file://component_log_sink_test';

# Now add a specific row to the table and show it's there
# a) with error-log file
# restart: --log-error=LOG_FILE
SELECT logged INTO @startup
FROM performance_schema.error_log
WHERE error_code="MY-010116"
 ORDER BY logged DESC LIMIT 1;
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";
SELECT "Hello World";
Hello World
Hello World
SELECT COUNT(*)=1
FROM performance_schema.error_log
WHERE  logged>=@startup AND data='Parser saw: SELECT "Hello World"';
COUNT(*)=1
1
# b) without error-log file
# restart: 
SELECT logged INTO @startup
FROM performance_schema.error_log
WHERE error_code="MY-010116"
 ORDER BY logged DESC LIMIT 1;
# Should fail, we should not see entries from the previous run's
# error log file as we started without reading it.
SELECT error_code,data
FROM performance_schema.error_log
WHERE  logged>=@startup AND data='Parser saw: SELECT "Hello World"';
error_code	data
# But there should be entries in the table from start-up.
SELECT COUNT(*)>1 FROM performance_schema.error_log;
COUNT(*)>1
1
# Now let's re-add our entry.
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";
SELECT "Hello World";
Hello World
Hello World
# And now finally, it should be available in the table.
SELECT error_code,data
FROM performance_schema.error_log
WHERE  logged>=@startup AND data='Parser saw: SELECT "Hello World"';
error_code	data
MY-010000	Parser saw: SELECT "Hello World"

# clean up
SET @@session.debug="-d,parser_stmt_to_error_log_with_system_prio";

# Done!
