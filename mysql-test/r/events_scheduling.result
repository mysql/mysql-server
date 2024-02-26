CREATE DATABASE IF NOT EXISTS events_test;
USE events_test;
SET @event_scheduler=@@global.event_scheduler;
SET GLOBAL event_scheduler=OFF;
Try again to make sure it's allowed
SET GLOBAL event_scheduler=OFF;
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	OFF
SET GLOBAL event_scheduler=1;
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	ON
SET GLOBAL event_scheduler=0;
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	OFF
SET GLOBAL event_scheduler=ON;
Try again to make sure it's allowed
SET GLOBAL event_scheduler=ON;
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	ON
SET GLOBAL event_scheduler=DISABLED;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'DISABLED'
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	ON
SET GLOBAL event_scheduler=-1;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of '-1'
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	ON
SET GLOBAL event_scheduler=2;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of '2'
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	ON
SET GLOBAL event_scheduler=5;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of '5'
SHOW VARIABLES LIKE 'event_scheduler';
Variable_name	Value
event_scheduler	ON
CREATE TABLE table_1(a int);
CREATE TABLE table_2(a int);
CREATE TABLE table_3(a int);
CREATE TABLE table_4(a int);
SET GLOBAL event_scheduler=ON;
CREATE EVENT event_1 ON SCHEDULE EVERY 2 SECOND
DO
INSERT INTO table_1 VALUES (1);
CREATE EVENT event_2 ON SCHEDULE EVERY 1 SECOND
ENDS NOW() + INTERVAL 6 SECOND
ON COMPLETION PRESERVE
DO
INSERT INTO table_2 VALUES (1);
CREATE EVENT event_3 ON SCHEDULE EVERY 2 SECOND ENDS NOW() + INTERVAL 1 SECOND
ON COMPLETION NOT PRESERVE
DO
INSERT INTO table_3 VALUES (1);
CREATE EVENT event_4 ON SCHEDULE EVERY 1 SECOND ENDS NOW() + INTERVAL 1 SECOND
ON COMPLETION PRESERVE
DO
INSERT INTO table_4 VALUES (1);
SELECT IF(TIME_TO_SEC(TIMEDIFF(ENDS,STARTS))=6, 'OK', 'ERROR')
FROM INFORMATION_SCHEMA.EVENTS
WHERE EVENT_SCHEMA=DATABASE() AND EVENT_NAME='event_2';
IF(TIME_TO_SEC(TIMEDIFF(ENDS,STARTS))=6, 'OK', 'ERROR')
OK
"Already dropped because ended. Therefore an error."
DROP EVENT event_3;
ERROR HY000: Unknown event 'event_3'
DROP EVENT event_1;
"Should be preserved"
SELECT EVENT_NAME, STATUS FROM INFORMATION_SCHEMA.EVENTS ORDER BY EVENT_NAME;
EVENT_NAME	STATUS
event_2	DISABLED
event_4	DISABLED
DROP EVENT event_2;
DROP EVENT event_4;
DROP TABLE table_1;
DROP TABLE table_2;
DROP TABLE table_3;
DROP TABLE table_4;

Bug #50087 Interval arithmetic for Event_queue_element is not portable.

CREATE TABLE t1(a int);
CREATE EVENT e1 ON SCHEDULE EVERY 1 MONTH
STARTS NOW() - INTERVAL 1 MONTH
ENDS NOW() + INTERVAL 2 MONTH
ON COMPLETION PRESERVE
DO
INSERT INTO t1 VALUES (1);
CREATE EVENT e2 ON SCHEDULE EVERY 1 MONTH
STARTS NOW()
ENDS NOW() + INTERVAL 11 MONTH
ON COMPLETION PRESERVE
DO
INSERT INTO t1 VALUES (1);
DROP TABLE t1;
DROP EVENT e1;
DROP EVENT e2;
DROP DATABASE events_test;
SET GLOBAL event_scheduler=@event_scheduler;
