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

# Discover if server version maturity is LTS
--source include/is_mysql_version_maturity_lts.inc

--let $_is_all_members_version_maturity_lts= 0
if ($is_mysql_version_maturity_lts_out_var)
{
  # Discover if all members belong to the same LTS version.
  # The version is fetch from the column MEMBER_VERSION of the
  # performance_schema.replication_group_members table.
  --let $_is_all_members_version_maturity_lts= 1

  # Get first member major and minor version.
  --let $_first_member_version= `SELECT MEMBER_VERSION FROM performance_schema.replication_group_members WHERE member_state IN ("ONLINE", "RECOVERY") LIMIT 1`
  --let $_first_member_major_version = `SELECT SUBSTRING_INDEX('$_first_member_version', '.', 1)`
  --let $_first_member_minor_version = `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$_first_member_version', '.', 2), '.', -1)`

  # Compare first member major and minor versions with all members
  # that belong to the group.
  while ($_server_index <= $rpl_server_count)
  {
    --let $rpl_connection_name= server_spm_consistency$_server_index
    --let $rpl_server_number= $_server_index
    --source include/rpl/connect.inc
    --let $rpl_connection_name= server_spm_consistency$_server_index
    --source include/connection.inc

    --let $_this_member_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
    --let $_is_this_member_part_of_the_group= `SELECT member_state IN ("ONLINE", "RECOVERY") FROM performance_schema.replication_group_members WHERE member_id="$_this_member_uuid"`

    if ($_is_this_member_part_of_the_group)
    {
      --let $_this_member_version= query_get_value(SELECT MEMBER_VERSION FROM performance_schema.replication_group_members WHERE member_id='$_this_member_uuid', MEMBER_VERSION, 1)
      --let $_this_member_major_version = `SELECT SUBSTRING_INDEX('$_this_member_version', '.', 1)`
      --let $_this_member_minor_version = `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$_this_member_version', '.', 2), '.', -1)`
    }

    # Disconnect the current connection since we may change
    # $_server_index before the end of the iteration.
    --connection default
    --disconnect server_spm_consistency$_server_index

    if ($_is_this_member_part_of_the_group)
    {
      if ($_first_member_major_version != $_this_member_major_version)
      {
        --let $_is_all_members_version_maturity_lts= 0
        --let $_server_index= $rpl_server_count
      }

      if ($_first_member_minor_version != $_this_member_minor_version)
      {
        --let $_is_all_members_version_maturity_lts= 0
        --let $_server_index= $rpl_server_count
      }
    }

    --inc $_server_index
  }
}

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
    --let $skip_checking_read_only= 1
    --source include/gr_assert_multi_primary_mode.inc

    if (!$lowest_version_of_group)
    {
      --source include/gr_find_lowest_member_version.inc
      --let $lowest_version_of_group = $group_replication_lowest_member_version_out_var
    }
    --let $_am_i_writable= query_get_value(SELECT COUNT(*) AS COUNT FROM performance_schema.replication_group_members WHERE member_version="$lowest_version_of_group" AND member_id="$_this_member_id", COUNT, 1)

    # LTS versions are compatible, thence allowed to do writes.
    if ($_is_all_members_version_maturity_lts)
    {
      --let $_am_i_writable= 1
    }

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
