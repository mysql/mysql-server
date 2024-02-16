--echo #
--echo # Bug #29622406: --INTIALIZE FAILS WHEN MYSQLD CONFIG HAS PLUGIN LOADING
--echo #

--let BASEDIR= `select @@basedir`
--let $MYSQLD_SOCKET= `SELECT @@socket`
--let $MYSQLD_PORT= `SELECT @@port`
--let DDIR=$MYSQL_TMP_DIR/installdb_test
--let MYSQLD_LOG=$MYSQL_TMP_DIR/server.log
--let $counter= 0

--echo # shut server down
--source include/shutdown_mysqld.inc
--echo # Server is down

--echo # Must not crash
--let DEFARGS= --no-defaults --console --socket=$MYSQLD_SOCKET --port=$MYSQLD_PORT --tls-version= --datadir=$DDIR
--exec $MYSQLD $DEFARGS --initialize $PFS_EXAMPLE_PLUGIN_EMPLOYEE_OPT $PFS_EXAMPLE_PLUGIN_EMPLOYEE_LOAD_ADD > $MYSQLD_LOG 2>&1

--echo # Restarting the server
--source include/start_mysqld.inc

--let $assert_text=Searching for the plugins disabled warning in the log
--let $assert_file=$MYSQLD_LOG
--let $assert_select=Ignoring --plugin-load._add. list as the server is running with --initialize.-insecure..
--let $assert_count=1
--source include/assert_grep.inc

--echo # Cleanup
--echo # delete mysqld log
--remove_file $MYSQLD_LOG

--echo # delete datadir
--force-rmdir $DDIR

--echo # End of 8.0 tests
