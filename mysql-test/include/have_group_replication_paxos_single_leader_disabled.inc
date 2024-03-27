# ==== Purpose ====
#
# Ensure that group replication is running with Paxos Single Leader disabled
#

--let $_have_paxos_single_leader= `SELECT @@group_replication_paxos_single_leader`
if ($_have_paxos_single_leader)
{
  --skip Test requires Paxos Single Leader disabled in Group Replication
}