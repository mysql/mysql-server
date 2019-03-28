# ==== Purpose ====
#
# Assert MPM consistency for ONLINE and RECOVERING members.
#
# Confirms all members have:
# 1. SPM mode not set,
# 2. Enforce_update_everywhere_checks is not set,
# 3. Have no secondary roles,
# 4. Only lowest versions are writable,
# 5. ONLINE and RECOVERING numbers are same in each member,
# 6. No member in ERROR state.(configuration changes only aborts operation)
#
# ==== Usage ====
#
#   --let $lowest_version_of_group=group_lowest_version
#   --source include/gr_assert_mpm_consistency.inc
#
# Parameters:
#   $lowest_version_of_group
#     Lowest version of the group.
#

--let $include_filename= gr_assert_mpm_consistency.inc
--source include/begin_include_file.inc

--let $_server_index= 1
--let $_active_members_of_group=
--let $_saved_connection= $CURRENT_CONNECTION
--disable_query_log

while ($_server_index <= $rpl_server_count)
{
  --let $rpl_connection_name= server_spm_consistency$_server_index
  --let $rpl_server_number= $_server_index
  --source include/rpl_connect.inc
  --let $rpl_connection_name= server_spm_consistency$_server_index
  --source include/rpl_connection.inc

  let $_this_member_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1);
  let $_is_member_in_error= `SELECT member_state IN ("ERROR") FROM performance_schema.replication_group_members WHERE member_id="$_this_member_id"`;
  if ($_is_member_in_error)
  {
    --die "Please check server \$_server_index is in ERROR state."
  }
  let $_is_plugin_running= `SELECT member_state IN ("ONLINE", "RECOVERY") FROM performance_schema.replication_group_members WHERE member_id="$_this_member_id"`;
  if ($_is_plugin_running)
  {
    --let $skip_checking_read_only= 1
    --source include/gr_assert_multi_primary_mode.inc

    # Do not rely on this. Manually set lowest version of group.
    # Tests will fail when major version moves from 9 -> 10
    if (!$lowest_version_of_group)
    {
      --let lowest_version_of_group = query_get_value(SELECT member_version FROM performance_schema.replication_group_members ORDER BY member_version ASC LIMIT 1,member_version,1)
    }
    --let $_am_i_writable= query_get_value(SELECT COUNT(*) AS COUNT FROM performance_schema.replication_group_members WHERE member_version="$lowest_version_of_group" AND member_id="$_this_member_id", COUNT, 1)
    if ($_am_i_writable)
    {
      --let $assert_text= The super_read_only mode should be 0 here.
      --let $assert_cond= "[SELECT @@GLOBAL.super_read_only]" = 0;
      --source include/assert.inc
    }
    if (!$_am_i_writable)
    {
      --let $assert_text= The super_read_only mode should be 1 here.
      --let $assert_cond= "[SELECT @@GLOBAL.super_read_only]" = 1;
      --source include/assert.inc
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
--let $lowest_version_of_group=
--enable_query_log
--connection $_saved_connection

--let $include_filename= gr_assert_mpm_consistency.inc
--source include/end_include_file.inc
