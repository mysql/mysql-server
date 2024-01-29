####################################################################
# The aim of this test is to provide mysqld options related to     #
# error logging on the command line and verify their behaviour.    #
#                                                                  #
# Creation Date: 2017-03-02                                        #
# Author: Srikanth B R                                             #
#                                                                  #
####################################################################

##================================================================##
# 1 - Tests to check error log verbosity on command line           #
##================================================================##
--source include/big_test.inc
--source include/have_log_bin.inc

--let $PLUGIN_DIR   = 	 `select @@plugin_dir`

# Setup
CREATE TABLE t1(a int);

# 1.1 Set log_error_verbosity= 1 
#-------------------------------
--let LOG_FILE1= $MYSQLTEST_VARDIR/tmp/test1.err
--let restart_parameters="restart: --log-error=$LOG_FILE1 --log-error-verbosity=1 --binlog-format=statement"
--replace_result $LOG_FILE1 LOG_FILE1
--source include/restart_mysqld.inc

# Run commands which lead to entry of warnings/notes in the error log
# Warning - unsafe statement to binlog
INSERT INTO t1 SELECT FOUND_ROWS();
# Note - Access denied
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con1,localhost,unknown_user,,*NO-ONE*);


# 1.2 Set log_error_verbosity= 2 
#-------------------------------
--let LOG_FILE2= $MYSQLTEST_VARDIR/tmp/test2.err
--let restart_parameters="restart: --log-error=$LOG_FILE2 --log-error-verbosity=2 --binlog-format=statement"
--replace_result $LOG_FILE2 LOG_FILE2
--source include/restart_mysqld.inc

# Run commands which lead to entry of warnings/notes in the error log
# Warning - unsafe statement to binlog
INSERT INTO t1 SELECT FOUND_ROWS();
# Note - Access denied
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con2,localhost,unknown_user,,*NO-ONE*);


# 1.3 Set log_error_verbosity= 3
#-------------------------------
--let LOG_FILE3= $MYSQLTEST_VARDIR/tmp/test3.err
--let restart_parameters="restart: --log-error=$LOG_FILE3 --log-error-verbosity=3 --binlog-format=statement"
--replace_result $LOG_FILE3 LOG_FILE3
--source include/restart_mysqld.inc

# Run commands which lead to entry of warnings/notes in the error log
# Warning - unsafe statement to binlog
INSERT INTO t1 SELECT FOUND_ROWS();
# Note - Access denied
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con3,localhost,unknown_user,,*NO-ONE*);

# Cleanup
DROP TABLE t1;

##================================================================##
# 2 - Tests to check error log verbosity during initialization     #
##================================================================##

# 2.1. Invalid verbosity with --initialize(reverts to
#      minimum - 1)
#-----------------------------------------------------
--let LOG_FILE4 = $MYSQLTEST_VARDIR/tmp/bootstrap1.err
--let CMD1= $MYSQLD --no-defaults --plugin-dir=$PLUGIN_DIR --innodb_dedicated_server=OFF --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLTEST_VARDIR/tmp/new_datadir --log-error=$LOG_FILE4 --log-error-verbosity=-1 2>&1
--exec $CMD1
--force-rmdir $MYSQLTEST_VARDIR/tmp/new_datadir

# 2.2. Set verbosity to 1 explicitly with --initialize
#-----------------------------------------------------
--let LOG_FILE5 = $MYSQLTEST_VARDIR/tmp/bootstrap3.err
--let CMD3 = $MYSQLD --no-defaults --plugin-dir=$PLUGIN_DIR --innodb_dedicated_server=OFF --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLTEST_VARDIR/tmp/new_datadir --log-error=$LOG_FILE5 --log-error-verbosity=1 2>&1
--exec $CMD3
--force-rmdir $MYSQLTEST_VARDIR/tmp/new_datadir

# 2.3. Set verbosity to 2 explicitly with --initialize
#-----------------------------------------------------
--let LOG_FILE6 = $MYSQLTEST_VARDIR/tmp/bootstrap2.err
--let CMD2 = $MYSQLD --no-defaults --plugin-dir=$PLUGIN_DIR --innodb_dedicated_server=OFF --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLTEST_VARDIR/tmp/new_datadir --log-error=$LOG_FILE6 --log-error-verbosity=2 2>&1
--exec $CMD2
--force-rmdir $MYSQLTEST_VARDIR/tmp/new_datadir

