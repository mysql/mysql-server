SET GLOBAL event_scheduler = OFF;
SELECT name, type, processlist_user, processlist_host, processlist_db,
processlist_command, processlist_info,
IF(parent_thread_id IS NULL, parent_thread_id, 'unified parent_thread_id')
AS unified_parent_thread_id,
`role`, instrumented
FROM performance_schema.threads
WHERE name LIKE 'thread/sql%'
ORDER BY name;
name	thread/sql/compress_gtid_table
type	FOREGROUND
processlist_user	NULL
processlist_host	NULL
processlist_db	NULL
processlist_command	Daemon
processlist_info	NULL
unified_parent_thread_id	unified parent_thread_id
role	NULL
instrumented	YES
name	thread/sql/main
type	BACKGROUND
processlist_user	NULL
processlist_host	NULL
processlist_db	mysql
processlist_command	NULL
processlist_info	NULL
unified_parent_thread_id	NULL
role	NULL
instrumented	YES
name	thread/sql/one_connection
type	FOREGROUND
processlist_user	root
processlist_host	localhost
processlist_db	test
processlist_command	Query
processlist_info	SELECT name, type, processlist_user, processlist_host, processlist_db,
processlist_command, processlist_info,
IF(parent_thread_id IS NULL, parent_thread_id, 'unified parent_thread_id')
AS unified_parent_thread_id,
`role`, instrumented
FROM performance_schema.threads
WHERE name LIKE 'thread/sql%'
ORDER BY name
unified_parent_thread_id	unified parent_thread_id
role	NULL
instrumented	YES
name	thread/sql/signal_handler
type	BACKGROUND
processlist_user	NULL
processlist_host	NULL
processlist_db	NULL
processlist_command	NULL
processlist_info	NULL
unified_parent_thread_id	unified parent_thread_id
role	NULL
instrumented	YES
CREATE TEMPORARY TABLE t1 AS
SELECT thread_id FROM performance_schema.threads
WHERE name LIKE 'thread/sql%';
SET GLOBAL event_scheduler = ON;
SELECT name, type, processlist_user, processlist_host, processlist_db,
processlist_command, processlist_info,
IF(parent_thread_id IS NULL, parent_thread_id, 'unified parent_thread_id')
AS unified_parent_thread_id,
`role`, instrumented
FROM performance_schema.threads
WHERE name LIKE 'thread/sql%'
  AND thread_id NOT IN (SELECT thread_id FROM t1)
ORDER BY name;
name	thread/sql/event_scheduler
type	FOREGROUND
processlist_user	event_scheduler
processlist_host	localhost
processlist_db	NULL
processlist_command	Daemon
processlist_info	NULL
unified_parent_thread_id	unified parent_thread_id
role	NULL
instrumented	YES
TRUNCATE t1;
INSERT INTO t1
SELECT thread_id FROM performance_schema.threads
WHERE name LIKE 'thread/sql%';
SELECT COUNT(*) INTO @aux FROM t1;
DROP EVENT IF EXISTS t_ps_event;
CREATE EVENT t_ps_event
ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 SECOND
DO SELECT SLEEP(3);
SELECT name, type, processlist_user, processlist_host, processlist_db,
processlist_command, processlist_info,
IF(parent_thread_id IS NULL, parent_thread_id, 'unified parent_thread_id')
AS unified_parent_thread_id,
`role`, instrumented
FROM performance_schema.threads
WHERE name LIKE 'thread/sql%'
  AND thread_id NOT IN (SELECT thread_id FROM t1)
ORDER BY name;
name	thread/sql/event_worker
type	FOREGROUND
processlist_user	event_scheduler
processlist_host	localhost
processlist_db	test
processlist_command	Connect
processlist_info	SELECT SLEEP(3)
unified_parent_thread_id	unified parent_thread_id
role	NULL
instrumented	YES
SELECT t2.name AS parent_thread_name, t1.name AS child_thread_name
FROM performance_schema.threads t1 INNER JOIN performance_schema.threads t2
ON t1.parent_thread_id = t2.thread_id
WHERE t1.name LIKE 'thread/sql%'
  AND t1.parent_thread_id IS NOT NULL
ORDER BY parent_thread_name, child_thread_name;
parent_thread_name	child_thread_name
thread/sql/event_scheduler	thread/sql/event_worker
thread/sql/main	thread/sql/compress_gtid_table
thread/sql/main	thread/sql/one_connection
thread/sql/main	thread/sql/signal_handler
thread/sql/one_connection	thread/sql/event_scheduler
