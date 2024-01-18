--source include/have_binlog_format_row.inc

--echo #
--echo # Bug#26725206 - EVENTS, CRASH RECOVERY FAILS WITH CAN'T INIT TC LOG, EVENTS
--echo #
--echo #

RESET BINARY LOGS AND GTIDS;

--let $offset_of_query_event= 4
--let $binlog_position= query_get_value("SHOW BINARY LOG STATUS", Position, 1)
--let $binlog_file= query_get_value("SHOW BINARY LOG STATUS", File, 1)

CREATE EVENT IF NOT EXISTS ev1 ON SCHEDULE EVERY 1 SECOND STARTS NOW() ENDS
NOW() + INTERVAL 1 SECOND ON COMPLETION NOT PRESERVE DO SELECT 1;

--let $wait_binlog_event= DROP
--source include/rpl/wait_for_binlog_event.inc

--let $logged_query= query_get_value(SHOW BINLOG EVENTS IN "$binlog_file" FROM $binlog_position, Info, $offset_of_query_event) 
--let $assert_cond= `SELECT "$logged_query" NOT LIKE "%xid=0%"`
--let $assert_text= There shall not exist a DDL with xid=0
--source include/assert.inc
