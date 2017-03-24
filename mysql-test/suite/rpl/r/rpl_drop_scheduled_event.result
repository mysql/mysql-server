include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET @saved_event_scheduler= @@GLOBAL.event_scheduler;
SET GLOBAL EVENT_SCHEDULER= OFF;
CREATE EVENT e1
ON SCHEDULE EVERY 1 SECOND
STARTS NOW() ENDS NOW() + INTERVAL 2 SECOND
ON COMPLETION NOT PRESERVE
DO SELECT 1;
SET GLOBAL EVENT_SCHEDULER= ON;
Wait until the event is dropped to disable the event scheduler again.
SET GLOBAL EVENT_SCHEDULER= OFF;
include/sync_slave_sql_with_master.inc
[connection master]
CREATE EVENT e1
ON SCHEDULE EVERY 1 SECOND
STARTS NOW() ENDS NOW() + INTERVAL 3 SECOND
ON COMPLETION NOT PRESERVE
DO SELECT 1;
include/sync_slave_sql_with_master.inc
[connection master]
DROP EVENT e1;
SET GLOBAL EVENT_SCHEDULER= @saved_event_scheduler;
include/rpl_end.inc
