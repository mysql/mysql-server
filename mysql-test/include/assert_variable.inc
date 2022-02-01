# ==== Purpose ====
#
# Check if a global system variable has expected value, fail with debug info if
# not.
# ==== Usage ====
#
# --let $assert_text= rpl_semi_sync_master_enabled should be ON
# --let $assert_variable_name= rpl_semi_sync_master_enabled
# --let $assert_variable_value= ON
# [--let $rpl_debug= 1]
# --source include/assert_variable.inc
#
# Parameters:
#
# $assert_text
#   See the help in assert.inc
#
# $assert_variable_name
#   The global variable's name

# $assert_variable_value
#   The global variable's value which is expected by the test case.
#
# $rpl_debug
#   Print extra debug info.

if (!$assert_text)
{
  --let $assert_text= $assert_varaible_name should be $assert_variable_value
}

--let $assert_cond= [SELECT @@$assert_variable_name AS Value, Value, 1] = $assert_variable_value
--source include/assert.inc

--let $assert_variable_name=
--let $assert_variable_value=
