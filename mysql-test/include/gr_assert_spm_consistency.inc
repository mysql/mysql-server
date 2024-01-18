# ==== Purpose ====
#
# Assert SPM consistency for ONLINE and RECOVERING members.
# Confirms all members have:
# 1. SPM mode set,
# 2. Enforce_update_everywhere_checks is not set,
# 3. Have same primary,
# 4. Super read-only set if secondary else not set
# 5. ONLINE and RECOVERING numbers are same in each member
# 6. No member in ERROR state.
#
# ==== Usage ====
#
#   [--let $_primary_member_of_group=server_uuid]
#   --source include/gr_assert_spm_consistency.inc
#
# Parameters:
#   $_primary_member_of_group
#     Server UUID of primary member of the group.
#

--let $include_filename= gr_assert_spm_consistency.inc
--source include/begin_include_file.inc

--let $_server_index= 1
--let $_active_members_of_group=
--let $_saved_connection= $CURRENT_CONNECTION
--disable_query_log

while ($_server_index <= $rpl_server_count)
{
  --let $rpl_connection_name= server_spm_consistency$_server_index
  --let $rpl_server_number= $_server_index
  --source include/rpl/connect.inc
  --let $rpl_connection_name= server_spm_consistency$_server_index
  --source include/connection.inc

  let $_this_member_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1);
  let $_is_member_in_error= `SELECT member_state IN ("ERROR") FROM performance_schema.replication_group_members WHERE member_id="$_this_member_id"`;
  if ($_is_member_in_error)
  {
    --die "Please check server \$_server_index is in ERROR state."
  }
  let $_is_plugin_running= `SELECT member_state IN ("ONLINE", "RECOVERY") FROM performance_schema.replication_group_members WHERE member_id="$_this_member_id"`;
  if ($_is_plugin_running)
  {
    # Get primary server UUID from first ONLINE member.
    if (!$_primary_member_of_group)
    {
      let $_primary_member_of_group= query_get_value(SELECT MEMBER_ID FROM performance_schema.replication_group_members WHERE member_role="PRIMARY", MEMBER_ID, 1);
    }
    # Confirm primary is same in each member.
    if ($_primary_member_of_group)
    {
      --let $assert_text= 'Primary is same for each member'
      --let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_role="PRIMARY" AND member_id="$_primary_member_of_group"] = 1
      --source include/assert.inc
    }

    --let $_am_i_primary= query_get_value(SELECT COUNT(*) AS COUNT FROM performance_schema.replication_group_members WHERE member_id="$_this_member_id" AND member_role="PRIMARY", COUNT, 1)

    if ($_am_i_primary)
    {
      --source include/gr_assert_primary_member.inc
    }
    if (!$_am_i_primary)
    {
      --source include/gr_assert_secondary_member.inc
    }

    if (!$_active_members_of_group)
    {
      let $_active_members_of_group= `SELECT COUNT(*) AS COUNT FROM performance_schema.replication_group_members WHERE member_state IN ("ONLINE", "RECOVERY")`;
    }
    if ($_active_members_of_group)
    {
      --let $assert_text= 'Number of active members are same across group.'
      --let $assert_cond= [SELECT COUNT(*) AS COUNT FROM performance_schema.replication_group_members WHERE member_state IN ("ONLINE", "RECOVERY")] = $_active_members_of_group
      --source include/assert.inc
    }
  }
  --connection default
  --disconnect server_spm_consistency$_server_index

  --inc $_server_index
}

--enable_query_log
--connection $_saved_connection

--let $include_filename= gr_assert_spm_consistency.inc
--source include/end_include_file.inc