# 2.4. Set verbosity to 7 with --initialize (adjusted
#      to 3)
#-----------------------------------------------------
--let LOG_FILE7 = $MYSQLTEST_VARDIR/tmp/bootstrap4.err
--let CMD4 = $MYSQLD --no-defaults --plugin-dir=$PLUGIN_DIR --innodb_dedicated_server=OFF --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLTEST_VARDIR/tmp/new_datadir --log-error=$LOG_FILE7 --log-error-verbosity=7 2>&1
--exec $CMD4
--force-rmdir $MYSQLTEST_VARDIR/tmp/new_datadir

##================================================================##
# 3 - WL#9344:  Logging services: error messages                   #
##================================================================##

# 3.1. Test giving an empty lc-messages-dir and verify that the
#      server makes use of builtin english messages when the
#      language files (errmsg.sys) are not found
#--------------------------------------------------------------
--let LOG_FILE8 = $MYSQLTEST_VARDIR/tmp/wl9344.err
--let $MSG_DIR = $MYSQLTEST_VARDIR/tmp/empty_lcmsgsdir
--mkdir $MSG_DIR
--let restart_parameters="restart: --log-error=$LOG_FILE8 --lc-messages-dir=$MSG_DIR --log-error-verbosity=3"
--replace_result $LOG_FILE8 LOG_FILE8 $MSG_DIR MSG_DIR
--source include/restart_mysqld.inc
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con4,localhost,unknown_user,,*NO-ONE*);
--rmdir $MSG_DIR


# Related:
#
# Bug#26576922: SERVER DOESN'T ALWAYS FALL BACK TO DEFAULT ERROR MESSAGES
#
#      Test giving an empty lc-messages and verify that the
#      server makes use of builtin english messages when the
#      language files (errmsg.sys) are not found
#--------------------------------------------------------------
--let LOG_FILE8B = $MYSQLTEST_VARDIR/tmp/bug26576922.err
--mkdir $MSG_DIR
--let restart_parameters="restart: --log-error=$LOG_FILE8B --lc-messages=invalid_arg1 --lc-time-names=invalid_arg2"
--replace_result $LOG_FILE8B LOG_FILE8B $MSG_DIR MSG_DIR
--source include/restart_mysqld.inc
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con4,localhost,unknown_user,,*NO-ONE*);
--rmdir $MSG_DIR


##================================================================##
# 4 - WL#9323:  Logging services: Improved Error logging in 8.0    #
##================================================================##

# 4.1 Provide invalid value 'abcd' to --log-error-services
# --------------------------------------------------------
# Below check is to see that the server doesn't abort
--let LOG_FILE9= $MYSQLTEST_VARDIR/tmp/test9.err
--let restart_parameters="restart: --log-error=$LOG_FILE9 --log-error-verbosity=3 --log-error-services=abcd "
--replace_result $LOG_FILE9 LOG_FILE9
--source include/restart_mysqld.inc
SELECT @@GLOBAL.LOG_ERROR_SERVICES;
# If we fail with ER_WRONG_VALUE_FOR_VAR here, then the server has started
# despite of an invalid value for log-error-services: it has in fact rejected
# that value and is using the default (as it should), but shows the (illegal)
# value from start-up on SELECT (which it shouldn't):
SET GLOBAL LOG_ERROR_SERVICES=@@GLOBAL.LOG_ERROR_SERVICES;

# Verify that log_error_services is reset to default with a message
# in the error log
--let SEARCH_FILE= $LOG_FILE9
--let SEARCH_PATTERN= \[Warning\].*Cannot set services "abcd" requested in --log-error-services, using defaults
--source include/search_pattern.inc

# 4.2 Provide 'log_sink_json' to --log-error-services without
#     installing the json writer component
#-------------------------------------------------------------
# Below check is to see that the server doesn't abort.
# Component will be implicitly loaded by listing it in
# --log-error-services at start-up. (Before WL#14793,
# the component, not having been INSTALL COMPONENTed,
# would be absent, and setting --log-error-services=...
# would fail, with the variable remaining at its default
# value.)
# This serves as test for WL#14793 FR-1.
--let LOG_FILE10= $MYSQLTEST_VARDIR/tmp/test10.err
--let restart_parameters="restart: --log-error=$LOG_FILE10 --log-error-verbosity=3 --log-error-services=log_sink_json "
--replace_result $LOG_FILE10 LOG_FILE10
--source include/restart_mysqld.inc


