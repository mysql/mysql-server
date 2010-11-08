# ==== Purpose ====
#
# Setup replication from an existing relay log in the current
# connection.
#
# ==== Usage ====
#
# Make sure the slave is not running and issue:
#
#   let $fake_relay_log= /path/to/fake-relay-log-file.000001
#   source include/setup_fake_relay_log.inc;
#   START SLAVE SQL_THREAD; # setup_fake_relay_log doesn't start slave
#   ...
#   source include/cleanup_fake_relay_log.inc
#
# You must run the server with --relay-log=FILE. You probably want to
# run with --replicate-same-server-id too.
#
# ==== Implementation ====
#
# First makes a sanity check, ensuring that the slave threads are not
# running.  Then copies the $fake_relay_log to RELAY_BIN-fake.000001,
# where RELAY_BIN is the basename of the relay log, and updates
# RELAY_BIN.index. Finally issues CHANGE MASTER so that it uses the
# given files.
#
# ==== Side effects ====
#
# Creates a binlog file and a binlog index file, and sets
# @@global.relay_log_purge=1. All this is restored when you call
# cleanup_fake_relay_log.inc.
#
# Enables the query log.


--disable_query_log

# Print message.
let $_fake_relay_log_printable= `SELECT REPLACE('$fake_relay_log', '$MYSQL_TEST_DIR', 'MYSQL_TEST_DIR')`;
--echo Setting up fake replication from $_fake_relay_log_printable

# Sanity check.
let $_sql_running= query_get_value(SHOW SLAVE STATUS, Slave_SQL_Running, 1);
let $_io_running= query_get_value(SHOW SLAVE STATUS, Slave_IO_Running, 1);
if (`SELECT "$_sql_running" = "Yes" OR "$_io_running" = "Yes"`) {
  --echo Error: Slave was running when test case sourced
  --echo include/setup_fake_replication.inc
  --echo Slave_IO_Running = $_io_running; Slave_SQL_Running = $_sql_running
  --echo Printing some debug info:
  SHOW SLAVE STATUS;
  SHOW MASTER STATUS;
  SHOW BINLOG EVENTS;
  SHOW PROCESSLIST;
}

# Read server variables.
let $MYSQLD_DATADIR= `SELECT @@datadir`;
let $_fake_filename= query_get_value(SHOW VARIABLES LIKE 'relay_log', Value, 1);
if (!$_fake_filename) {
  --echo Badly written test case: relay_log variable is empty. Please use the
  --echo server option --relay-log=FILE.
}
let $_fake_relay_log= $MYSQLD_DATADIR/$_fake_filename-fake.000001;
let $_fake_relay_index= $MYSQLD_DATADIR/$_fake_filename.index;
# Need to restore relay_log_purge in cleanup_fake_relay_log.inc, since
# CHANGE MASTER modifies it (see the manual for CHANGE MASTER).
let $_fake_relay_log_purge= `SELECT @@global.relay_log_purge`;

# Create relay log file.
copy_file $fake_relay_log $_fake_relay_log;
# Create relay log index.

if (`SELECT LENGTH(@@secure_file_priv) > 0`)
{
  -- let $_file_priv_dir= `SELECT @@secure_file_priv`
  -- let $_suffix= `SELECT UUID()`
  -- let $_tmp_file= $_file_priv_dir/fake-index.$_suffix

  -- eval select '$_fake_filename-fake.000001\n' into dumpfile '$_tmp_file'
  -- copy_file $_tmp_file $_fake_relay_index
  -- remove_file $_tmp_file
}

if (`SELECT LENGTH(@@secure_file_priv) = 0`)
{
  -- eval select '$_fake_filename-fake.000001\n' into dumpfile '$_fake_relay_index'
}

# Setup replication from existing relay log.
eval CHANGE MASTER TO MASTER_HOST='dummy.localdomain', RELAY_LOG_FILE='$_fake_filename-fake.000001', RELAY_LOG_POS=4;

--enable_query_log
