--echo #
--echo # bug#30209760 : ASSERTION FAILURE: SRV0START.CC:969:SUCCESS THREAD
--echo #

--echo #########
--echo # SETUP #
--echo #########
let $MYSQLD_BASEDIR= `select @@basedir`;
let $START_PAGE_SIZE= `select @@innodb_page_size`;
let $LOG_SIZE= `select @@innodb_redo_log_capacity`;

--echo # Create path for ibdata* & undo* files
--mkdir $MYSQL_TMP_DIR/innodb_undo_data_dir
--mkdir $MYSQL_TMP_DIR/innodb_data_home_dir
--mkdir $MYSQL_TMP_DIR/datadir

# Set path for --datadir
let $MYSQLD_DATADIR = $MYSQL_TMP_DIR/datadir/data;

# Set path for undo* files.
let $MYSQLD_UNDO_DATADIR = $MYSQL_TMP_DIR/innodb_undo_data_dir;

# Set path for ibdata* files.
let $MYSQLD_HOME_DATA_DIR = $MYSQL_TMP_DIR/innodb_data_home_dir;

--echo # create bootstrap file
let $BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF

--echo # Stop the MTR default DB server
--source include/shutdown_mysqld.inc

--echo #########################################################################
--echo # INITIALIZE NEW SERVER
--echo #########################################################################
--source include/keyring_tests/helper/binary_backup_manifest.inc
let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR --init-file=$BOOTSTRAP_SQL --secure-file-priv="" $PLUGIN_DIR_OPT;

--echo # Server should be initialized successfully.
--exec $NEW_CMD

--echo #########################################################################
--echo # RESTART 2 : WITH KEYRING.
--echo               innodb_undo_log_encrypt=ON.
--echo               Crash server before purge thread could start.
--echo #########################################################################
# Restart the server with undo_log_encrypt=ON and make sure it asserts before
# Purge thread is started (i.e. before master thread could have started to
# rotate default master key. At this time,
# - UNDO tablespace would have been encrypted with default master key
# - Header page for UNDO tablespace wouldn't have been flushed
# - So encryption information is still in REDO Log.
--source include/keyring_tests/helper/binary_restore_manifest.inc
let $NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR --innodb_undo_log_encrypt=ON --debug=d,crash_before_purge_thread $PLUGIN_DIR_OPT;
--error 1,2,42,137
--exec $NEW_CMD

--echo #########################################################################
--echo # RESTART 3 : WITH KEYRING.
--echo               innodb_undo_log_encrypt=ON.
--echo               Skip rotating default master key in master thread
--echo #########################################################################
# Here server would start and UNDO encryption key would be loaded in-mem
# successfully after reading them from REDO Log during scan.
# NOTE: without the fix
# - the encryption information wouldn't be applied on page 0.
# - And as we don't allow master thread to rotate default master key, UNDO
#   tablespace encryption information will be lost at next restart.
let $restart_parameters = restart: $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR --innodb_undo_log_encrypt=ON --debug=d,skip_rotating_default_master_key;
--source include/start_mysqld_no_echo.inc

--echo #########################################################################
--echo # RESTART 4 : WITH KEYRING.
--echo               innodb_undo_log_encrypt=OFF.
--echo #########################################################################
# During this restart, without the fix:
# - Encryption information will be looked for on page 0. But as it wasn't
#   applied in previous start, it wouldn't be found thus ASSERT will be hit.
let $restart_parameters = restart: $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR --innodb_undo_log_encrypt=OFF;
--source include/restart_mysqld_no_echo.inc

--echo ###########
--echo # CLEANUP #
--echo ###########
# Reset to initial state
--source include/shutdown_mysqld.inc
--source include/wait_until_disconnected.inc
--let $restart_parameters="restart: $PLUGIN_DIR_OPT"
--source include/start_mysqld_no_echo.inc

# Remove residue files
--remove_file $BOOTSTRAP_SQL
--force-rmdir $MYSQL_TMP_DIR/datadir
--force-rmdir $MYSQL_TMP_DIR/innodb_data_home_dir
--force-rmdir $MYSQL_TMP_DIR/innodb_undo_data_dir