# WL#14793 - FR-1.1.1
# Show that the implicit loading of the JSON component from the command-line
# above did not add it to the components table (as explicitly adding it to
# the server with INSTALL COMPONENT would have done). Should produce empty set.
--echo # Empty result expected:
SELECT component_urn from mysql.component WHERE component_urn LIKE "log_%";

--echo # Component was loaded implicitly and made active.
--echo # Trying to UNINSTALL it while it is active will throw an error here.
--error ER_COMPONENTS_UNLOAD_CANT_UNREGISTER_SERVICE
UNINSTALL COMPONENT "file://component_log_sink_json";


# 4.3 Install the json writer component and provide its service
#     to --log-error-services
# -------------------------------------------------------------

# INSTALLing will fail because the component was already implicitly loaded
# by listing it in --log-error-services at start-up (WL#14793).
--error ER_COMPONENTS_CANT_LOAD
INSTALL COMPONENT "file://component_log_sink_json";

# restart clean
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE10"
--replace_result $LOG_FILE10 LOG_FILE10
--source include/restart_mysqld.inc

# WL#14793 - FR-2.1
# Show interaction of explicit and implicit component-loading.
# --------------------------------------------------------------

# INSTALL the component, then restart with log_error_services on the
# command-line. That way, the component will be loaded implicitly before
# our INSTALL could be restored. Then, we'll show some interactions and
# implications of mixing implicit and explicit loading (not because mixing
# the modes is recommended practice, but to show how we are handling it).

--echo # Previously component is not yet in the system table (empty result):
SELECT component_urn FROM mysql.component;
--echo # Component is not active in log_error_services:
SELECT @@global.log_error_services;
--echo # INSTALL the component.
INSTALL COMPONENT "file://component_log_sink_json";
--echo # No issues expected:
SHOW WARNINGS;
--echo # Component should now be in the system table:
SELECT component_urn FROM mysql.component;

--let LOG_FILE11= $MYSQLTEST_VARDIR/tmp/test11.err
--let LOG_FILE11j= $MYSQLTEST_VARDIR/tmp/test11.err.00.json
--let $log_services="log_filter_internal;log_sink_internal;log_sink_json"
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE11 --log-error-services=$log_services --log-error-verbosity=3"
--replace_result $LOG_FILE11 LOG_FILE11
--source include/restart_mysqld.inc
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con5,localhost,unknown_user,,*NO-ONE*);

# command-line is processed before INSTALLs are restored, so component
# was in fact implicitly loaded.
# show that the component is still in the table from being INSTALLed though.
--echo # Component should still be in the system table:
SELECT component_urn FROM mysql.component;

--echo # Component was loaded implicitly and made active.
--echo # Trying to UNINSTALL it while it is active will throw an error here.
--error ER_COMPONENTS_UNLOAD_CANT_UNREGISTER_SERVICE
UNINSTALL COMPONENT "file://component_log_sink_json";

--echo # log_sink_json is not in DEFAULT -- the component will be implicitly unloaded.
SET GLOBAL log_error_services= DEFAULT;

--echo # Show the component is still in the system table however:
SELECT component_urn FROM mysql.component;

--echo # The component is neither implicitly not explicitly loaded now:
--echo # Attempt to UNINSTALL the component:
--echo # The component framework refuses to UNINSTALL what is not in memory.
--error ER_COMPONENTS_UNLOAD_NOT_LOADED
UNINSTALL COMPONENT "file://component_log_sink_json";

--echo # Its entry however is still in the system table.
--echo # The correct way to handle this in production is to restart the server
--echo # while *not* implicitly loading the component by setting
--echo # --log-error-services=... from the command-line or a .cnf file,
--echo # and by RESETting log_error_services if it was SET PERSISTed before.

--echo # Show that we threw ER_COMPONENTS_CANT_LOAD during start-up
--echo # when we tried to INSTALL COMPONENT a component that had already
--echo # been loaded implicitly using --log-error-services=...
--echo # This would be reported as MY-003529 if we had a client session
--echo # attached, but since none is, we'll bounce the message to the
--echo # error log with a ER_SERVER_NO_SESSION_TO_SEND_TO wrapper.
SELECT error_code, data
  FROM performance_schema.error_log
 WHERE data LIKE "%'file://component_log_sink_json'%"
   AND error_code="MY-013129" AND data LIKE "%MY-003529%";


##================================================================##
# 5 - WL#9323: Logging services: log filter (configuration engine) #
##================================================================##

