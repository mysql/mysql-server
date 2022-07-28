#
# Bug#31633859: IN GR ENVIRONMENT WITH POOR RESOURCES,
#               EVENT_SCHEDULER DOSEN'T WORK PROPERLY.
#

# Set up.

# Make sure performance_schema.error_log and session agree on the timezone.
SET @@session.time_zone=@@global.log_timestamps;
# Find (timestamp of) most recent row in performance_schema.error_log:
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
INTO @pfs_errlog_latest
FROM performance_schema.global_status
WHERE VARIABLE_NAME LIKE "Error_log_latest_write";
# Create a test-user that is not root.
CREATE USER 'user31633859'@'127.0.0.1';
GRANT SELECT, EVENT, RELOAD ON *.* TO 'user31633859'@'127.0.0.1';
# Show that the event scheduler is running:
SELECT COUNT(thread_id)
FROM performance_schema.threads
WHERE name='thread/sql/event_scheduler';
COUNT(thread_id)
1
# Create an event.
CREATE DEFINER='user31633859'@'127.0.0.1' EVENT
IF NOT EXISTS
mysql.event31633859
ON SCHEDULE EVERY 1 SECOND
DO SET @dummy=1;
# Create an environment where the event will fail.
SET @@global.offline_mode=ON;
SET @@global.super_read_only=ON;
# Show that the event exists:
SELECT definer,event_name
FROM information_schema.events
WHERE event_schema='mysql'
   AND event_name='event31633859';
DEFINER	EVENT_NAME
user31633859@127.0.0.1	event31633859
# Wait till the event scheduler is no longer running:
# Show state of relevant configuration:
SELECT @@global.offline_mode;
@@global.offline_mode
1
SELECT @@global.super_read_only;
@@global.super_read_only
1
# Show the error log entry for the failed event:
SELECT error_code,data
FROM performance_schema.error_log
WHERE error_code='MY-010045' AND logged>=@pfs_errlog_latest;
error_code	data
MY-010045	Event Scheduler: Unable to schedule event: The MySQL server is running with the --super-read-only option so it cannot execute this statement
# Show that the event scheduler has stopped:
SELECT COUNT(thread_id)
FROM performance_schema.threads
WHERE name='thread/sql/event_scheduler';
COUNT(thread_id)
0
# Show that the scheduler is still switched on (indicating that the user
# wishes for it to run, not that it's actually running):
SELECT @@global.event_scheduler;
@@global.event_scheduler
ON

# Show that we now restart the scheduler when turning off SUPER_READ_ONLY.

SET @@global.super_read_only=OFF;
# Wait till the event scheduler is running again:
# Show that the event scheduler is now running again:
SELECT COUNT(thread_id)>0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';
COUNT(thread_id)>0
1
#
# Bug#33539082: BUG#31633859 does not consider SET @@global.read_only=OFF
#

# Set up.

SELECT logged
FROM performance_schema.error_log
WHERE error_code='MY-010045' AND logged>=@pfs_errlog_latest
INTO @pfs_errlog_latest;
# Create an environment where the event (created for 31633859) will fail.
SET @@global.offline_mode=ON;
SET @@global.read_only=ON;
SET @@global.super_read_only=ON;
# Wait till the event scheduler is no longer running:
# Show state of relevant configuration:
SELECT @@global.offline_mode;
@@global.offline_mode
1
SELECT @@global.super_read_only;
@@global.super_read_only
1
SELECT @@global.read_only;
@@global.read_only
1
# Show the error log entry for the failed event:
SELECT error_code,data
FROM performance_schema.error_log
WHERE error_code='MY-010045' AND logged>@pfs_errlog_latest;
error_code	data
MY-010045	Event Scheduler: Unable to schedule event: The MySQL server is running with the --super-read-only option so it cannot execute this statement
# Show that the event scheduler has stopped:
SELECT COUNT(thread_id)
FROM performance_schema.threads
WHERE name='thread/sql/event_scheduler';
COUNT(thread_id)
0
# Show that the scheduler is still switched on (indicating that the user
# wishes for it to run, not that it's actually running):
SELECT @@global.event_scheduler;
@@global.event_scheduler
ON

# Show that we now restart the scheduler when turning off READ_ONLY.
# (Turning off READ_ONLY implicitly unsets SUPER_READ_ONLY as well.)

SET @@global.read_only=OFF;
# Wait till the event scheduler is running again:
# Show that the event scheduler is now running again:
SELECT COUNT(thread_id)>0 FROM performance_schema.threads WHERE name='thread/sql/event_scheduler';
COUNT(thread_id)>0
1
# Show state of relevant configuration:
SELECT @@global.offline_mode;
@@global.offline_mode
1
SELECT @@global.super_read_only;
@@global.super_read_only
0
SELECT @@global.read_only;
@@global.read_only
0

# Clean up.

SET @@global.read_only=OFF;
SET @@global.offline_mode=OFF;
DROP EVENT mysql.event31633859;
DROP USER 'user31633859'@'127.0.0.1';
# Done.
