--echo #
--echo # Bug #30263773: MYSQLD IS NOT PROPERLY HANDLING INCORRECT COMMAND LINE
--echo #   ARGUMENTS
--echo #

--let $LOG_FILE = $MYSQL_TMP_DIR/bug30263773.log

--echo # Must contain an error about failing to set a data dir
--error 1
--exec $MYSQLD --no-defaults --secure-file-priv="" --datadir =xyz > $LOG_FILE 2>&1

--let $assert_file = $LOG_FILE
--let $assert_text = error log must contain failed to set data dir error
--let $assert_select = \[ERROR\] \[MY-[0-9][0-9]*\] \[Server\] Failed to set datadir to .*=xyz.*
--let $assert_count = 1
--source include/assert_grep.inc

--remove_file $LOG_FILE