# 5.1 Check that option --dragnet.log_error_filter_rules does
#     not exist when the component isn't loaded
# ------------------------------------------------------------
--let LOG_FILE12 = $MYSQLTEST_VARDIR/tmp/test12.err
--let CMD12 = $MYSQLD --no-defaults --plugin-dir=$PLUGIN_DIR --lc-messages-dir=$MYSQL_SHAREDIR --innodb_dedicated_server=OFF --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLTEST_VARDIR/tmp/new_datadir --log-error=$LOG_FILE12 --dragnet.log_error_filter_rules=a 2>&1
# mysqld aborts if the option is used without loading the component
--error 1
--exec $CMD12
--force-rmdir $MYSQLTEST_VARDIR/tmp/new_datadir

--let SEARCH_FILE= $LOG_FILE12
--let SEARCH_PATTERN= \[ERROR\].*unknown variable 'dragnet.log_error_filter_rules=a'
--source include/search_pattern.inc

# 5.2 a) Pass --dragnet.log-error-filter-rules through command
#        line
#     b) Set dragnet.log_error_filter_rules to include a throttle
#     for number of times ER_ACCESS_DENIED_ERROR appears in the
#     error log and exceed the throttle limit
# -------------------------------------------------------------

--let $log_services="log_filter_internal;log_filter_dragnet;log_sink_internal"
--let LOG_FILE13= $MYSQLTEST_VARDIR/tmp/test13.err
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE13 --log-error-services=$log_services --dragnet.log-error-filter-rules="
--replace_result $LOG_FILE13 LOG_FILE13
--source include/restart_mysqld.inc

--echo # Clean up: The test case for 4.3 left the JSON component installed, which
--echo # therefore got re-added to this instance on start-up. Let's remove it.
UNINSTALL COMPONENT "file://component_log_sink_json";

--echo # Validate parameters passed through command line
SELECT @@log_error_services;
SELECT @@dragnet.log_error_filter_rules;

SET @saved_log_error_verbosity= @@global.log_error_verbosity;

# Enable logging NOTE's to test throttling
SET GLOBAL log_error_verbosity= 3;

# Throttle 'Access denied' messages in error log to three
# entries per hour
SET GLOBAL dragnet.log_error_filter_rules="IF err_code==ER_ACCESS_DENIED_ERROR_WITH_PASSWORD THEN throttle 3/3600.";

--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con6,localhost,unknown_user,,*NO-ONE*);

--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con6,localhost,unknown_user,,*NO-ONE*);

--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con6,localhost,unknown_user,,*NO-ONE*);

--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con6,localhost,unknown_user,,*NO-ONE*);

--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con6,localhost,unknown_user,,*NO-ONE*);

SET GLOBAL log_error_verbosity= @saved_log_error_verbosity;

# 5.3 Check that SET PERSIST retains error log filter rules
# ---------------------------------------------------------
SET PERSIST dragnet.log_error_filter_rules= "IF prio>INFORMATION THEN drop.";

--let LOG_FILE14= $MYSQLTEST_VARDIR/tmp/test14.err
--let $log_services="log_filter_internal;log_filter_dragnet;log_sink_internal"
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE14 --log-error-services=$log_services "

--replace_result $LOG_FILE14 LOG_FILE14
--source include/restart_mysqld.inc

--echo # Verify value of the persisted variable
SELECT @@global.dragnet.log_error_filter_rules;

RESET PERSIST `dragnet.log_error_filter_rules`;

--echo # Remove the dragnet component from the active configuration:
SET GLOBAL log_error_services= default;

--echo # Show that component was implictly unloaded and variable has gone.
--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT @@global.dragnet.log_error_filter_rules;

--let $MYSQLD_DATADIR= `select @@datadir`
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf


# 6 Verify that we use default settings if an invalid rule-set
#   is passed at start-up
# ---------------------------------------------------------

INSTALL COMPONENT "file://component_log_filter_dragnet";

# Invalid filter config given in mysqld command line
--let LOG_FILE15= $MYSQLTEST_VARDIR/tmp/test15.err
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE15 --dragnet.log_error_filter_rules=invalid"

--replace_result $LOG_FILE15 LOG_FILE15
--source include/restart_mysqld.inc

# Component will use default value due to the wrong configuration

SELECT @@global.dragnet.log_error_filter_rules;
# SET @@global.dragnet.log_error_filter_rules= DEFAULT;
# SELECT @@global.dragnet.log_error_filter_rules;

UNINSTALL COMPONENT "file://component_log_filter_dragnet";



##==========================================================##
# 7 - WL#11393: Logging services: log-error-suppression-list #
##==========================================================##

