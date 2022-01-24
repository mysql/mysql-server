# ==== Purpose ====
#
# Check if a global system status has expected value, fail with debug info if
# not.
# ==== Usage ====
#
# --let $assert_text= rpl_semi_sync_source_yes_tx should be 2
# --let $assert_status_name= rpl_semi_sync_source_yes_tx
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

let $assert_cond= [SHOW STATUS LIKE "$assert_status_name", Value, 1] $assert_comparator $assert_status_value;
--source include/assert.inc

--let $assert_status_name=
--let $assert_status_value=
--let $assert_condition=
