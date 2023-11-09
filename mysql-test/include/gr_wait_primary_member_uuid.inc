# ==== Purpose ====
#
# Wait for the given server to be elected as the Primary member of the group
#
# ==== Usage ====
#
# [--let $wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --let $group_replication_expected_uuid=UUID
# --source include/gr_wait_primary_member_uuid.inc
#

--let $include_filename= gr_wait_primary_member_uuid.inc
--source include/begin_include_file.inc

if (`SELECT @@GLOBAL.group_replication_single_primary_mode != 1`)
{
  --die "The group must be operating in single-primary mode."
}

if ($group_replication_expected_uuid == '')
{
  --die "Missing argument 'group_replication_expected_uuid'"
}

let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_group_members
                     WHERE MEMBER_ROLE="PRIMARY" AND
                           MEMBER_ID="$group_replication_expected_uuid";
--source include/wait_condition_or_abort.inc

--let $include_filename= gr_wait_primary_member_uuid.inc
--source include/end_include_file.inc

--let $group_replication_expected_uuid=
--let $group_replication_member_role=
