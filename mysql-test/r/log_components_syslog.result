#
# WL#9343:  log TNG: log writers
#
# This test verifies the interaction with the 'ix syslog.
# It is disabled by default, to be run manually, so we
# won't spam the test host's syslog on a regular and concurrent
# basis. Besides, we'd have to make an unpleasant amount of
# assumptions about where the logs are located, how they're structured,
# and that we had the privileges to access them.
# This test is therefore provided for convenience, but disabled by
# default, and is not expect to pass on all platforms.
#
# Loading of logging components is now implicit when log_error_services
# is set. Using INSTALL/UNINSTALL is no longer necessary (but possible
# e.g. when you wish to load but not immediately activate a component,
# like we do here for testing).
# Mixing the two (e.g. by using INSTALL and then implicitly loading
# the same component implicitly by setting --log-error-services from
# the command-line on restart as well) results in a warning that we
# failed to load a component (as trying to restore the configuration
# persisted with INSTALL COMPONENT will try to load a component a
# second time that was already loaded implicitly by listing it in
# --log-error-services=...).
#
# on systemd systems: journalctl SYSLOG_IDENTIFIER=mysqld-wl9343
# on syslog  systems: grep mysqld-wl9343 /var/log/messages|cut -d: -f4-
#
SET @@global.log_error_verbosity=3;
FLUSH LOGS;
SET @@session.debug="+d,parser_stmt_to_error_log";
SET @@session.debug="+d,log_error_normalize";
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";
SELECT @@global.log_error_services;
@@global.log_error_services
log_filter_internal; log_sink_internal

SELECT @@syseventlog.tag;
ERROR HY000: Unknown system variable 'syseventlog.tag'
SELECT "*** SWITCHING ERROR LOG TO SYSEVENTLOG ***";
*** SWITCHING ERROR LOG TO SYSEVENTLOG ***
*** SWITCHING ERROR LOG TO SYSEVENTLOG ***
INSTALL COMPONENT "file://component_log_sink_syseventlog";
# default value
SELECT @@global.syseventlog.tag;
@@global.syseventlog.tag

# change tag
SET @@global.syseventlog.tag="wl9343";
SELECT @@global.syseventlog.tag;
@@global.syseventlog.tag
wl9343
# use syslog/eventlog as only sink. this should throw a warning.
SET @@global.log_error_services="log_sink_syseventlog";
Warnings:
Note	4001	None of the log-sinks selected with --log-error-services=... provides a log-parser. The server will not be able to make the previous runs' error-logs available in performance_schema.error_log.
# show that this sink can log to performance_schema.error_log.
SELECT "log_sink_syseventlog-mark";
log_sink_syseventlog-mark
log_sink_syseventlog-mark
SELECT error_code, data
FROM performance_schema.error_log
WHERE DATA LIKE "%log_sink_syseventlog-mark%" LIMIT 1;
error_code	data
MY-010000	Parser saw: SELECT "log_sink_syseventlog-mark"
SET @@global.log_error_services="log_filter_internal; log_sink_internal; log_sink_syseventlog";
SELECT "logging to syslog";
logging to syslog
logging to syslog
# change while component is active
SET @@global.syseventlog.tag="wl9343_2";
SELECT @@global.syseventlog.tag;
@@global.syseventlog.tag
wl9343_2
SET @@global.log_error_services="log_filter_internal; log_sink_internal";
# change while component is loaded, but inactive
SET @@global.syseventlog.tag=DEFAULT;
SELECT @@global.syseventlog.tag;
@@global.syseventlog.tag

SET PERSIST syseventlog.include_pid = OFF;
SET PERSIST syseventlog.facility = "local1";
SET PERSIST syseventlog.tag = "wl11828";
UNINSTALL COMPONENT "file://component_log_sink_syseventlog";
# restart: --no-console --log-error-services=log_filter_internal;log_sink_syseventlog;log_sink_internal --syseventlog.tag=override
SELECT @@syseventlog.include_pid;
@@syseventlog.include_pid
0
SELECT @@syseventlog.facility;
@@syseventlog.facility
local1
SELECT @@syseventlog.tag;
@@syseventlog.tag
wl11828
RESET PERSIST `syseventlog.include_pid`;
RESET PERSIST `syseventlog.facility`;
RESET PERSIST `syseventlog.tag`;

# restart: --no-console --log-error-services=log_filter_internal;log_sink_syseventlog;log_sink_internal --syseventlog.include_pid=OFF --syseventlog.facility=local2 --syseventlog.tag=wl11828_2 
SELECT @@syseventlog.include_pid;
@@syseventlog.include_pid
0
SELECT @@syseventlog.facility;
@@syseventlog.facility
local2
SELECT @@syseventlog.tag;
@@syseventlog.tag
wl11828_2
# restart: --no-console --log-error=LOG_FILE --log-error-services=log_filter_internal;log_sink_syseventlog;log_sink_internal --syseventlog.include_pid=PIKA --syseventlog.facility=localZ --syseventlog.tag=wl11828/2 
SELECT @@syseventlog.include_pid;
@@syseventlog.include_pid
0
SELECT @@syseventlog.facility;
@@syseventlog.facility
daemon
SELECT @@syseventlog.tag;
@@syseventlog.tag

SET GLOBAL log_error_services= default;

SET GLOBAL log_error_services= "log_filter_internal;log_sink_syseventlog";
Warnings:
Note	4001	None of the log-sinks selected with --log-error-services=... provides a log-parser. The server will not be able to make the previous runs' error-logs available in performance_schema.error_log.
UNINSTALL COMPONENT "file://component_log_sink_syseventlog";
ERROR HY000: Unregistration of service implementation 'log_service.log_sink_syseventlog' provided by component 'mysql:log_sink_syseventlog' failed during unloading of the component.
SET GLOBAL log_error_services= default;
SET @@session.debug="-d,parser_stmt_to_error_log_with_system_prio";
SET @@session.debug="-d,parser_stmt_to_error_log";
SET @@session.debug="-d,log_error_normalize";
FLUSH ERROR LOGS;
 Variable 'syseventlog.tag' can't be set to the value of 'wl11828/2'
 Variable 'syseventlog.facility' can't be set to the value of 'localZ'
 option 'syseventlog.include-pid': boolean value 'PIKA' was not recognized. Set to OFF.
 Variable 'syseventlog.tag' can't be set to the value of 'wl11828/2'
 Variable 'syseventlog.facility' can't be set to the value of 'localZ'
 option 'syseventlog.include-pid': boolean value 'PIKA' was not recognized. Set to OFF.
# restart:

###
### done
###
