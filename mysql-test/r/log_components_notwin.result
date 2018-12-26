#
# WL#9343: Logging services: log writers
#
SELECT @@global.log_error_services;
@@global.log_error_services
log_filter_internal; log_sink_internal

#
# WL#9343: Logging services: log writers: syslog
#
SELECT "*** SWITCHING ERROR LOG TO SYSLOG/EVENTLOG ***";
*** SWITCHING ERROR LOG TO SYSLOG/EVENTLOG ***
*** SWITCHING ERROR LOG TO SYSLOG/EVENTLOG ***
INSTALL COMPONENT "file://component_log_sink_syseventlog";
SET @@global.log_error_services="log_filter_internal; log_sink_internal; log_sink_syseventlog";
SELECT "logging to syseventlog";
logging to syseventlog
logging to syseventlog
SET @start_value= @@global.syseventlog.facility;
SET @@global.syseventlog.facility= DEFAULT;
SELECT @@global.syseventlog.facility, @start_value;
@@global.syseventlog.facility	@start_value
daemon	daemon
SET @@global.syseventlog.facility="user";
SELECT @@global.syseventlog.facility;
@@global.syseventlog.facility
user
SET @@global.syseventlog.facility= "daemon";
SELECT @@global.syseventlog.facility;
@@global.syseventlog.facility
daemon
SET @@global.syseventlog.facility= "local0";
SELECT @@global.syseventlog.facility;
@@global.syseventlog.facility
local0
SET @@global.syseventlog.facility= 9;
ERROR 42000: Incorrect argument type to variable 'syseventlog.facility'
SET GLOBAL syseventlog.facility= 0.01;
ERROR 42000: Incorrect argument type to variable 'syseventlog.facility'
SET SESSION syseventlog.facility= "local1";
ERROR HY000: Variable 'syseventlog.facility' is a GLOBAL variable and should be set with SET GLOBAL
SET LOCAL syseventlog.facility= "local1";
ERROR HY000: Variable 'syseventlog.facility' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.syseventlog.facility= "log_local5";
SELECT @@global.syseventlog.facility;
@@global.syseventlog.facility
local5
SET @@global.syseventlog.facility= "LOG_LOCAL7";
SELECT @@global.syseventlog.facility;
@@global.syseventlog.facility
local7
SET @@global.syseventlog.facility= "9";
ERROR 42000: Variable 'syseventlog.facility' can't be set to the value of '9'
SET @@global.syseventlog.facility= "local8";
ERROR 42000: Variable 'syseventlog.facility' can't be set to the value of 'local8'
SET @@global.syseventlog.facility= "";
ERROR 42000: Variable 'syseventlog.facility' can't be set to the value of ''
SET @@global.syseventlog.facility= @start_value;

SET @start_value= @@global.syseventlog.include_pid;
SET @@global.syseventlog.include_pid= DEFAULT;
SELECT @@global.syseventlog.include_pid;
@@global.syseventlog.include_pid
1
SET @@global.syseventlog.include_pid= 0;
SELECT @@global.syseventlog.include_pid;
@@global.syseventlog.include_pid
0
SET @@global.syseventlog.include_pid= 1;
SELECT @@global.syseventlog.include_pid;
@@global.syseventlog.include_pid
1
SET @@global.syseventlog.include_pid= ON;
SELECT @@global.syseventlog.include_pid;
@@global.syseventlog.include_pid
1
SET @@global.syseventlog.include_pid= OFF;
SELECT @@global.syseventlog.include_pid;
@@global.syseventlog.include_pid
0
SET @@global.syseventlog.include_pid= 9;
ERROR 42000: Variable 'syseventlog.include_pid' can't be set to the value of '9'
SELECT @@global.syseventlog.include_pid;
@@global.syseventlog.include_pid
0
SET GLOBAL syseventlog.include_pid= 0.01;
ERROR 42000: Incorrect argument type to variable 'syseventlog.include_pid'
SET SESSION syseventlog.include_pid= 0;
ERROR HY000: Variable 'syseventlog.include_pid' is a GLOBAL variable and should be set with SET GLOBAL
SET LOCAL syseventlog.include_pid= 0;
ERROR HY000: Variable 'syseventlog.include_pid' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.syseventlog.include_pid= @start_value;
SET @start_value= @@global.syseventlog.tag;
SET @@global.syseventlog.tag= DEFAULT;
SELECT @@global.syseventlog.tag;
@@global.syseventlog.tag

SET @@global.syseventlog.tag="production";
SELECT @@global.syseventlog.tag;
@@global.syseventlog.tag
production
SET @@global.syseventlog.tag= "";
SELECT @@global.syseventlog.tag;
@@global.syseventlog.tag

SET @@global.syseventlog.tag= 9;
ERROR 42000: Incorrect argument type to variable 'syseventlog.tag'
SET @@global.syseventlog.tag= "path/like";
ERROR 42000: Variable 'syseventlog.tag' can't be set to the value of 'path/like'
SET GLOBAL syseventlog.tag= 0.01;
ERROR 42000: Incorrect argument type to variable 'syseventlog.tag'
SET SESSION syseventlog.tag= "staging";
ERROR HY000: Variable 'syseventlog.tag' is a GLOBAL variable and should be set with SET GLOBAL
SET LOCAL syseventlog.tag= "staging";
ERROR HY000: Variable 'syseventlog.tag' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.syseventlog.tag= @start_value;
FLUSH ERROR LOGS;
SET @@global.log_error_services=DEFAULT;
UNINSTALL COMPONENT "file://component_log_sink_syseventlog";

###
### done
###
