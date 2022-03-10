#
# WL-13689 Use signal SIGUSR1 to flush logs
#
# Use signal SIGUSR1 to be a light version of SIGHUP.
# The signal is needed to have a reliable
# way to flush logs in mysql. This will include error log,
# general log and slow query log.
#
# This test will cover, that the signal is sent corectly, meaning that:
# When SIGUSR1 is sent:
# Logs are empty afterwards.
# Logs can be written to afterwards.
# Test involves sending SIGUSR1 signal from perl.
#################################################################
###############     ERROR LOG    ################################
# restart:--log-error=MYSQLD_LOG --general_log=0 --general_log_file=MYSQLD_LOG_GENERAL --log_output=FILE --slow_query_log=0 --long_query_time=0 --slow_query_log_file=MYSQLD_LOG_SLOW
#
# Send the log file into a new file that perl can fiddle with.
#
# Check that both files still exists
# CLEAN UP
#################################################################
###############     SLOW LOG     ################################
SET @@global.slow_query_log=1;
SELECT 1;
1
1
#
# Send the log file into a new file that perl can fiddle with.
#
# Check that both files still exists
#
# Set the log output to a table.
# The server must not fail when SIGUSR1 is sent, even though slow log output
# is set to a table (log_output).
#
SET @@global.log_output='TABLE';
SELECT 1;
1
1
#
# SET LOG OUTPUT TO 'NONE'
#
SET @@global.log_output='NONE';
SELECT 1;
1
1
# CLEAN UP
SET @@global.slow_query_log=0;
#################################################################
###############   GENERAL LOG    ################################
SET @@global.general_log=1;
SET @@global.log_output='FILE';
SELECT 1;
1
1
#
# Send the log file into a new file that perl can fiddle with.
#
# Check that both files still exists
#
# Set the log output to a table.
# The server must not fail when SIGUSR1 is sent, even though slow log output 
# is set to a table (log_output).
# let MYSQLD_PIDFILE= `SELECT @@pid_file;`;
SET @@global.log_output='TABLE';
SELECT 1;
1
1
#
# SET LOG OUTPUT TO 'NONE'
#
SET @@global.log_output='NONE';
SELECT 1;
1
1
# CLEAN UP
SET @@global.general_log=0;
TRUNCATE TABLE mysql.general_log;
TRUNCATE TABLE mysql.slow_log;
# Restore default settings
# restart:
