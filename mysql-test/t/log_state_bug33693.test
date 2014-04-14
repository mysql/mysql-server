### t/log_state_bug33693.test
#
# Regression test for bug #33693
#   "general log name and location depend on PID 
#    file, not on predefined values"
#
# The server is started with a hard-coded 
# PID file in the $MYSQLTEST_VARDIR/run
# directory, and an unspecified general log
# file name.
#
# The correct result should show the log file to 
# rest in the database directory. Unfixed, the 
# log file will be in the same directory as the
# PID.

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--eval SELECT INSTR(@@general_log_file, '$MYSQLTEST_VARDIR/run');
