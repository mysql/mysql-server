# ==== Purpose ====
#
# Wait for the number of members on the given group name on this server.
#
#
# ==== Usage ====
#
# --let $group_replication_group_name= UUID
# --let $group_replication_number_of_members= NUMBER
# [--let $$wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/gr_wait_for_number_of_members.inc
#
# Parameters:
#   $group_replication_group_name
#     Group name
#
#   $group_replication_number_of_members
#     Number of members that must be on the group.
#
#   $wait_timeout
#     See include/wait_condition.inc
#
#   $rpl_debug
#     See include/rpl/init.inc

--let $include_filename= rpl/gr_wait_for_number_of_members.inc
--source include/begin_include_file.inc

if ($group_replication_group_name == '')
{
  --die ERROR IN TEST: the "group_replication_group_name" variable must be set
}

if ($group_replication_number_of_members == '')
{
  --die ERROR IN TEST: the "group_replication_number_of_members" variable must be set
}

if (!$rpl_debug)
{
  --disable_query_log
}

--let $wait_condition= SELECT COUNT(*)=$group_replication_number_of_members FROM performance_schema.replication_group_members
if($group_replication_number_of_members == 0)
{
  --let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_group_members WHERE member_state = "OFFLINE"
}

--source include/wait_condition.inc

--let $include_filename= rpl/gr_wait_for_number_of_members.inc
--source include/end_include_file.inc
