SET GLOBAL log_timestamps=UTC;
SET LOCAL time_zone=UTC;
INSTALL COMPONENT "file://component_log_sink_json";
SET GLOBAL log_error_services="log_filter_internal; log_sink_json";
SET @@session.debug="+d,parser_stmt_to_error_log";
SET @@session.debug="-d,parser_stmt_to_error_log";
SELECT JSON_EXTRACT(data,'$.ts')/1000,
JSON_EXTRACT(data,'$.time')
INTO @my_ts, @my_time
FROM performance_schema.error_log
WHERE LEFT(data,1)="{"
  ORDER BY logged DESC LIMIT 1;
SELECT SUBSTRING(@my_time, 2, LENGTH(@my_time)-2) INTO @my_time;
SELECT TRUNCATE(UNIX_TIMESTAMP(@my_time),3) INTO @my_time;
SELECT IF((@my_ts-@my_time)=0,"SUCCESS","FAILURE");
IF((@my_ts-@my_time)=0,"SUCCESS","FAILURE")
SUCCESS
SET GLOBAL log_error_services= DEFAULT;
UNINSTALL COMPONENT "file://component_log_sink_json";
