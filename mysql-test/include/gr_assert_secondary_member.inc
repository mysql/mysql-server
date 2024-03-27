# ==== Purpose ====
#
# Assert member is a secondary member on single primary mode
#
# ==== Usage ====
#
# --source include/gr_assert_secondary_member.inc
#

--let $include_filename= gr_assert_secondary_member.inc
--source include/begin_include_file.inc

--let $assert_text= The single primary mode should be 1 here.
--let $assert_cond= [SELECT @@GLOBAL.group_replication_single_primary_mode] = 1
--source include/assert.inc

--let $wait_condition= SELECT @@GLOBAL.group_replication_enforce_update_everywhere_checks = '0'
--source include/wait_condition_or_abort.inc

--let $assert_text= 'There only exist one primary'
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_role="PRIMARY"] = 1
--source include/assert.inc

--let $server_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)

--let $wait_condition= SELECT @@GLOBAL.super_read_only = '1'
--source include/wait_condition_or_abort.inc

--let $assert_text= 'Secondary is correctly set'
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_role="SECONDARY" AND member_id="$server_uuid"] = 1
--source include/assert.inc

--let $include_filename= gr_assert_secondary_member.inc
--source include/end_include_file.inc

--let $server_uuid=
