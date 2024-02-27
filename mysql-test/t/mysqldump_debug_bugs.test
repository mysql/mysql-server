--source include/mysql_have_debug.inc

--echo #
--echo # Bug#36248967: Security issue in mysqldump (mysql dump utility)
--echo #

let $grep_file=$MYSQLTEST_VARDIR/tmp/bug36248967.sql;
let $grep_output=boolean;

CREATE DATABASE test_bug36248967;

--echo -- Run mysqldump with payload injected to server version.
--echo A warning must be issued.
--exec $MYSQL_DUMP --debug="d,server_version_injection_test" --result-file=$grep_file test_bug36248967 2>&1

--echo The test version must be found:
let $grep_pattern=8.0.0-injection_test;
--source include/grep_pattern.inc

--echo The test injected string must not be found:
let $grep_pattern=\\! touch /tmp/xxx;
--source include/grep_pattern.inc

--echo -- Run mysqldump with correct server version.
--exec $MYSQL_DUMP --result-file=$grep_file test_bug36248967 2>&1
--echo A warning must not be issued.

#Cleanup
--remove_file $grep_file
DROP DATABASE test_bug36248967;
