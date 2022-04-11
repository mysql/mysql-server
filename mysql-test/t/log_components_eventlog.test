--echo #
--echo # WL#9343:  log TNG: log writers
--echo #

--echo #
--echo # This test verifies the interaction with the 'ix syslog.
--echo # It is disabled by default, to be run manually, so we
--echo # won't spam the test host's syslog on a regular and concurrent
--echo # basis. Besides, we'd have to make an unpleasant amount of
--echo # assumptions about where the logs are located, how they're structured,
--echo # and that we had the privileges to access them.
--echo # This test is therefore provided for convenience, but disabled by
--echo # default, and is not expect to pass on all platforms.
--echo #

--source include/have_debug.inc
--source include/windows.inc
--source include/have_log_component.inc

let GREP_START=`SELECT DATE_FORMAT(CONVERT_TZ(SYSDATE(6),'SYSTEM','UTC'),'%Y%m%d%H%i%s%f');`;

SET @@global.log_error_verbosity=3;

let $log_error_= `SELECT @@GLOBAL.log_error`;
if($log_error_ == "stderr")
{
  let $log_error_ = $MYSQLTEST_VARDIR/log/mysqld.1.err;
}
FLUSH LOGS;

# Send parse-trace to error log; first one with a current timestamp
# to compare against our GREP_START defined above.
SET @@session.debug="+d,parser_stmt_to_error_log";
# Now normalize timestamp and thread_ID on all following lines,
# for less hassle with --regex_replace in test files.  Once
# WL#9651 goes live, we can use that to achieve the same thing.
SET @@session.debug="+d,log_error_normalize";

# Log parser statement with System label too.
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";

SELECT @@global.log_error_services;
--echo

# Component not loaded, variable not present
--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT @@syseventlog.tag;

SELECT "*** SWITCHING ERROR LOG TO SYSEVENTLOG ***";
INSTALL COMPONENT "file://component_log_sink_syseventlog";

--echo # default value
SELECT @@global.syseventlog.tag;

--echo # change tag
SET @@global.syseventlog.tag="wl9343";
SELECT @@global.syseventlog.tag;

SET @@global.log_error_services="log_filter_internal; log_sink_internal; log_sink_syseventlog";
SELECT "logging to syslog";

--echo # change while component is active
SET @@global.syseventlog.tag="wl9343_2";
SELECT @@global.syseventlog.tag;

SET @@global.log_error_services="log_filter_internal; log_sink_internal";
--echo # change while component is loaded, but inactive
SET @@global.syseventlog.tag=DEFAULT;
SELECT @@global.syseventlog.tag;

--error ER_UNKNOWN_SYSTEM_VARIABLE
SET GLOBAL syseventlog.include_pid = OFF;
--error ER_UNKNOWN_SYSTEM_VARIABLE
SET GLOBAL syseventlog.facility = "local1";

# Check that SET PERSIST retains variable values
# ---------------------------------------------------------
SET PERSIST syseventlog.tag = "wl11828";

# Now that we've shown explicit loading with INSTALL above, we'll UNINSTALL
# use implicit loading from the command-line.
SET @@global.log_error_services=DEFAULT;
UNINSTALL COMPONENT "file://component_log_sink_syseventlog";

--let $log_services="log_filter_internal;log_sink_syseventlog;log_sink_internal"
--let restart_parameters="restart: --no-console --log-error-services=$log_services "
--source include/restart_mysqld.inc

# Verify value of the persisted variable
SELECT @@syseventlog.tag;

RESET PERSIST `syseventlog.tag`;

--echo


# Verify that we accept arguments passed at start-up

--let $log_services="log_filter_internal;log_sink_syseventlog;log_sink_internal"
--let restart_parameters="restart: --no-console --log-error-services=$log_services --syseventlog.tag=wl11828_2 "
--source include/restart_mysqld.inc

# Verify value of the persisted variable
SELECT @@syseventlog.tag;

# Verify that we use default settings if invalid arguments are passed
# at start-up

--let $log_services="log_filter_internal;log_sink_syseventlog;log_sink_internal"
--let restart_parameters="restart: --no-console --log-error-services=$log_services --syseventlog.tag=wl11828/2 "
--source include/restart_mysqld.inc

# Verify value of the persisted variable
SELECT @@syseventlog.tag;

# Cleanup
SET GLOBAL log_error_services= default;
--echo

--echo # cleanup

SET @@session.debug="-d,parser_stmt_to_error_log_with_system_prio";
SET @@session.debug="-d,parser_stmt_to_error_log";
SET @@session.debug="-d,log_error_normalize";
## WL#9651
# SET @@global.log_error_filter_rules=@save_filters;

FLUSH ERROR LOGS;

let $restart_parameters = restart:;
--source include/restart_mysqld_no_echo.inc


--echo
--echo ###
--echo ### done
--echo ###
