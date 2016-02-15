# ==== Purpose ====
#
# Check if a global system status has expected value, fail with debug info if
# not.
# ==== Usage ====
#
# --let $assert_text= rpl_semi_sync_master_yes_tx should be 2
# --let $assert_status_name= rpl_semi_sync_master_yes_tx
# --let $assert_status_value= 2
# --let $assert_comparator= >
# [--let $rpl_debug= 1]
# --source include/assert_status.inc
#
# Parameters:
#
# $assert_text
#   See the help in assert.inc
#
# $assert_status_value
#   The status variable's value which is expected by the test case.
#
# $assert_comparator
#   The comparator is used in the the assertion. It could be =, >, >=, <, <=, !=
#
# $rpl_debug
#   Print extra debug info.
if (!$assert_text)
{
  --let $assert_text= $assert_status_name should be $assert_status_value
}

if (!$assert_comparator)
{
  --let $assert_comparator= =
}

#Only valid Replication PFS tables should be passed.Refer WL#7817
#Only valid assert_status_names should be passed. Refer WL#7817

if (`select count(*) != 0 from performance_schema.$table`)
{
let $assert_cond= [select $assert_status_name from performance_schema.$table] $assert_comparator $assert_status_value;
}
if (`select count(*) = 0 from performance_schema.$table`)
{
let $assert_cond= [select count(*) from performance_schema.$table] $assert_comparator $assert_status_value;
}

--source include/assert.inc

--let $assert_status_name=
--let $assert_status_value=
--let $assert_condition=
