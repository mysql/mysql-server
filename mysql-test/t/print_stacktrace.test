# Run manually, to test output from my_print_stacktrace()
# ./mtr --no-check-testcases print_stacktrace
# inspect output in var/log/msqld.1.err

--source include/have_debug.inc

--disable_query_log
SET GLOBAL debug='+d,print_stacktrace';
SELECT CONCAT("Please inspect mysqld server log,",
              " look for print_stacktrace.\n") as Hello;
SET GLOBAL debug='-d,print_stacktrace';
--enable_query_log
