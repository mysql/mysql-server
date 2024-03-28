# ==== Purpose ====
#
# Starts the server expecting it to crash.
#
# The preconditions are that:
# - the server is not currently running (the client is not connected)
# - the $_expect_file_name = $MYSQLTEST_VARDIR/tmp/mysqld.<SERVER_ID>.expect
# - the $_expect_file_name file exists
# - the $_expect_file_name file contains "wait"
# - the $restart_parameters string is either empty or a list of command line
#   parameters to use (in addition to and overwriting the defaults) when
#   starting the server
# - starting the server with restart parameters will lead to a failure
#
# The postcondition are:
# - the expect file exits
# - the expect file contains "wait"
# - the test runner was informed to start the server with specified options
#
# Note: there's no guarantee that the server process has already started
# by the time the script has returned, nor that it has already finished.
#
# ==== Usage ====
#
# 1. ensure the preconditions for example by:
# a) --source include/shutdown_mysqld.inc
# b) --source include/kill_mysqld.inc
# c) --source include/expect_crash.inc
#    and executing something which causes a crash
# 2. optionally set the $restart_parameters
# --let $restart_parameters=options-causing-crash=ON
# 3. include the script:
# --source include/start_mysqld_expecting_crash.inc
#
# ==== Common pitfalls ===
#
# A common bug in our tests was that people tried to call start_mysqld.inc
# while the expect file already existed and contained something other than "wait",
# i.e "restart", "restart_abort" etc.
# This was a race condition between server being restarted due to the inclusion
# of this script, and restart because the test runner notices something other
# than "wait".
# The proper way of using start_mysqld.inc is to call it when you know the
# server is not running - this is tricky, so as a compromise you should at
# least ensure you are not connected. How? You've used a script which waits
# for the server to shut down or at least disconnect your connection, or
# you've executed a query which triggered a fatal error and your client
# got disconnected (--error CR_SERVER_LOST, 2013).
# And you don't want the test runner to ressurect the server, so the expect
# file should exists and contain "wait" in it.
# If it doesn't exist, then the test runner will panic, that server went
# away and you didn't expect it. If it exists and has "restart" or
# "restart_abort" in it, then the test runner will restart the server for
# you, so you shouldn't attempt to also start it in parallel by using this
# script.
# Another, more rare bug was to try to use start_mysqld.inc when the expect
# file doesn't even exit. This implies that the server is up and running, too,
# as otherwise the test runner would panic not seeing the expect file.
# Trying to start the server while it is already started indicates a logic bug.
# Thus, we assert that the expect file exists and contains "wait" in it.

--let $_smec_old_wait_timeout=$wait_timeout
--let $wait_timeout=0
--source include/wait_for_wait_in_expect_file.inc

# We use --exec echo instead of simple "--echo" because
# replace_result works on --exec
--exec echo "# restart_abort: $restart_parameters"

# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart_abort: $restart_parameters" > $_expect_file_name

# We do not wait for the connection to be reestablished, as the
# start_mysqld.inc does by calling wait_until_connected_again.inc, because
# we expect the server to crash during startup.
# Still, we wait for the test runner to at least change the content of
# the expect file back to "wait", which indicates it noticed our request
# to start the server at all. This is to help caller of our script to ensure
# that the test will not progress further until we are promised a new process
# will be started, and that the "wait" in the expect file is not the one written
# by the caller, but the one written by test runner itself. Otherwise the
# caller could progress to next lines of script, perhaps changing the content of
# the expect file once again (say to test yet another scenario), before the test
# runner even notices the previous request to start the server.
# In practice the caller should probably use something like
# include/wait_for_pattern_in_file.inc anyway, to make sure the server crashed
# in the expected way.

--let $wait_timeout=300
--source include/wait_for_wait_in_expect_file.inc
--let $wait_timeout=$_smec_old_wait_timeout
