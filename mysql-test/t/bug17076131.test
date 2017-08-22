--echo #
--echo # Bug#17076131 MYSQLDUMP FAILS WHEN ERR LOG ON RUNNING SERVER IS
--echo #              DELETED ON WINDOWS
--echo #


--echo # Creating file aliases.
let $dump_with_err_log=$MYSQLTEST_VARDIR/tmp/bug17076131_with_err_log.sql;
let $dump_without_err_log=$MYSQLTEST_VARDIR/tmp/bug17076131_without_err_log.sql;

--echo # Executing mysqldump normally.
--exec $MYSQL_DUMP --extended-insert --flush-logs --add-drop-database --add-drop-table  --force --databases --skip-dump-date mysql  > $dump_with_err_log

--echo # Removing Error_log file.
--remove_file $MYSQLTEST_VARDIR/tmp/mysql.err

--echo # Executing mysqldump after deleting error_log file.
--exec $MYSQL_DUMP --extended-insert --flush-logs --add-drop-database --add-drop-table  --force --databases --skip-dump-date mysql  > $dump_without_err_log

--echo # Checking diff of the 2 dumps.
--diff_files $dump_with_err_log $dump_without_err_log
--echo # No Difference found.

--echo # Clean Up.
--remove_file $dump_with_err_log
--remove_file $dump_without_err_log
--echo # End of Test.
