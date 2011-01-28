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


--let $include_filename= setup_fake_relay_log.inc
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}

# Print message.
let $_fake_relay_log_printable= `SELECT REPLACE('$fake_relay_log', '$MYSQL_TEST_DIR', 'MYSQL_TEST_DIR')`;
--echo Setting up fake replication from $_fake_relay_log_printable

# Sanity check.
let $running= 0;
let $_sql_running= query_get_value(SHOW SLAVE STATUS, Slave_SQL_Running, 1);
let $_io_running= query_get_value(SHOW SLAVE STATUS, Slave_IO_Running, 1);
if ($_sql_running == Yes) {
  let $running= 1;
}
if ($_io_running == Yes) {
  let $running= 1;
}
if ($running) {
  --echo Error: Slave was running when test case sourced
  --echo include/setup_fake_replication.inc
  --echo Slave_IO_Running = $_io_running; Slave_SQL_Running = $_sql_running
  --source include/show_rpl_debug_info.inc
  --die
}

# Read server variables.
let $_fake_datadir= `SELECT @@datadir`;
let $_fake_filename= query_get_value(SHOW VARIABLES LIKE 'relay_log', Value, 1);
if (!$_fake_filename) {
  --die ERROR IN TEST: relay_log variable is empty. Please use the server option --relay-log=FILE.
}
let $_fake_relay_log= $_fake_datadir/$_fake_filename-fake.000001;
let $_fake_relay_index= $_fake_datadir/$_fake_filename.index;
# Need to restore relay_log_purge in cleanup_fake_relay_log.inc, since
# CHANGE MASTER modifies it (see the manual for CHANGE MASTER).
let $_fake_relay_log_purge= `SELECT @@global.relay_log_purge`;

RESET SLAVE;
let $_orphan_relay_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1);

# Create relay log file.
--copy_file $fake_relay_log $_fake_relay_log

# Create relay log index.

# After patch for BUG#12190, the filename used in CHANGE MASTER
# RELAY_LOG_FILE will be automatically added the directory of the
# relay log before comparison, thus we need to added the directory
# part (./ on unix .\ on windows) when faking the relay-log-bin.index.

# mysqltest currently parses backslash escapes wrong, so any sequence
# of N backslashes is collapsed to just one backslash. So we use the
# SQL function CHAR() to generate a backslash character instead. Since
# the string is interpreted in SQL context, we to escape it, so we use
# two backslashes.

--let $_fake_dir= `select IF(convert(@@version_compile_os using latin1) IN ("Win32","Win64","Windows"), CONCAT('.', CHAR(92), CHAR(92)), './')`
--let $write_var= $_fake_dir$_fake_filename-fake.000001\n
--let $write_to_file= $_fake_relay_index
--source include/write_var_to_file.inc

# Remember old settings.
--let $_fake_old_master_host= query_get_value(SHOW SLAVE STATUS, Master_Host, 1)

# Setup replication from existing relay log.
eval CHANGE MASTER TO MASTER_HOST='dummy.localdomain', RELAY_LOG_FILE='$_fake_filename-fake.000001', RELAY_LOG_POS=4;

# remove the orphan log file (became spurious)
-- remove_file $_fake_datadir/$_orphan_relay_file

--let $include_filename= setup_fake_relay_log.inc
--source include/end_include_file.inc
