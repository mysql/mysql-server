# ../include/processlist_load.inc
#
# SUMMARY
#
#   Execute PROCESSLIST queries, sorted by user
#
# USAGE
#
#   Example: Using processlist_set.inc to set @@global.performance_schema_show_processlist
#
#   let $pfs_spl = on/off
#   --source ../include/processlist_set.inc
#   --source ../include/processlist_load.inc
#
#   Columns
#   1 <Id> 2 <User> 3 <Host> 4 <db> 5 <Command> 6 <Time> 7 <State> 8 <Info>

# Sort SHOW PROCESSLIST by User instead of Id because Id is not zero-padded
# Unique usernames give best results
--echo
--replace_column 1 <Id> 3 <Host> 6 <Time> 7 <State>
--replace_regex /Daemon/<Command>/ /Connect/<Command>/ /Sleep/<Command>/
--sorted_result
SHOW FULL PROCESSLIST;

--echo
--echo # Performance Schema processlist table
--echo
--replace_column 1 <Id> 3 <Host> 6 <Time> 7 <State>
--replace_regex /Daemon/<Command>/ /Connect/<Command>/ /Sleep/<Command>/
select * from performance_schema.processlist order by user, id;

--echo
--echo # Information Schema processlist table
--echo
--replace_column 1 <Id> 3 <Host> 6 <Time> 7 <State>
--replace_regex /Daemon/<Command>/ /Connect/<Command>/ /Sleep/<Command>/
select * from information_schema.processlist order by user, id;

