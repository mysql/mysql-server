--echo #
--echo # Bug#31633859: IN GR ENVIRONMENT WITH POOR RESOURCES,
--echo #               EVENT_SCHEDULER DOSEN'T WORK PROPERLY.
--echo #
--echo

--echo # Set up.
--echo

# We need to have performance_schema.error_log to ourselves:
--source include/not_parallel.inc

--echo # Make sure performance_schema.error_log and session agree on the timezone.
SET @@session.time_zone=@@global.log_timestamps;

--echo # Find (timestamp of) most recent row in performance_schema.error_log:
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
  INTO @pfs_errlog_latest
  FROM performance_schema.global_status
 WHERE VARIABLE_NAME LIKE "Error_log_latest_write";

--echo # Create a test-user that is not root.
CREATE USER 'user31633859'@'127.0.0.1';
GRANT SELECT, EVENT, RELOAD ON *.* TO 'user31633859'@'127.0.0.1';

--echo # Show that the event scheduler is running:
SELECT COUNT(thread_id)
  FROM performance_schema.threads
 WHERE name='thread/sql/event_scheduler';

--echo # Create an event.
CREATE DEFINER='user31633859'@'127.0.0.1' EVENT
  IF NOT EXISTS
  mysql.event31633859
  ON SCHEDULE EVERY 1 SECOND
  DO SET @dummy=1;

--echo # Create an environment where the event will fail.
SET @@global.offline_mode=ON;
SET @@global.super_read_only=ON;

--echo # Show that the event exists:
SELECT definer,event_name
  FROM information_schema.events
 WHERE event_schema='mysql'
   AND event_name='event31633859';

--echo # Wait till the event scheduler is no longer running:
let $wait_condition=
  SELECT COUNT(thread_id)=0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';
--source include/wait_condition.inc

--echo # Show state of relevant configuration:
SELECT @@global.offline_mode;
SELECT @@global.super_read_only;

--echo # Show the error log entry for the failed event:
SELECT error_code,data
  FROM performance_schema.error_log
 WHERE error_code='MY-010045' AND logged>@pfs_errlog_latest;

--echo # Show that the event scheduler has stopped:
SELECT COUNT(thread_id)
  FROM performance_schema.threads
 WHERE name='thread/sql/event_scheduler';

--echo # Show that the scheduler is still switched on (indicating that the user
--echo # wishes for it to run, not that it's actually running):
SELECT @@global.event_scheduler;

--echo
--echo # Show that we now restart the scheduler when turning off SUPER_READ_ONLY.
--echo

SET @@global.super_read_only=OFF;

--echo # Wait till the event scheduler is running again:
let $wait_condition=
  SELECT COUNT(thread_id)>0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';
--source include/wait_condition.inc

--echo # Show that the event scheduler is now running again:
SELECT COUNT(thread_id)>0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';

--echo #
--echo # Bug#33539082: BUG#31633859 does not consider SET @@global.read_only=OFF
--echo #
--echo

--echo # Set up.
--echo

--echo # Find (timestamp of) most recent row in performance_schema.error_log:
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
  INTO @pfs_errlog_latest
  FROM performance_schema.global_status
 WHERE VARIABLE_NAME LIKE "Error_log_latest_write";

--echo # Create an environment where the event (created for 31633859) will fail.
SET @@global.offline_mode=ON;
SET @@global.read_only=ON;
SET @@global.super_read_only=ON;

--echo # Wait till the event scheduler is no longer running:
let $wait_condition=
  SELECT COUNT(thread_id)=0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';
--source include/wait_condition.inc

--echo # Show state of relevant configuration:
SELECT @@global.offline_mode;
SELECT @@global.super_read_only;
SELECT @@global.read_only;

--echo # Show the error log entry for the failed event:
SELECT error_code,data
  FROM performance_schema.error_log
 WHERE error_code='MY-010045' AND logged>@pfs_errlog_latest;

--echo # Show that the event scheduler has stopped:
SELECT COUNT(thread_id)
  FROM performance_schema.threads
 WHERE name='thread/sql/event_scheduler';

--echo # Show that the scheduler is still switched on (indicating that the user
--echo # wishes for it to run, not that it's actually running):
SELECT @@global.event_scheduler;

--echo
--echo # Show that we now restart the scheduler when turning off READ_ONLY.
--echo # (Turning off READ_ONLY implicitly unsets SUPER_READ_ONLY as well.)
--echo

SET @@global.read_only=OFF;

--echo # Wait till the event scheduler is running again:
let $wait_condition=
  SELECT COUNT(thread_id)>0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';
