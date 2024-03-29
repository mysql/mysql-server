# ==== Purpose ====
#
# The replication suites are often run in two 'combinations': with the
# gtid_mode=on and gtid_mode=off.  Some specific test cases do not
# care what gtid_mode is used, but make sense to run with two options
# for some *other* variable, which the test framework does not
# explicitly vary.
#
# This file is used to replace the combination
# gtid_mode=on/gtid_mode=off by some other combination, specified by
# the caller.
#
# ==== Usage ====
#
# --let $variable= VARIABLE_NAME
# --source include/begin_replace_gtid_combination.inc
#
# ... test test test ...
#
# --let $variable= VARIABLE_NAME
# --source include/end_replace_gtid_combination.inc
#
# Parameters:
#   $variable
#     Name of the variable. If GTID_MODE=OFF, @@SESSION.$varible and
#     @@GLOBAL.$variable will be set to 0.  Otherwise they will be set
#     to 1.  As a special case, if $variable==default_storage_engine,
#     then GTID_MODE=OFF sets InnoDB and GTID_MODE=ON sets MyISAM.

--let $_brgc_values= 0/1
if ($variable == 'default_storage_engine')
{
  --let $_brgc_values=InnoDB/MyISAM
}
--let $include_filename= begin_replace_gtid_combination.inc [gtid_mode=off/on -> $variable=$_brgc_values]
--source include/begin_include_file.inc

--disable_query_log
--let $_rgc_old_variable_value= `SELECT @@GLOBAL.$variable`
--let $_rgc_old_gtid_mode= `SELECT @@GLOBAL.GTID_MODE`
--let $_rgc_old_enforce_gtid_consistency= `SELECT @@GLOBAL.ENFORCE_GTID_CONSISTENCY`

--let $_rgc_variable_value= `SELECT @@GLOBAL.GTID_MODE = 'ON'`
if ($variable == 'default_storage_engine')
{
  if ($_rgc_variable_value == 0)
  {
    --let $_rgc_variable_value= InnoDB
  }
  if ($_rgc_variable_value == 1)
  {
    --let $_rgc_variable_value= MyISAM
  }
}

--let $rpl_gtid_mode= OFF
--source include/rpl_set_gtid_mode.inc

--disable_query_log
eval SET @@GLOBAL.$variable= $_rgc_variable_value;
eval SET @@SESSION.$variable = $_rgc_variable_value;
SET @@GLOBAL.ENFORCE_GTID_CONSISTENCY = OFF;
--enable_query_log

--let $include_filename= begin_replace_gtid_combination.inc [gtid_mode=off/on -> $variable=$_brgc_values]
--source include/end_include_file.inc
