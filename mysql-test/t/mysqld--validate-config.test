--echo # This test covers the 'validate-config' option.

--echo # Stop the server which was started by MTR.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

let MYSQLD_LOG= $MYSQL_TMP_DIR/validate-config.log;
let DEFARGS= --no-defaults --secure-file-priv="" --log-error=$MYSQLD_LOG;

--echo # Reports an error since --foo is an invalid option
--error 1
--exec $MYSQLD $DEFARGS --validate-config --foo=bar 2>$MYSQLD_LOG

let SEARCH_FILE= $MYSQLD_LOG;
--let SEARCH_PATTERN= unknown variable 'foo=bar'
--source include/search_pattern.inc

--remove_file $MYSQLD_LOG

--echo # Reports an error since --tx_read_only is an invalid option
--error 1
--exec $MYSQLD $DEFARGS --validate-config --read_only=1 --tx_read_only=on 2>$MYSQLD_LOG

--let SEARCH_PATTERN= unknown variable 'tx_read_only=on'
--source include/search_pattern.inc

--echo # No error reported.
--exec $MYSQLD $DEFARGS --validate-config --read_only=1

--echo # No error reported. Only warning about invalid value.
--exec $MYSQLD $DEFARGS --log_error_verbosity=2 --validate-config --read_only=s 2>$MYSQLD_LOG

--echo # No error or warning reported for invalid value for SE option.
--exec $MYSQLD $DEFARGS --log_error_verbosity=2 --validate-config --innodb-table-locks=gg

let SEARCH_FILE= $MYSQLD_LOG;
--let SEARCH_PATTERN= option 'read_only': boolean value 's' was not recognized. Set to OFF.
--source include/search_pattern.inc
--remove_file $MYSQLD_LOG

--echo # Restart the server with default options.
--source include/start_mysqld.inc
