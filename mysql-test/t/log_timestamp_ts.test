
# we need to be able to emit to the error log at will
--source include/have_debug.inc

# let's make sure we have a shared timezone
SET GLOBAL log_timestamps=UTC;
SET LOCAL time_zone=UTC;

# set up JSON logging
INSTALL COMPONENT "file://component_log_sink_json";
SET GLOBAL log_error_services="log_filter_internal; log_sink_json";

# log something in JSON
SET @@session.debug="+d,parser_stmt_to_error_log";
SET @@session.debug="-d,parser_stmt_to_error_log";

# SELECT that line, and compare both timestamps.
SELECT JSON_EXTRACT(data,'$.ts')/1000,
       JSON_EXTRACT(data,'$.time')
  INTO @my_ts, @my_time
  FROM performance_schema.error_log
  WHERE LEFT(data,1)="{"
  ORDER BY logged DESC LIMIT 1;

# remove quotation
--disable_warnings
SELECT SUBSTRING(@my_time, 2, LENGTH(@my_time)-2) INTO @my_time;
# adjust to 3 decimals to match the milli-second timestamp
SELECT TRUNCATE(UNIX_TIMESTAMP(@my_time),3) INTO @my_time;
--enable_warnings

# The difference should be 0.
# SELECT FROM_UNIXTIME(@my_ts),FROM_UNIXTIME(@my_time);
SELECT IF((@my_ts-@my_time)=0,"SUCCESS","FAILURE");

# clean up
SET GLOBAL log_error_services= DEFAULT;
UNINSTALL COMPONENT "file://component_log_sink_json";
