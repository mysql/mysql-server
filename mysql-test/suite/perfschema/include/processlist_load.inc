
# 1 <Id> 2 <User> 3 <Host> 4 <db> 5 <Command> 6 <Time> 7 <State> 8 <Info>

--echo
--replace_column 1 <Id> 3 <Host> 6 <Time> 7 <State>
--replace_regex /Waiting on empty queue/WAITING/ /Waiting for next activation/WAITING/ /Waiting on cond/WAITING/ /Daemon/<Command>/ /Connect/<Command>/
--sorted_result
SHOW FULL PROCESSLIST;

--echo
--echo # Performance Schema processlist table
--echo
--replace_column 1 <Id> 3 <Host> 6 <Time> 7 <State>
--replace_regex /Waiting on empty queue/WAITING/ /Waiting for next activation/WAITING/ /Waiting on cond/WAITING/ /Daemon/<Command>/ /Connect/<Command>/
--sorted_result
select * from performance_schema.processlist order by user, id;

--echo
--echo # Information Schema processlist table
--echo
--replace_column 1 <Id> 3 <Host> 6 <Time> 7 <State>
--replace_regex /Waiting on empty queue/WAITING/ /Waiting for next activation/WAITING/ /Waiting on cond/WAITING/ /Daemon/<Command>/ /Connect/<Command>/
--sorted_result
select * from information_schema.processlist order by user, id;