# 7.1 Pass --log-error-suppression-list through command line
# ----------------------------------------------------------

--let LOG_FILE16= $MYSQLTEST_VARDIR/tmp/test16.err
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE16 --log-error-suppression-list=ER_ACCESS_DENIED_ERROR_WITH_PASSWORD "
--replace_result $LOG_FILE16 LOG_FILE16
--source include/restart_mysqld.inc

# Validate parameters passed through command line
SELECT @@global.log_error_suppression_list;

# 7.2 Set --log-error-suppression-list to include a suppression
#     for ER_ACCESS_DENIED_ERROR, then cause that condition to occur.
# -------------------------------------------------------------------

SET @saved_log_error_verbosity= @@global.log_error_verbosity;

# Enable logging NOTE's to test throttling
SET GLOBAL log_error_verbosity= 3;

# Try to log in without proper credentials, cause ACCESS DENIED, filter it
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con6,localhost,unknown_user_filtered,,*NO-ONE*);

# Unset suppression list, logging ACCESS DENIED is now allowed!
SET @@global.log_error_suppression_list="";
SELECT @@global.log_error_suppression_list;
# Try to log in without proper credentials, cause ACCESS DENIED, log it
--replace_result $MASTER_MYSOCK SOURCE_SOCKET $MASTER_MYPORT SOURCE_PORT
--error ER_ACCESS_DENIED_ERROR
connect(con6,localhost,unknown_user_unfiltered,,*NO-ONE*);

SET GLOBAL log_error_verbosity= @saved_log_error_verbosity;
SET @@global.log_error_suppression_list=DEFAULT;


# 7.3 Check that SET PERSIST retains error suppression list
# ---------------------------------------------------------
SET PERSIST log_error_suppression_list= "MY-10000";

--let LOG_FILE17= $MYSQLTEST_VARDIR/tmp/test17.err
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE17 "

--replace_result $LOG_FILE17 LOG_FILE17
--source include/restart_mysqld.inc

# Verify value of the persisted variable
SELECT @@global.log_error_suppression_list;

RESET PERSIST `log_error_suppression_list`;

# Cleanup
SET GLOBAL log_error_suppression_list= DEFAULT;
SET GLOBAL log_error_services= DEFAULT;

--let $MYSQLD_DATADIR= `select @@datadir`
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf


# 7.4 Verify that we use default settings if an invalid
#     suppression list is passed at start-up
# --------------------------------------------------------------

# Invalid filter config given in mysqld command line
--let LOG_FILE18= $MYSQLTEST_VARDIR/tmp/test18.err
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE18 --log_error_suppression_list=invalid"

--replace_result $LOG_FILE18 LOG_FILE18
--source include/restart_mysqld.inc

# Server will use default value due to the wrong configuration
SELECT @@global.log_error_suppression_list;

# Verify that a message is written to the error log
--let SEARCH_FILE= $LOG_FILE18
--let SEARCH_PATTERN= log_error_suppression_list: Could not add suppression rule for code .invalid.
--source include/search_pattern.inc


# WL#11875: Verify that we restore PERSISTed log settings at start-up
#           before flushing buffered events
# --------------------------------------------------------------


INSTALL COMPONENT "file://component_log_sink_json";
INSTALL COMPONENT "file://component_log_filter_dragnet";

--let LOG_FILE19= $MYSQLTEST_VARDIR/tmp/test19.err
--let LOG_FILE19j= $MYSQLTEST_VARDIR/tmp/test19.err.00.json

SET PERSIST dragnet.log_error_filter_rules="IF EXISTS msg THEN drop.";

UNINSTALL COMPONENT "file://component_log_sink_json";
UNINSTALL COMPONENT "file://component_log_filter_dragnet";

--let $log_services="log_filter_dragnet;log_sink_json"
--let restart_parameters="restart: --log-error-services=$log_services --log-error=$LOG_FILE19"

--replace_result $LOG_FILE19 LOG_FILE19
--source include/restart_mysqld.inc

--echo # JSON-19 error log content - begin
--cat_file $LOG_FILE19j
--echo # JSON-19 error log content - end

RESET PERSIST `dragnet.log_error_filter_rules`;

SET GLOBAL log_error_services= DEFAULT;

--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf


# WL#14793 - FR-1.2
# Show that when we PERSIST log_error_services, the server will
# attempt to restore the value at start-up and implicitly load
# any listed logging components that are not yet present.
# (With the current implementation this means "any components
# that are not built in, as persists are restored before InnoDB
# becomes are available, and thus before INSTALLed COMPONENTs
# (which are persisted in an Inno table, mysql.components) have
# been loaded.)
# --------------------------------------------------------------

