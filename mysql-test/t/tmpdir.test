# Removing a tmpdir in use by mysqld fails on windows.
--source include/not_windows.inc

# Verify that tmpdir is a read only variable.
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET @@tmpdir= 'no/such/directory';

# Get hold of data directory and assign a separate log file.
--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/tmpdir_err.log
--let $MYSQLD_DATADIR= `SELECT @@datadir`

# Stop the default server.
--source include/shutdown_mysqld.inc

# Start with an invalid tmpdir.
--error 1,42,134,138,139
--exec $MYSQLD --no-defaults --secure-file-priv="" --basedir=$BASEDIR --lc-messages-dir=$MYSQL_SHAREDIR --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR --tmpdir=no/such/directory > $MYSQLD_LOG 2>&1

# Search the error log for error message.
--let $grep_file= $MYSQLD_LOG
--let $grep_pattern= No such file or directory
--let $grep_output= print_count
--source include/grep_pattern.inc

# Start with a valid tmpdir, then delete it to make it invalid.
--let $tmpdir= $MYSQLTEST_VARDIR/tmp/tmpdir
--mkdir $tmpdir
--let $restart_parameters= restart: --tmpdir=$tmpdir
--replace_result $tmpdir tmpdir
--source include/start_mysqld.inc
--force-rmdir $tmpdir

# Make InnoDB use tmpdir and check that we get an error as expected when submitting a statement that
# we know will use the tmpdir.
CALL mtr.add_suppression("Cannot create temporary merge file");
SET @@innodb_tmpdir= NULL;
CREATE TABLE test.t(a text);
--replace_regex / '[^']+/ 'tmpdir\/tmpfile/
--error 1
ALTER TABLE test.t ADD fulltext(a);
DROP TABLE test.t;

# Restart default server.
--let $restart_parameters= restart:
--source include/restart_mysqld.inc
