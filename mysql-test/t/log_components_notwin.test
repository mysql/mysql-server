--echo #
--echo # WL#9343: Logging services: log writers
--echo #

# This tests whether we can load and unload the systemd log writer server.
# It also tests whether the log_syslog* system variables behave as expected.
# This used to be tested in the sys_vars suite, but since the behavior now
# depends on what service is loaded, if any, we test it here instead.
# In fact, we'll have to test it for each service that provides these.

--source include/linux.inc
--source include/have_log_component.inc

SELECT @@global.log_error_services;
--echo

--echo #
--echo # WL#9343: Logging services: log writers: syslog
--echo #

SELECT "*** SWITCHING ERROR LOG TO SYSLOG/EVENTLOG ***";
INSTALL COMPONENT "file://component_log_sink_syseventlog";
SET @@global.log_error_services="log_filter_internal; log_sink_internal; log_sink_syseventlog";
SELECT "logging to syseventlog";


################## mysql-test\t\log_syslog_facility_basic.test ################
#                                                                             #
# Variable Name: log_syslog_facility                                          #
# Scope: Global                                                               #
# Access Type: Dynamic                                                        #
# Data Type: string                                                           #
#                                                                             #
#                                                                             #
# Creation Date: 2014-02-14                                                   #
# Author : Azundris (tnuernbe)                                                #
#                                                                             #
# Description:Test Cases of Dynamic System Variable                           #
#             log_syslog_facility                                             #
#             that checks the behavior of this variable in the following ways #
#              * Value Check                                                  #
#              * Scope Check                                                  #
#                                                                             #
# Reference:                                                                  #
#  http://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html        #
#                                                                             #
###############################################################################

SET @start_value= @@global.syseventlog.facility;

SET @@global.syseventlog.facility= DEFAULT;
SELECT @@global.syseventlog.facility, @start_value;

SET @@global.syseventlog.facility="user";
SELECT @@global.syseventlog.facility;

SET @@global.syseventlog.facility= "daemon";
SELECT @@global.syseventlog.facility;

SET @@global.syseventlog.facility= "local0";
SELECT @@global.syseventlog.facility;

--error ER_WRONG_TYPE_FOR_VAR
SET @@global.syseventlog.facility= 9;

--error ER_WRONG_TYPE_FOR_VAR
SET GLOBAL syseventlog.facility= 0.01;

--error ER_GLOBAL_VARIABLE
SET SESSION syseventlog.facility= "local1";

--error ER_GLOBAL_VARIABLE
SET LOCAL syseventlog.facility= "local1";

SET @@global.syseventlog.facility= "log_local5";
SELECT @@global.syseventlog.facility;
SET @@global.syseventlog.facility= "LOG_LOCAL7";
SELECT @@global.syseventlog.facility;
--error ER_WRONG_VALUE_FOR_VAR
SET @@global.syseventlog.facility= "9";
--error ER_WRONG_VALUE_FOR_VAR
SET @@global.syseventlog.facility= "local8";
--error ER_WRONG_VALUE_FOR_VAR
SET @@global.syseventlog.facility= "";

SET @@global.syseventlog.facility= @start_value;
--echo



################## mysql-test\t\log_syslog_include_pid_basic.test #############
#                                                                             #
# Variable Name: log_syslog_include_pid                                       #
# Scope: Global                                                               #
# Access Type: Dynamic                                                        #
# Data Type: bool                                                             #
#                                                                             #
#                                                                             #
# Creation Date: 2014-02-14                                                   #
# Author : Azundris (tnuernbe)                                                #
#                                                                             #
# Description:Test Cases of Dynamic System Variable                           #
#             log_syslog_include_pid                                          #
#             that checks the behavior of this variable in the following ways #
#              * Value Check                                                  #
#              * Scope Check                                                  #
#                                                                             #
# Reference:                                                                  #
#  http://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html        #
#                                                                             #
###############################################################################

SET @start_value= @@global.syseventlog.include_pid;

SET @@global.syseventlog.include_pid= DEFAULT;
SELECT @@global.syseventlog.include_pid;

SET @@global.syseventlog.include_pid= 0;
SELECT @@global.syseventlog.include_pid;

SET @@global.syseventlog.include_pid= 1;
SELECT @@global.syseventlog.include_pid;

SET @@global.syseventlog.include_pid= ON;
SELECT @@global.syseventlog.include_pid;

SET @@global.syseventlog.include_pid= OFF;
SELECT @@global.syseventlog.include_pid;

--error ER_WRONG_VALUE_FOR_VAR
SET @@global.syseventlog.include_pid= 9;
SELECT @@global.syseventlog.include_pid;

--error ER_WRONG_TYPE_FOR_VAR
SET GLOBAL syseventlog.include_pid= 0.01;

--error ER_GLOBAL_VARIABLE
SET SESSION syseventlog.include_pid= 0;

--error ER_GLOBAL_VARIABLE
SET LOCAL syseventlog.include_pid= 0;

SET @@global.syseventlog.include_pid= @start_value;



################## mysql-test\t\syseventlog.tag_basic.test #####################
#                                                                             #
# Variable Name: syseventlog.tag                                               #
# Scope: Global                                                               #
# Access Type: Dynamic                                                        #
# Data Type: string                                                           #
#                                                                             #
#                                                                             #
# Creation Date: 2014-02-14                                                   #
# Author : Azundris (tnuernbe)                                                #
#                                                                             #
# Description:Test Cases of Dynamic System Variable                           #
#             syseventlog.tag                                                  #
#             that checks the behavior of this variable in the following ways #
#              * Value Check                                                  #
#              * Scope Check                                                  #
#                                                                             #
# Reference:                                                                  #
#  http://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html        #
#                                                                             #
###############################################################################

SET @start_value= @@global.syseventlog.tag;

SET @@global.syseventlog.tag= DEFAULT;
SELECT @@global.syseventlog.tag;

SET @@global.syseventlog.tag="production";
SELECT @@global.syseventlog.tag;

SET @@global.syseventlog.tag= "";
SELECT @@global.syseventlog.tag;

--error ER_WRONG_TYPE_FOR_VAR
SET @@global.syseventlog.tag= 9;

--error ER_WRONG_VALUE_FOR_VAR
SET @@global.syseventlog.tag= "path/like";

--error ER_WRONG_TYPE_FOR_VAR
SET GLOBAL syseventlog.tag= 0.01;

--error ER_GLOBAL_VARIABLE
SET SESSION syseventlog.tag= "staging";

--error ER_GLOBAL_VARIABLE
SET LOCAL syseventlog.tag= "staging";

SET @@global.syseventlog.tag= @start_value;



FLUSH ERROR LOGS;
SET @@global.log_error_services=DEFAULT;
UNINSTALL COMPONENT "file://component_log_sink_syseventlog";


--echo
--echo ###
--echo ### done
--echo ###
