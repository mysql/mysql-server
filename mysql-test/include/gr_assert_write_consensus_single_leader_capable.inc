# Check that value of WRITE_CONSENSUS_SINGLE_LEADER_CAPABLE is same on all members

# ===== Usage ====
#
# --let $single_leader_capable = [1|0]
# --source include/gr_assert_write_consensus_single_leader_capable.inc
#

# Save current connection.
--let $_old_connection= $CURRENT_CONNECTION

--let $i = 1
while ($i <= $rpl_server_count)
{
  --connection server$i
  -- echo [connection server$i]
  if (!$single_leader_capable)
  {
    --let $assert_text = group_replication_paxos_single_leader must be disabled
    --let $assert_cond = "[SELECT WRITE_CONSENSUS_SINGLE_LEADER_CAPABLE from performance_schema.replication_group_communication_information]" = "0"
    --source include/assert.inc
  }
  if ($single_leader_capable)
  {
    --let $assert_text = group_replication_paxos_single_leader must be enabled
    --let $assert_cond = "[SELECT WRITE_CONSENSUS_SINGLE_LEADER_CAPABLE from performance_schema.replication_group_communication_information]" = "1"
    --source include/assert.inc
  }
  --inc $i
}

# Revert to old connection.
--connection $_old_connection
