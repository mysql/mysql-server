# ==== Purpose ====
#
# Finds the lowest version in the group, that is, finds on the
# content of the performance_schema.replication_group_members
# on the current member.
#
# Result is placed on var:
# - $group_replication_lowest_member_version_out_var
#
# The current connection is preserved.
#
# ==== Usage ====
#
# --source include/gr_find_lowest_member_version.inc
#
# --echo: LOWEST_MEMBER_VERSION: $group_replication_lowest_member_version_out_var

--let $include_filename= gr_find_lowest_member_version.inc
--source include/begin_include_file.inc

--let $_members_count= `SELECT COUNT(*) FROM performance_schema.replication_group_members`
if (!$_members_count)
{
  --die "This include file can only be used on a server that is part of a group."
}

--let $_member_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--let $_is_member_not_in_a_group= `SELECT member_state IN ("ERROR", "OFFLINE") FROM performance_schema.replication_group_members WHERE member_id="$_member_id"`
if ($_is_member_not_in_a_group)
{
  --die "This include file can only be used on a server that is part of a group."
}

# Fetch current member version.
--let $group_replication_lowest_member_version_out_var= query_get_value(SELECT MEMBER_VERSION FROM performance_schema.replication_group_members WHERE member_id='$_member_uuid', MEMBER_VERSION, 1)
--let $_lowest_member_major_version= `SELECT SUBSTRING_INDEX('$group_replication_lowest_member_version_out_var', '.', 1)`
--let $_lowest_member_minor_version= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$group_replication_lowest_member_version_out_var', '.', 2), '.', -1)`
--let $_lowest_member_patch_version= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$group_replication_lowest_member_version_out_var', '.', 3), '.', -1)`

# Find lowest member version in the group.
--let $_limit_start= 0
--let $_member_index= 1
while ($_member_index <= $_members_count)
{
  --let $_this_member_version= `SELECT MEMBER_VERSION FROM performance_schema.replication_group_members ORDER BY MEMBER_PORT LIMIT $_limit_start, $_member_index`
  --let $_this_member_major_version= `SELECT SUBSTRING_INDEX('$_this_member_version', '.', 1)`
  --let $_this_member_minor_version= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$_this_member_version', '.', 2), '.', -1)`
  --let $_this_member_patch_version= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$_this_member_version', '.', 3), '.', -1)`

  # Lower patch version.
  if ($_this_member_major_version == $_lowest_member_major_version)
  {
    if ($_this_member_minor_version == $_lowest_member_minor_version)
    {
      if ($_this_member_patch_version < $_lowest_member_patch_version)
      {
        --let $group_replication_lowest_member_version_out_var= $_this_member_version
        --let $_lowest_member_patch_version= $_this_member_patch_version
      }
    }
  }

  # Lower minor version
  if ($_this_member_major_version == $_lowest_member_major_version)
  {
    if ($_this_member_minor_version < $_lowest_member_minor_version)
    {
      --let $group_replication_lowest_member_version_out_var= $_this_member_version
      --let $_lowest_member_minor_version= $_this_member_minor_version
      --let $_lowest_member_patch_version= $_this_member_patch_version
    }
  }

  # Lower major version
  if ($_this_member_major_version < $_lowest_member_major_version)
  {
    --let $group_replication_lowest_member_version_out_var= $_this_member_version
    --let $_lowest_member_major_version= $_this_member_major_version
    --let $_lowest_member_minor_version= $_this_member_minor_version
    --let $_lowest_member_patch_version= $_this_member_patch_version
  }

  --inc $_limit_start
  --inc $_member_index
}

--let $include_filename= gr_find_lowest_member_version.inc
--source include/end_include_file.inc
