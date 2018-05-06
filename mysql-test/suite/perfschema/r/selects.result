UPDATE performance_schema.setup_instruments SET enabled = 'YES', timed = 'YES';
UPDATE performance_schema.setup_threads SET ENABLED='NO';
UPDATE performance_schema.threads SET INSTRUMENTED='NO';
UPDATE performance_schema.threads
SET INSTRUMENTED='YES' WHERE PROCESSLIST_ID = connection_id();
UPDATE performance_schema.setup_instruments SET enabled = 'NO', timed = 'NO'
  where NAME='wait/io/table/sql/handler';
TRUNCATE TABLE performance_schema.events_waits_current;
TRUNCATE TABLE performance_schema.events_waits_history;
TRUNCATE TABLE performance_schema.events_waits_history_long;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (id INT PRIMARY KEY, b CHAR(100) DEFAULT 'initial value')
ENGINE=MyISAM;
INSERT INTO t1 (id) VALUES (1), (2), (3), (4), (5), (6), (7), (8);
SELECT OPERATION, SUM(NUMBER_OF_BYTES) AS TOTAL
FROM performance_schema.events_waits_history_long
GROUP BY OPERATION
HAVING TOTAL IS NOT NULL
ORDER BY OPERATION
LIMIT 1;
OPERATION	TOTAL
chsize	[NUM_BYTES]
SELECT EVENT_ID FROM performance_schema.events_waits_current
WHERE THREAD_ID IN
(SELECT THREAD_ID FROM performance_schema.threads)
AND EVENT_NAME IN
(SELECT NAME FROM performance_schema.setup_instruments
WHERE NAME LIKE "wait/synch/%")
LIMIT 1;
EVENT_ID
[EVENT_ID]
SELECT DISTINCT EVENT_ID
FROM performance_schema.events_waits_current
JOIN performance_schema.events_waits_history USING (EVENT_ID)
JOIN performance_schema.events_waits_history_long USING (EVENT_ID)
ORDER BY EVENT_ID
LIMIT 1;
EVENT_ID
[EVENT_ID]
SELECT t1.THREAD_ID, t2.EVENT_ID, t3.EVENT_NAME, t4.TIMER_WAIT
FROM performance_schema.events_waits_history t1
JOIN performance_schema.events_waits_history t2 USING (EVENT_ID)
JOIN performance_schema.events_waits_history t3 ON (t2.THREAD_ID = t3.THREAD_ID)
JOIN performance_schema.events_waits_history t4 ON (t3.EVENT_NAME = t4.EVENT_NAME)
ORDER BY t1.EVENT_ID, t2.EVENT_ID
LIMIT 5;
THREAD_ID	EVENT_ID	EVENT_NAME	TIMER_WAIT
[THREAD_ID]	[EVENT_ID]	[EVENT_NAME]	[TIMER_WAIT]
[THREAD_ID]	[EVENT_ID]	[EVENT_NAME]	[TIMER_WAIT]
[THREAD_ID]	[EVENT_ID]	[EVENT_NAME]	[TIMER_WAIT]
[THREAD_ID]	[EVENT_ID]	[EVENT_NAME]	[TIMER_WAIT]
[THREAD_ID]	[EVENT_ID]	[EVENT_NAME]	[TIMER_WAIT]
SELECT THREAD_ID, EVENT_ID FROM (
SELECT THREAD_ID, EVENT_ID FROM performance_schema.events_waits_current
UNION
SELECT THREAD_ID, EVENT_ID FROM performance_schema.events_waits_history
UNION
SELECT THREAD_ID, EVENT_ID FROM performance_schema.events_waits_history_long
) t1 ORDER BY THREAD_ID, EVENT_ID
LIMIT 5;
THREAD_ID	EVENT_ID
[THREAD_ID]	[EVENT_ID]
[THREAD_ID]	[EVENT_ID]
[THREAD_ID]	[EVENT_ID]
[THREAD_ID]	[EVENT_ID]
[THREAD_ID]	[EVENT_ID]
DROP TABLE IF EXISTS t_event;
DROP EVENT IF EXISTS t_ps_event;
CREATE TABLE t_event AS
SELECT EVENT_ID FROM performance_schema.events_waits_current
WHERE 1 = 2;
CREATE EVENT t_ps_event
ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 SECOND
ON COMPLETION PRESERVE
DO INSERT INTO t_event
SELECT DISTINCT EVENT_ID
FROM performance_schema.events_waits_current
ORDER BY EVENT_ID
LIMIT 1;
ALTER TABLE t1 ADD COLUMN c INT;
DROP TRIGGER IF EXISTS t_ps_trigger;
CREATE TRIGGER t_ps_trigger BEFORE INSERT ON t1
FOR EACH ROW BEGIN
SET NEW.c = (SELECT MAX(EVENT_ID)
FROM performance_schema.events_waits_current);
END;
|
INSERT INTO t1 (id) VALUES (11), (12), (13);
SELECT id, c FROM t1 WHERE id > 10 ORDER BY id, c;
id	c
11	[EVENT_ID]
12	[EVENT_ID]
13	[EVENT_ID]
DROP TRIGGER t_ps_trigger;
DROP PROCEDURE IF EXISTS t_ps_proc;
CREATE PROCEDURE t_ps_proc(IN conid INT, OUT pid INT)
BEGIN
SELECT thread_id FROM performance_schema.threads
WHERE PROCESSLIST_ID = conid INTO pid;
END;
|
CALL t_ps_proc(connection_id(), @p_id);
DROP FUNCTION IF EXISTS t_ps_proc;
CREATE FUNCTION t_ps_func(conid INT) RETURNS int
BEGIN
return (SELECT thread_id FROM performance_schema.threads
WHERE PROCESSLIST_ID = conid);
END;
|
SELECT t_ps_func(connection_id()) = @p_id;
t_ps_func(connection_id()) = @p_id
1
SELECT * FROM t_event;
EVENT_ID
[EVENT_ID]
DROP PROCEDURE t_ps_proc;
DROP FUNCTION t_ps_func;
DROP EVENT t_ps_event;
DROP TABLE t1;
DROP TABLE t_event;
UPDATE performance_schema.setup_threads SET ENABLED='YES';
UPDATE performance_schema.threads SET INSTRUMENTED='YES';
UPDATE performance_schema.setup_instruments SET enabled = 'YES', timed = 'YES';
