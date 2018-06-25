# ==== Purpose ====
#
# Assert member is on multi primary mode
#
# ==== Usage ====
#
# --source include/gr_assert_multi_primary_mode.inc
#

--let $include_filename= gr_assert_multi_primary_mode.inc
--source include/begin_include_file.inc

--let $assert_text= The single primary mode should be 0 here.
--let $assert_cond= [SELECT @@GLOBAL.group_replication_single_primary_mode] = 0
--source include/assert.inc

--let $assert_text= group_replication_enforce_update_everywhere_checks is ON
--let $assert_cond= [SELECT @@GLOBAL.group_replication_enforce_update_everywhere_checks] = "1"
--source include/assert.inc

--let $assert_text= 'No secondaries on the group'
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_role="SECONDARY"] = 0
--source include/assert.inc

--let $assert_text= The super_read_only mode should be 0 here.
--let $assert_cond= "[SELECT @@GLOBAL.super_read_only]" = 0;
--source include/assert.inc

--let $include_filename= gr_assert_multi_primary_mode.inc
--source include/end_include_file.inc
