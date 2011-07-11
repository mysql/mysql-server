# ==== Usage ====
# [--let $rpl_debug=1]
# --let $rpl_log_var_name= log_bin_basename|relay_log_basename|...
# --source suite/sys_vars/inc/bin_relay_log_basename_index.inc
#
#   $rpl_log_var_name
#     The name of the variable to test, i.e., one of:
#     - log_bin_basename
#     - relay_log_basename
#     - log_bin_index
#     - relay_log_index
#
#   $rpl_debug
#     See include/rpl_init.inc

if ($rpl_debug)
{
  --echo Testing variable named: $rpl_log_var_name
}

--let $basename= `SELECT @@global.$rpl_log_var_name`

#
# Assert that the variable is indeed only global
#
--replace_result $basename REPLACED
--eval select @@global.$rpl_log_var_name

--error ER_INCORRECT_GLOBAL_LOCAL_VAR
--eval select @@session.$rpl_log_var_name

#
# Assert that it is retrievable 
# 
--replace_result $basename REPLACED
--eval show global variables like '$rpl_log_var_name'

--replace_result $basename REPLACED
--eval show session variables like '$rpl_log_var_name'

--replace_result $basename REPLACED
--eval select * from information_schema.global_variables where variable_name='$rpl_log_var_name'

--replace_result $basename REPLACED
--eval select * from information_schema.session_variables where variable_name='$rpl_log_var_name'

#
# Assert it is read-only
#
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
--eval set global $rpl_log_var_name=1

--error ER_INCORRECT_GLOBAL_LOCAL_VAR
--eval set session $rpl_log_var_name=1

