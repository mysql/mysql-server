SET SESSION autocommit= 0;
CREATE TABLE t1 AS SELECT 1 AS col1;
DROP TABLE t1;
RESET MASTER;
SET SESSION DEBUG= '+d,mysql_lock_tables_kill_query';
CREATE TABLE t2 AS SELECT 1 AS col1;
ERROR 70100: Query execution was interrupted
SET SESSION DEBUG= '-d,mysql_lock_tables_kill_query';
# Verify that 'CREATE TABLE t2' event was not logged.
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET SESSION autocommit= 1;
