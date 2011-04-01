# ==== Purpose ====
#
# Auxiliary file used by suite/rpl/t/rpl_test_framework.test
#
# The purpose is to check that the sync chain generated in
# rpl_change_topology.inc (invoked from rpl_init.inc) is correct. This
# is done in two ways:
#  (1) Print the sync chain.
#  (2) Execute a statement and verify that it replicates to all slaves.
#
#
# ==== Implementation ====
#
# Does this:
# (1) Set up a given replication topology (rpl_init.inc)
# (2) Print $rpl_sync_chain
# (3) Execute "DELETE FROM t1" and then "INSERT INTO t1" on the master
# (4) Sync and compare all servers.
# (5) Clean up the replication topology (rpl_end.inc)
#
# (Technical detail: Since DELETE FROM t1 is not executed at the end,
# some servers may have rows left in t1 from a previous invocation of
# rpl_test_framework.inc.  Therefore, this file will only work in
# statement mode where "DELETE FROM t1" removes rows that exist on
# slave but not on master.)
#
#
# ==== Usage ====
#
# --let $rpl_server_count= <number>
# --let $rpl_topology= <topology specification>
# --let $masters= <list of masters>
# [--let $rpl_diff_servers= <list of servers>]
# --source extra/rpl_tests/rpl_test_framework.inc
#
# Parameters:
#   $next_number
#     The INSERT statement will insert $next_number into t1, and
#     $next_number will increase by 1.
#
#   $rpl_server_count, $rpl_topology:
#     See include/rpl_init.inc
#
#   $masters
#     This should be a list of numbers, each identifying a server.
#     The DELETE and INSERT statements will be executed on all servers
#     in the list.
#
#   $rpl_diff_servers
#     See include/rpl_diff.inc

--source include/rpl_init.inc
--source include/rpl_generate_sync_chain.inc
--echo rpl_sync_chain= '$rpl_sync_chain'

--inc $next_number

# Iterate over masters
while ($masters)
{
  --let $master_i= `SELECT SUBSTRING_INDEX('$masters', ',', 1)`
  --let $masters= `SELECT SUBSTRING('$masters', LENGTH('$master_i') + 2)`

  # Connect to master and execute statement
  --let $rpl_connection_name= server_$master_i
  --source include/rpl_connection.inc
  DELETE FROM t1;
  --eval INSERT INTO t1 VALUES ($next_number)
}

--source include/rpl_sync.inc

# Compare all servers.
--let $diff_tables= server_$rpl_server_count:t1
--let $server_i= $rpl_server_count
--dec $server_i
while ($server_i)
{
  --let $diff_tables= server_$server_i:t1,$diff_tables
  --dec $server_i
}
--source include/diff_tables.inc

--let $diff_servers=
--let $masters=
--source include/rpl_end.inc
