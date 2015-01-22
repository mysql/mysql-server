# ==== Purpose ====
#
# Assert that exactly one GTID (or a specified number) was added to
# the Retrieved_Gtid_Set since last call to gtid_step_reset_on_retrieved.inc
# or gtid_step_assert_on_retrieved.inc
#
# ==== Usage ====
#
# [--let $gtid_step_count= N]
# [--let $gtid_step_only_count= [0|1]]
# [--let $gtid_step_uuid= UUID]
# --source include/gtid_step_assert_on_retrieved.inc
#
# Parameters:
#   $gtid_step_count
#     Assert that N GTIDs were added to the Retrieved_Gtid_Set.
#     If this is not given, the default value is 1.
#
#   $gtid_step_only_count
#     By default, this script checks the numeric component of the
#     GTIDs added to the Retrieved_Gtid_Set to see that the correct
#     number was generated.
#     If this parameter is nonzero, it only counts the number of added
#     GTIDs to see that it is equal to $gtid_step_count, without caring
#     that they were generated in order (or generated on this server).
#
#   $gtid_step_uuid
#     By default, when $gtid_step_count is not set, asserts that the
#     uuid component of all GTIDs added to the Retrieved_Gtid_set
#     is equal to @@GLOBAL.SERVER_UUID.
#     If this parameter is given, uses that UUID instead.
#
# ==== Example ====
#
# # (1) Remember the current position.
# --source include/rpl_connection_slave.inc
# --source include/gtid_step_reset_on_retrieved.inc
# --source include/rpl_connection_master.inc
# CREATE TABLE t1 (a INT);
# # (2) Assert that exactly one GTID was generated since (1).
# --source include/rpl_connection_slave.inc
# --source include/gtid_step_assert_on_retrieved.inc
# --source include/rpl_connection_master.inc
# INSERT INTO t1 VALUES (1);
# INSERT INTO t1 VALUES (2);
# # (3) Assert that exactly 2 GTIDs were generated since (2).
# --let $gtid_step_count= 2
# --source include/rpl_connection_slave.inc
# --source include/gtid_step_assert_on_retrieved.inc
# # (4) Assert that no GTID was generated since (3).
# --let $gtid_step_count= 0
# --source include/gtid_step_assert_on_retrieved.inc

if ($uuidf == '')
{
  --die You must source gtid_utils.inc before using gtid_step_assert_on_retrieved.inc
}

--let $_gtid_step_count= 1
if ($gtid_step_count != '')
{
  --let $_gtid_step_count= $gtid_step_count
}

--let $_gtid_step_only_count= 0
if ($gtid_step_only_count != '')
{
  --let $_gtid_step_only_count= $gtid_step_only_count
}

--let $include_filename= gtid_step_assert_on_retrieved.inc [count=$_gtid_step_count, only_count=$_gtid_step_only_count]
--source include/begin_include_file.inc

--let $_gtid_step_uuid= $gtid_step_uuid
if ($_gtid_step_uuid == '')
{
  --let $_gtid_step_uuid= `SELECT @@GLOBAL.SERVER_UUID`
}

--let $_gsa_extra_debug_eval_old= $extra_debug_eval

--let $local_gtid_set= query_get_value(SHOW SLAVE STATUS, Retrieved_Gtid_Set, 1)
--echo Asserting based on SHOW SLAVE STATUS(Retrieved_Gtid_Set)

--let $extra_debug_eval= "$local_gtid_set", GTID_NEXT_GENERATED_MULTIPLE("$gtid_step_last", "$_gtid_step_uuid", $_gtid_step_count), GTID_SUBTRACT("$local_gtid_set", "$gtid_step_last")

if ($_gtid_step_only_count)
{
  --let $assert_text= Exactly $_gtid_step_count GTIDs should have been retrieved since last invocation
  --let $assert_cond= GTID_COUNT(GTID_SUBTRACT("$local_gtid_set", "$gtid_step_last")) = $_gtid_step_count
  --source include/assert.inc
}
if (!$_gtid_step_only_count)
{
  --let $assert_text= Exactly $_gtid_step_count GTIDs should have been retrieved since last invocation
  --let $assert_cond= GTID_NEXT_GENERATED_MULTIPLE("$gtid_step_last", "$_gtid_step_uuid", $_gtid_step_count) = GTID_SUBTRACT("$local_gtid_set", "$gtid_step_last")
  --source include/assert.inc
}

--let $extra_debug_eval= $_gsa_extra_debug_eval_old

--source include/gtid_step_reset_on_retrieved.inc

--let $include_filename= gtid_step_assert_on_retrieved.inc
--source include/end_include_file.inc
