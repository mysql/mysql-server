
CREATE DATABASE IF NOT EXISTS events_test;
USE events_test;

SET @event_scheduler=@@global.event_scheduler;
SET GLOBAL event_scheduler=OFF;
--echo Try again to make sure it's allowed
SET GLOBAL event_scheduler=OFF;
SHOW VARIABLES LIKE 'event_scheduler';
SET GLOBAL event_scheduler=1;
SHOW VARIABLES LIKE 'event_scheduler';
SET GLOBAL event_scheduler=0;
SHOW VARIABLES LIKE 'event_scheduler';
SET GLOBAL event_scheduler=ON;
--echo Try again to make sure it's allowed
SET GLOBAL event_scheduler=ON;
SHOW VARIABLES LIKE 'event_scheduler';
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL event_scheduler=DISABLED;
SHOW VARIABLES LIKE 'event_scheduler';
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL event_scheduler=-1;
SHOW VARIABLES LIKE 'event_scheduler';
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL event_scheduler=2;
SHOW VARIABLES LIKE 'event_scheduler';
--error ER_WRONG_VALUE_FOR_VAR
SET GLOBAL event_scheduler=5;
SHOW VARIABLES LIKE 'event_scheduler';

CREATE TABLE table_1(a int);
CREATE TABLE table_2(a int);
CREATE TABLE table_3(a int);
CREATE TABLE table_4(a int);

SET GLOBAL event_scheduler=ON;
# We need to have 2 to make it safe with valgrind. This is probably because
# of when we calculate the timestamp value
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

# Let event_1 insert at least 4 records into the table
let $wait_condition=select count(*) >= 4 from table_1;
--source include/wait_condition.inc
# Minimum of passed time is 6 seconds assuming
# - event executions starts immediate after creation
# - 4 times event_1 means an insert at ect, ect+2, ect+4, ect+6
# ect = event creation time

# Let event_2 reach the end of its execution interval
let $wait_condition=select count(*) = 1 from information_schema.events
where event_name='event_2' and status='DISABLED';
--source include/wait_condition.inc
# Minimum of passed time is 6 seconds.
# See wait_condition for event_1 above and ENDS condition for event_2.

# Let event_3, which is ON COMPLETION NOT PRESERVE execute and drop itself
let $wait_condition=select count(*) = 0 from information_schema.events
where event_name='event_3';
--source include/wait_condition.inc

# Let event_4 reach the end of its execution interval
let $wait_condition=select count(*) = 1 from information_schema.events
where event_name='event_4' and status='DISABLED';
--source include/wait_condition.inc

#
# On a busy system the scheduler may skip execution of events,
# we can't reliably expect that the data in a table to be modified
# by an event will be exact. Thus we do not SELECT from the tables
# in this test. See also
#    Bug#39854 events_scheduling fails sporadically on pushbuild
#

SELECT IF(TIME_TO_SEC(TIMEDIFF(ENDS,STARTS))=6, 'OK', 'ERROR')
FROM INFORMATION_SCHEMA.EVENTS
WHERE EVENT_SCHEMA=DATABASE() AND EVENT_NAME='event_2';

--echo "Already dropped because ended. Therefore an error."
--error ER_EVENT_DOES_NOT_EXIST
DROP EVENT event_3;

DROP EVENT event_1;
--echo "Should be preserved"
SELECT EVENT_NAME, STATUS FROM INFORMATION_SCHEMA.EVENTS ORDER BY EVENT_NAME;
DROP EVENT event_2;
DROP EVENT event_4;
DROP TABLE table_1;
DROP TABLE table_2;
DROP TABLE table_3;
DROP TABLE table_4;

-- echo
-- echo Bug #50087 Interval arithmetic for Event_queue_element is not portable.
-- echo

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

#
# End of tests
#

let $wait_condition=
  select count(*) = 0 from information_schema.processlist
  where db='events_test' and command = 'Connect' and user=current_user();
--source include/wait_condition.inc

