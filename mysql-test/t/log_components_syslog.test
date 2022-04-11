--echo #
--echo # WL#9343:  log TNG: log writers
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

--echo # Loading of logging components is now implicit when log_error_services
--echo # is set. Using INSTALL/UNINSTALL is no longer necessary (but possible
--echo # e.g. when you wish to load but not immediately activate a component,
--echo # like we do here for testing).
--echo # Mixing the two (e.g. by using INSTALL and then implicitly loading
--echo # the same component implicitly by setting --log-error-services from
--echo # the command-line on restart as well) results in a warning that we
--echo # failed to load a component (as trying to restore the configuration
--echo # persisted with INSTALL COMPONENT will try to load a component a
--echo # second time that was already loaded implicitly by listing it in
--echo # --log-error-services=...).
--echo #

--echo # on systemd systems: journalctl SYSLOG_IDENTIFIER=mysqld-wl9343
--echo # on syslog  systems: grep mysqld-wl9343 /var/log/messages|cut -d: -f4-
--echo #

--source include/have_debug.inc
--source include/linux.inc
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

# Component is not present, INSTALL should succeed
SELECT "*** SWITCHING ERROR LOG TO SYSEVENTLOG ***";
INSTALL COMPONENT "file://component_log_sink_syseventlog";

--echo # default value
SELECT @@global.syseventlog.tag;

--echo # change tag
SET @@global.syseventlog.tag="wl9343";
SELECT @@global.syseventlog.tag;

--echo # use syslog/eventlog as only sink. this should throw a warning.
SET @@global.log_error_services="log_sink_syseventlog";
--echo # show that this sink can log to performance_schema.error_log.
SELECT "log_sink_syseventlog-mark";
SELECT error_code, data
  FROM performance_schema.error_log
 WHERE DATA LIKE "%log_sink_syseventlog-mark%" LIMIT 1;

SET @@global.log_error_services="log_filter_internal; log_sink_internal; log_sink_syseventlog";
SELECT "logging to syslog";

--echo # change while component is active
SET @@global.syseventlog.tag="wl9343_2";
SELECT @@global.syseventlog.tag;

SET @@global.log_error_services="log_filter_internal; log_sink_internal";
--echo # change while component is loaded, but inactive
SET @@global.syseventlog.tag=DEFAULT;
SELECT @@global.syseventlog.tag;


# Check that SET PERSIST retains variable values
# ---------------------------------------------------------
SET PERSIST syseventlog.include_pid = OFF;
SET PERSIST syseventlog.facility = "local1";
SET PERSIST syseventlog.tag = "wl11828";

# We uninstall the component we explicitly installed above so we can use
# implicitly loading from the command-line below without the server
# complaining that we're mixing both.
UNINSTALL COMPONENT "file://component_log_sink_syseventlog";

--let $log_services="log_filter_internal;log_sink_syseventlog;log_sink_internal"
--let restart_parameters="restart: --no-console --log-error-services=$log_services --syseventlog.tag=override"
--source include/restart_mysqld.inc

# Verify value of the persisted variable
SELECT @@syseventlog.include_pid;
SELECT @@syseventlog.facility;
SELECT @@syseventlog.tag;

RESET PERSIST `syseventlog.include_pid`;
RESET PERSIST `syseventlog.facility`;
RESET PERSIST `syseventlog.tag`;

--echo


# Verify that we accept arguments passed at start-up

--let $log_services="log_filter_internal;log_sink_syseventlog;log_sink_internal"
--let restart_parameters="restart: --no-console --log-error-services=$log_services --syseventlog.include_pid=OFF --syseventlog.facility=local2 --syseventlog.tag=wl11828_2 "
--source include/restart_mysqld.inc

# Verify value of the persisted variable
SELECT @@syseventlog.include_pid;
SELECT @@syseventlog.facility;
SELECT @@syseventlog.tag;

# Verify that we use default settings if invalid arguments are passed
# at start-up

--let LOG_FILE= $MYSQLTEST_VARDIR/tmp/wl11828.err
--let $log_services="log_filter_internal;log_sink_syseventlog;log_sink_internal"
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE --log-error-services=$log_services --syseventlog.include_pid=PIKA --syseventlog.facility=localZ --syseventlog.tag=wl11828/2 "

--replace_result $LOG_FILE LOG_FILE
--source include/restart_mysqld.inc

# Verify value of the persisted variable
SELECT @@syseventlog.include_pid;
SELECT @@syseventlog.facility;
SELECT @@syseventlog.tag;

# Cleanup
SET GLOBAL log_error_services= default;
--echo

# Implicitly load the component.
SET GLOBAL log_error_services= "log_filter_internal;log_sink_syseventlog";
# Now we have the component loaded and active (because of the implicit
# loading), and we have an entry in mysql.component.
# Unfortunately, they don't belong to each other. The component
# framework detects this:
--error ER_COMPONENTS_UNLOAD_CANT_UNREGISTER_SERVICE
UNINSTALL COMPONENT "file://component_log_sink_syseventlog";
# Unload the implicitly loaded component.
SET GLOBAL log_error_services= default;

SET @@session.debug="-d,parser_stmt_to_error_log_with_system_prio";
SET @@session.debug="-d,parser_stmt_to_error_log";
SET @@session.debug="-d,log_error_normalize";
## WL#9651
# SET @@global.log_error_filter_rules=@save_filters;

FLUSH ERROR LOGS;

--perl
   use strict;
   use JSON;

   my $file= $ENV{'LOG_FILE'} or die("logfile not set");
   my $result=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;

     if ($line =~ /syseventlog\./) {
       $line =~ s/.*\]//;
       print $line;
     }
   }
   close(FILE);
EOF

--remove_file $LOG_FILE

--let restart_parameters="restart:"
--source include/restart_mysqld.inc

--echo
--echo ###
--echo ### done
--echo ###