# Load the filter component so we may configure it. Don't PERSIST yet.
SET GLOBAL log_error_services="log_filter_dragnet;log_sink_json";

# Persist the configuration of a component.
# As an implementation detail, PERSISTs are timestamped and have
# an order in the file that contains them. We specifically persist
# the configuration of the component before we persist the instruction
# to load said component (i.e., the assignment to log_error_services
# which will attempt to implicitly load any missing components).
SET PERSIST dragnet.log_error_filter_rules="IF err_code==ER_BASEDIR_SET_TO THEN SET cat:='meow-wl14793'.";

# Show that we successfully assigned the value.
SELECT @@global.dragnet.log_error_filter_rules;

# Only now do we persist the error logging pipeline.
SET PERSIST log_error_services="log_filter_dragnet;log_sink_json";

# restart the server
--let LOG_FILE20= $MYSQLTEST_VARDIR/tmp/test20.err
--let LOG_FILE20j= $MYSQLTEST_VARDIR/tmp/test20.err.00.json
--let restart_parameters="restart: --no-console --log-error=$LOG_FILE20"
--replace_result $LOG_FILE20 LOG_FILE20
--source include/restart_mysqld.inc

# show that the pipeline is configured as expected
SELECT @@global.log_error_services;

# show that we successfully restored the component-variable
SELECT @@global.dragnet.log_error_filter_rules;

# show that dragnet was implicitly loaded and that its variables are present
SET @@global.dragnet.log_error_filter_rules= DEFAULT;

SELECT JSON_EXTRACT(data,'$.err_symbol'),JSON_EXTRACT(data,'$.cat')
  FROM performance_schema.error_log
 WHERE LEFT(data,1)='{'
   AND JSON_EXTRACT(data,'$.err_symbol')="ER_BASEDIR_SET_TO"
 ORDER BY logged DESC LIMIT 1;

# clean up
RESET PERSIST `log_error_services`;
RESET PERSIST `dragnet.log_error_filter_rules`;
SET GLOBAL dragnet.log_error_filter_rules=DEFAULT;
SET GLOBAL log_error_services= DEFAULT;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf


##===============================================================##
#           Validate error logs for the above cases               #
##===============================================================##

--echo
--echo Reading error logs for validation
--echo ---------------------------------

