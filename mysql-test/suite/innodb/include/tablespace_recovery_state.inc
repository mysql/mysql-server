if (!$MYSQLD_DATADIR)
{
  --die This script requires 'MYSQLD_DATADIR'
}

if (!$blob_length)
{
 --die This script requires 'blob_length'
}

if (!$recovery_snapshot_dir) {
--die This script requires 'recovery_snapshot_dir'
}

if (!$shutdown_snapshot_dir) {
--die This script requires 'shutdown_snapshot_dir'
}

--echo #
--echo # Stop the page cleaner and other background threads
--echo #
--source ../include/stop_dirty_page_flushing_and_background_redo_producers.inc

--echo #
--echo # Do DML such that redo log record spans multiple log blocks
--echo #
eval INSERT INTO foo VALUES(1, repeat('#', $blob_length));

--echo #
--echo # Take snapshot of the datadir to recovery_snapshot_dir
--echo #
--mkdir $recovery_snapshot_dir
--force-cpdir $MYSQLD_DATADIR $recovery_snapshot_dir

--echo #
--echo # Resume the page cleaner and other background threads
--echo #
--source ../include/resume_dirty_page_flushing_and_background_redo_producers.inc

--echo #
--echo # Clean server shutdown
--echo #
--source include/shutdown_mysqld.inc

--echo # Take snapshot of datadir after shutdown to MYSQL_TMP_DIR/shutdown_snapshot
--mkdir $shutdown_snapshot_dir
--force-cpdir $MYSQLD_DATADIR $shutdown_snapshot_dir

--echo #
--echo # Start the server with the snapshot dir to recover it
--echo #
--let DATADIR = $recovery_snapshot_dir/data
--replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR
--let $restart_parameters = restart: --datadir=$DATADIR
--source include/start_mysqld.inc

--echo #
--echo # Clean server shutdown
--echo #
--source include/shutdown_mysqld.inc

--echo #
--echo # Start the server with usual datadir
--echo #
--let $restart_parameters = restart: --datadir=$MYSQLD_DATADIR
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--source include/start_mysqld.inc