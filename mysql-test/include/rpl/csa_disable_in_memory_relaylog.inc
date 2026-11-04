# ==== Purpose ====
#
# Disable the in-memory relay log (IMR) on the current replica channel so that a
# Change Stream Applier (CSA) test exercises the classic on-disk relay-log path.
#
# IMR is ON by default: a new channel stores ON, which takes effect once the
# channel is on CSA. Tests that inspect or depend on
# the on-disk relay log -- relay-log purge, SHOW RELAYLOG EVENTS, the on-disk
# reader's APPLIER_EVENT_MEMORY_LIMIT backpressure, backup-vs-purge, etc. --
# must opt back into the disk path with this include.
#
# The IMR selection can only be changed while replication is stopped, so this
# include is transparent to the current thread state: it records which of the
# receiver (IO) and applier (SQL) threads are running, stops them, sets
# IN_MEMORY_RELAYLOG_ENABLED = OFF, then restarts exactly the threads that were
# running.
#
# The OFF is kept across APPLIER_VERSION switches and restart, and across RESET
# REPLICA on a channel with REQUIRE_ROW_FORMAT = 1 (every CSA channel); RESET
# REPLICA ALL returns the channel to the default ON.
#
# ==== Usage ====
#
# --source include/rpl/connection_replica.inc
# --source include/rpl/csa_disable_in_memory_relaylog.inc
#
# Must be called on the replica connection. Default channel only.
# The channel must be on CSA.

--let $include_filename= rpl/csa_disable_in_memory_relaylog.inc
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}

# Remember which threads are running so they can be restored afterwards.
--let $_csa_imr_io_running= query_get_value(SHOW REPLICA STATUS, Replica_IO_Running, 1)
--let $_csa_imr_sql_running= query_get_value(SHOW REPLICA STATUS, Replica_SQL_Running, 1)
--let $_csa_imr_running_bits= `SELECT IF('$_csa_imr_io_running' = 'Yes', 1, 0) + IF('$_csa_imr_sql_running' = 'Yes', 2, 0)`

# Stop only the threads that are running (no-op if already stopped), then
# disable the in-memory relay log.
--let $_csa_imr_saved_only_running= $rpl_only_running_threads
--let $rpl_only_running_threads= 1
--source include/rpl/stop_replica.inc
--let $rpl_only_running_threads= $_csa_imr_saved_only_running

CHANGE REPLICATION SOURCE TO IN_MEMORY_RELAYLOG_ENABLED = OFF;

# Restore exactly the threads that were running (bit 1 = IO, bit 2 = SQL).
if ($_csa_imr_running_bits == 3)
{
  --source include/rpl/start_replica.inc
}
if ($_csa_imr_running_bits == 1)
{
  --source include/rpl/start_receiver.inc
}
if ($_csa_imr_running_bits == 2)
{
  --source include/rpl/start_applier.inc
}

--let $include_filename= rpl/csa_disable_in_memory_relaylog.inc
--source include/end_include_file.inc
