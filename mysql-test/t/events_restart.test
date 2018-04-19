# Switch off the scheduler for now.
set global event_scheduler=off;

--disable_warnings
drop database if exists events_test;
--enable_warnings
create database events_test;
use events_test;
create table execution_log(name char(10));

create event abc1 on schedule every 1 second do
  insert into execution_log value('abc1');
create event abc2 on schedule every 1 second do
  insert into execution_log value('abc2');
create event abc3 on schedule every 1 second do 
  insert into execution_log value('abc3');
--echo "Now we restart the server"

--source include/restart_mysqld.inc

use events_test;
# Make sure the scheduler was started successfully
select @@event_scheduler;
let $wait_condition=select count(distinct name)=3 from execution_log;
--source include/wait_condition.inc
drop table execution_log;
# Will drop all events
drop database events_test;

let $wait_condition=
  select count(*) = 0 from information_schema.processlist
  where db='events_test' and command = 'Connect' and user=current_user();
--source include/wait_condition.inc

--echo #
--echo # Test for bug#11748899 -- EVENT SET TO DISABLED AND ON COMPLETION
--echo #                          NOT PRESERVE IS DELETED AT SERVER
--echo #
SELECT @@event_scheduler;
USE test;
--disable_warnings
DROP EVENT IF EXISTS e1;
--enable_warnings
CREATE EVENT e1 ON SCHEDULE EVERY 1 SECOND DISABLE DO SELECT 1; 
--replace_column 6 # 9 # 10 #
SHOW EVENTS;

--echo "Now we restart the server"
--source include/restart_mysqld.inc
USE test;
SELECT @@event_scheduler;
--replace_column 6 # 9 # 10 #
SHOW EVENTS;
DROP EVENT e1;

--echo # end test for bug#11748899
