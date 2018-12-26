set global event_scheduler=off;
drop database if exists events_test;
create database events_test;
use events_test;
create table execution_log(name char(10));
create event abc1 on schedule every 1 second do
insert into execution_log value('abc1');
create event abc2 on schedule every 1 second do
insert into execution_log value('abc2');
create event abc3 on schedule every 1 second do 
insert into execution_log value('abc3');
"Now we restart the server"
# restart
use events_test;
select @@event_scheduler;
@@event_scheduler
ON
drop table execution_log;
drop database events_test;
#
# Test for bug#11748899 -- EVENT SET TO DISABLED AND ON COMPLETION
#                          NOT PRESERVE IS DELETED AT SERVER
#
SELECT @@event_scheduler;
@@event_scheduler
ON
USE test;
DROP EVENT IF EXISTS e1;
CREATE EVENT e1 ON SCHEDULE EVERY 1 SECOND DISABLE DO SELECT 1;
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
test	e1	root@localhost	SYSTEM	RECURRING	#	1	SECOND	#	#	DISABLED	1	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
"Now we restart the server"
# restart
USE test;
SELECT @@event_scheduler;
@@event_scheduler
ON
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
test	e1	root@localhost	SYSTEM	RECURRING	#	1	SECOND	#	#	DISABLED	1	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
DROP EVENT e1;
# end test for bug#11748899
