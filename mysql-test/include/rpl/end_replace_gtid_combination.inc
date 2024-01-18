# See begin_replace_gtid_combination.inc.

--let $_brgc_values= 0/1
if ($variable == 'default_storage_engine')
{
  --let $_brgc_values=InnoDB/MyISAM
}
--let $include_filename= rpl/end_replace_gtid_combination.inc [gtid_mode=off/on -> $variable=$_brgc_values]
--source include/begin_include_file.inc

--disable_query_log
eval SET @@GLOBAL.$variable = $_rgc_old_variable_value;
eval SET @@SESSION.$variable = $_rgc_old_variable_value;
eval SET @@GLOBAL.ENFORCE_GTID_CONSISTENCY = $_rgc_old_enforce_gtid_consistency;
--enable_query_log

--let $include_silent= 1
--let $rpl_gtid_mode= $_rgc_old_gtid_mode
--source include/rpl/set_gtid_mode.inc

--let $include_filename= rpl/end_replace_gtid_combination.inc [gtid_mode=off/on -> $variable=$_brgc_values]
--source include/end_include_file.inc
