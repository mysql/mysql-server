# ../include/processlist_set.inc
#
# SUMMARY
#
#   Set the value of performance_schema_show_proceslist then
#   wait for the operation to complete
#
# USAGE
#
#   let $pfs_spl = on;
#   --source ../include/processlist_set.inc

eval set @@global.performance_schema_show_processlist = $pfs_spl;

let $wait_condition = show variables where variable_name like '%show_processlist%' and value = '$pfs_spl';
--source include/wait_condition.inc