--source include/wait_condition.inc

--echo # Show that the event scheduler is now running again:
SELECT COUNT(thread_id)>0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';

--echo # Show state of relevant configuration:
SELECT @@global.offline_mode;
SELECT @@global.super_read_only;
SELECT @@global.read_only;

--echo
--echo # Clean up.
--echo

SET @@global.read_only=OFF;
SET @@global.offline_mode=OFF;

DROP EVENT mysql.event31633859;
DROP USER 'user31633859'@'127.0.0.1';


--echo #
--echo # Bug#33711304: Event Scheduler fails to restart after LOCK INSTANCE FOR BACKUP
--echo #

--echo
--echo # Set up.

--echo
--echo # Find (timestamp of) most recent row in performance_schema.error_log:
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
  INTO @pfs_errlog_latest
  FROM performance_schema.global_status
 WHERE VARIABLE_NAME LIKE "Error_log_latest_write";

--echo
--echo # Create a test-user that is not root.
CREATE USER 'user33711304'@'127.0.0.1';
GRANT SELECT, EVENT, INSERT, RELOAD ON *.* TO 'user33711304'@'127.0.0.1';

--echo
--echo # Create test table for the event to write to.
CREATE table t1 (f1 INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, f2 TIMESTAMP);

--echo
--echo # Create an event.
CREATE DEFINER='user33711304'@'127.0.0.1' EVENT
  IF NOT EXISTS
  mysql.event33711304
  ON SCHEDULE EVERY 1 SECOND
  DO INSERT INTO test.t1 VALUES (NULL, CURRENT_TIMESTAMP);

--echo
--echo # Wait for event to run.
let $wait_timeout= 60;
let $wait_condition= SELECT COUNT(*)>0 FROM t1;
--source include/wait_condition.inc

--echo
--echo # Show that no scheduler failures have happened yet (should be empty).
SELECT error_code,data
  FROM performance_schema.error_log
 WHERE error_code='MY-010045' AND logged>@pfs_errlog_latest;

--echo
--echo # Show scheduler state (e.g. "Waiting for next activation").
SELECT PROCESSLIST_STATE
  FROM performance_schema.threads
 WHERE name='thread/sql/event_scheduler';

--echo
--echo # Create an environment where the event will fail.
LOCK INSTANCE FOR BACKUP;
FLUSH TABLES WITH READ LOCK;

--echo
--echo # Count rows in table.
SELECT COUNT(*) FROM t1 INTO @rows_before;
--echo # Should be non-empty ("1"):
SELECT @rows_before > 0;

--echo
--echo # Give event time to fire.
--echo # This will fail gracefully on very slow systems.
SELECT SLEEP(2);

--echo
--echo # Count rows in table (again).
SELECT COUNT(*) FROM t1 INTO @rows_after;

--echo
--echo # No new rows should have been added ("0"):
SELECT @rows_after - @rows_before;

--echo
--echo # Show that the scheduler has not failed.
SELECT error_code,data
  FROM performance_schema.error_log
 WHERE error_code='MY-010045' AND logged>@pfs_errlog_latest LIMIT 1;

--echo
--echo # Show scheduler state ("Waiting for global read lock")
SELECT PROCESSLIST_STATE
  FROM performance_schema.threads
 WHERE name='thread/sql/event_scheduler';

--echo
--echo # Unlock tables. Scheduler should restart.
UNLOCK INSTANCE;
UNLOCK TABLES;

--echo
--echo # Wait for event to run.
let $wait_timeout= 60;
let $wait_condition= SELECT (COUNT(*)-@rows_after)>0 FROM t1;
--source include/wait_condition.inc

--echo
--echo # Show that no error was recorded:
SELECT error_code,data
  FROM performance_schema.error_log
 WHERE error_code='MY-010045' AND logged>@pfs_errlog_latest;

--echo
--echo # Show that the event scheduler thread still exists.
SELECT COUNT(thread_id)
  FROM performance_schema.threads
 WHERE name='thread/sql/event_scheduler';

--echo
--echo # Show scheduler state ("Waiting for next activation")
SELECT PROCESSLIST_STATE
  FROM performance_schema.threads
 WHERE name='thread/sql/event_scheduler';

--echo
--echo # Show that the scheduler is still switched on (indicating that the user
--echo # wishes for it to run, not that it's actually running):
SELECT @@global.event_scheduler;

--echo
--echo # Clean up.
--echo

DROP EVENT mysql.event33711304;
DROP TABLE t1;
DROP USER 'user33711304'@'127.0.0.1';

--echo # Done.
