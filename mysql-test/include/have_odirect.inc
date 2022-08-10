# Tests if datadir can support innodb_flush_method as O_DIRECT
# if datadir is on ramdisk, O_DIRECT is not supported. Such tests
# should be run without --mem and mtr var dir should be on regular disk

--source include/shutdown_mysqld.inc

--let $ERROR_LOG=$MYSQL_TMP_DIR/o_direct.log
--error 1
--exec $MYSQLD_CMD --invalid-option --log-error-verbosity=3 --log-error=$ERROR_LOG --innodb_flush_method=O_DIRECT

--let $grep_pattern=Failed to set O_DIRECT
--let $grep_file=$ERROR_LOG
--let $grep_output=none
--source grep_pattern.inc

--remove_file $ERROR_LOG

--source include/start_mysqld_no_echo.inc
if ($GREP_RESULT == "1") {
  --skip "O_DIRECT not available. Requires datadir not be on ramdisk"
}