--perl
   use strict;
   use JSON;

   # Entries matching the below patterns are logged before the
   # option '--log-error-verbosity' is processed. Hence, they
   # need to be ignored during verbosity checks.
   my @ignore_patterns=();

   my $ignore_regex= scalar(@ignore_patterns)?
                     "(". join('|', @ignore_patterns). ")":
                     "";
   # --------
   # TEST 1.1
   # --------
   my $file= $ENV{'LOG_FILE1'} or die("logfile1 not set");
   my $result=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     next if $ignore_regex and $line =~ /$ignore_regex/;

     if ($line =~ /\[Note\]/ or $line =~ /\[Warning\]/) {
       # '[Warning] Changed limits' is shown in-spite of verbosity being 1
       print;
       $result=1;
     }
   }
   close(FILE);
   if($result) {
   print "[ FAIL ] Error log contains WARNING's/NOTE's even with --log-error-verbosity=1\n";
   }
   else {
   print "[ PASS ] Error log does not contain WARNING's/NOTE's with --log-error-verbosity=1\n";
   }

   # --------
   # TEST 1.2
   # --------
   $file= $ENV{'LOG_FILE2'} or die("log file2 not set");
   my $result_note=0;
   my $result_warning=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     next if $ignore_regex and $line =~ /$ignore_regex/;

     if ($line =~ /\[Note\]/) {
       $result_note=1;
       print;
       }
     if ($line =~ /\[Warning\]/) {
       $result_warning=1;
       }
     }
   close(FILE);
   if($result_note) {
   print "[ FAIL ] Error log contains NOTE's even with --log-error-verbosity=2\n";
   }
   elsif (!$result_warning) {
   print "[ FAIL ] Error log does not WARNING's with --log-error-verbosity=2\n";
   }
   else {
   print "[ PASS ] Error log does not contain NOTE's with --log-error-verbosity=2\n";
   }

   # --------
   # TEST 1.3
   # --------
   $file= $ENV{'LOG_FILE3'} or die("log file3 not set");
   $result_note=0;
   $result_warning=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     if ($line =~ /\[Note\]/) {
       $result_note=1;
       }
     elsif ($line =~ /\[Warning\]/) {
       $result_warning=1;
       }
     }
   close(FILE);
   if($result_note and $result_warning) {
   print "[ PASS ] Error log contains NOTE's & WARNING's with --log-error-verbosity=3\n";
   }
   else {
   print "[ FAIL ] Error log does not include WARNING's AND/OR NOTE's with --log-error-verbosity=3\n";
   }

   # --------
   # TEST 2.1
   # --------
   $file= $ENV{'LOG_FILE4'} or die("log file4 not set");
   $result=0;
   my $verbosity_adjust_message_logged= 0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     next if $ignore_regex and $line =~ /$ignore_regex/;

     if ($line =~ /\[Note\]/) {
       print;
       $result=1;
       }
     elsif ($line =~ /\[Warning\]/) {
       if ( $line =~ /option 'log_error_verbosity': value -1 adjusted to 1/) {
         $verbosity_adjust_message_logged=1;
         }
       else {
         $result=1;
         print;
         }
       }
     }
   close(FILE);

   if($result) {
     print "[ FAIL ] Error log contains NOTE's and/or WARNING's with --log-error-verbosity=-1 during initialization\n";
     }
   else {
     if ($verbosity_adjust_message_logged) {
       print "[ FAIL ] Error log does contain a warning that log_error_verbosity is adjusted to 1 from -1\n";
       }
     else {
       # WL#11875 FR1: ("log-filtering applies to early, buffered logging")
       print "[ PASS ] Error log does not contain NOTE's or WARNING's with --log-error-verbosity=-1 (adjusted to 1) during initialization\n";
       }
     }

   # --------
   # TEST 2.2
   # --------
   $file= $ENV{'LOG_FILE5'} or die("log file5 not set");
   $result=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     next if $ignore_regex and $line =~ /$ignore_regex/;

     if ($line =~ /\[Note\]/ or $line =~ /\[Warning\]/) {
       print;
       $result=1;
       }
     }
   close(FILE);
   if($result) {
     print "[ FAIL ] Error log contains WARNING's/NOTE's even with --log-error-verbosity=1  during initialization\n";
     }
   else {
     print "[ PASS ] Error log does not contain WARNING's/NOTE's with --log-error-verbosity=1  during initialization\n";
     }

   # --------
   # TEST 2.3
   # --------
   my $file= $ENV{'LOG_FILE6'} or die("log file6 not set");
   my $result=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     next if $ignore_regex and $line =~ /$ignore_regex/;

     if ($line =~ /\[Note\]/) {
       print;
       $result=1;
       }
     }
   close(FILE);

   if($result) {
     print "[ FAIL ] Error log contains NOTE's even with --log-error-verbosity=2  during initialization\n";
     }
   else {
     print "[ PASS ] Error log does not contain NOTE's with --log-error-verbosity=2  during initialization\n";
     }

   # --------
   # TEST 2.4
   # --------
   $file= $ENV{'LOG_FILE7'} or die("log file7 not set");
   $result=1;
   $verbosity_adjust_message_logged= 0;
   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     if ($line =~ /\[Note\]/) {
       $result=0;
       }
     if ($line =~ /\[Warning\].*'log_error_verbosity': unsigned value 7 adjusted to 3/) {
       $verbosity_adjust_message_logged= 1;
       }
     }
   close(FILE);

   if($result) {
     print "[ FAIL ] Error log does not include NOTE's with --log-error-verbosity=7 (adjusted to 3) during initialization.\n";
     }
   else {
     if ($verbosity_adjust_message_logged) {
       print "[ PASS ] Error log contains NOTE's with --log-error-verbosity=7 (adjusted to 3) during initialization.\n";
       }
     else {
       print "[ FAIL ] Error log does not contain a warning that log_error_verbosity is adjusted to 3 from 7\n";
       }
     }

   # --------
   # TEST 3.1
   # --------
   $file= $ENV{'LOG_FILE8'} or die("log file8 not set");
   $result=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     if ($line =~ /\[Note\] \[[^]]*\] \[[^]]*\] Access denied for user \'unknown_user\'/) {
         $result=1;
       }
     }
   close(FILE);
   if($result) {
     print "[ PASS ] Error log contains a NOTE with compiled-in message 'Access denied' when lc_messages_dir does not contain error messages\n";
     }
   else {
     print "[ FAIL ] Error log does not contain a NOTE with compiled-in message 'Access denied'  when lc_messages_dir does not contain error messages\n";
     }

   # --------
   # TEST 4.3
   # --------
   my $file= $ENV{'LOG_FILE11j'};
   open(FILE,"$file") or die("Unable to open $file $!\n");
   my @log_lines=<FILE>;
   close(FILE);
 
   # Test for validity of the json docs in error log
   my $string = "[\n".join("", @log_lines)."\n]";
   $string =~ s/\}\n\{/\},\n\{/g ;
   my $parsed_json;
   my $success=1;
   $parsed_json = decode_json $string;
   unless ( $parsed_json )
   {
     print "[ FAIL ] Error while parsing the error log as a json document:\n$@\n";
     $success=0;
   }
   if($success)
   {
     print "[ PASS ] Error log successfully parsed as a json document\n";
     for my $item( @$parsed_json ){
        if ( $item->{'msg'} =~ /Access denied for user \'unknown_user\'/) {
          print "[ PASS ] Expected entry found in the json error log: " . $item->{'msg'} . "\n";
        }
        # WL#11875 FR1: ("loaded logging components used for buffered logging")
        # WL#11875 FR2: ("all key/value pairs saved in early, buffered logging")
        if ( $item->{'err_symbol'} =~ /ER_STARTING_AS/ &&
             exists($item->{'buffered'})) {
          print "[ PASS ] Expected entry found in the json error log: " . $item->{'err_symbol'} . "\n";
        }
     }
   }

   # --------
   # TEST 5.1
   # --------
   my $file= $ENV{'LOG_FILE13'} or die("log file13 not set");
   my $result=0;

   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     if ($line =~ /\[Note\] \[[^]]*\] \[[^]]*\] Access denied for user \'unknown_user\'/) {
         $result= $result + 1;
       }
     }
   close(FILE);
   if ($result == 3) {
     print "[ PASS ] Error log messages throttled at three for 'Access denied' messages\n";
   }
   else
   {
     if ($result > 3) {
       print "[ FAIL ] Error log has $result 'Access denied' messages despite being throttled at 3\n";
     }
     else {
       print "[ FAIL ] Error log does not contain expected number of 'Access denied' messages\n";
     }
   }

   # --------
   # TEST 7.1
   # --------
   my $file= $ENV{'LOG_FILE16'} or die("log file16 not set");

   my $result=0;
   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     if ($line =~ /\[Note\] \[[^]]*\] \[[^]]*\] Access denied for user \'unknown_user_filtered\'/) {
         $result= 1;
         print "[ FAIL ] Offender: $line\n";
       }
     }
   close(FILE);
   if ($result == 1) {
     print "[ FAIL ] Error log has $result 'Access denied' message(s) despite of them being in the supression list\n";
   }
   else
   {
     print "[ PASS ] 'Access denied' messages filtered as per suppression list\n";
   }

   my $result=0;
   open(FILE, "$file") or die("Unable to open $file: $!");
   while (<FILE>) {
     my $line = $_;
     if ($line =~ /\[Note\] \[[^]]*\] \[[^]]*\] Access denied for user \'unknown_user_unfiltered\'/) {
         $result= 1;
       }
     }
   close(FILE);
   if ($result == 1) {
     print "[ PASS ] 'Access denied' messages not filtered, as per suppression list.\n";
   }
   else
   {
     print "[ FAIL ] 'Access denied' messages filtered despite of them not being in the supression list!\n";
   }

EOF

# Restart server after this test since it enables binary
# logging and may affect further tests
--source include/force_restart.inc

# Cleanup
--remove_file $LOG_FILE1
--remove_file $LOG_FILE2
--remove_file $LOG_FILE3
--remove_file $LOG_FILE4
--remove_file $LOG_FILE6
--remove_file $LOG_FILE5
--remove_file $LOG_FILE7
--remove_file $LOG_FILE8
--remove_file $LOG_FILE8B
--remove_file $LOG_FILE9
--remove_file $LOG_FILE10
--remove_file $LOG_FILE11
--remove_file $LOG_FILE11j
--remove_file $LOG_FILE12
--remove_file $LOG_FILE13
--remove_file $LOG_FILE14
--remove_file $LOG_FILE15
--remove_file $LOG_FILE16
--remove_file $LOG_FILE17
--remove_file $LOG_FILE18
--remove_file $LOG_FILE19
--remove_file $LOG_FILE19j
--remove_file $LOG_FILE20
--remove_file $LOG_FILE20j
