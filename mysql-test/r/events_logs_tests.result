drop database if exists events_test;
create database if not exists events_test;
use events_test;

We use procedure here because its statements won't be
logged into the general log. If we had used normal select
that are logged in different ways depending on whether the
test suite is run in normal mode or with --ps-protocol

create procedure select_general_log()
begin
select user_host, argument from mysql.general_log
where argument like '%events_logs_test%';
end|

Check that general query log works, but sub-statements
of the stored procedure do not leave traces in it.

truncate mysql.general_log;
select 'events_logs_tests' as outside_event;
outside_event
events_logs_tests
call select_general_log();
user_host	argument
USER_HOST	select 'events_logs_tests' as outside_event

Check that unlike sub-statements of stored procedures,
sub-statements of events are present in the general log.

SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE user = 'event_scheduler' AND command = 'Daemon';
COUNT(*) = 1
1
truncate mysql.general_log;
create event ev_log_general on schedule at now() on completion not preserve do select 'events_logs_test' as inside_event;
call select_general_log();
user_host	argument
USER_HOST	create event ev_log_general on schedule at now() on completion not preserve do select 'events_logs_test' as inside_event
USER_HOST	select 'events_logs_test' as inside_event

Check slow query log

Ensure that slow logging is on
show variables like 'slow_query_log';
Variable_name	Value
slow_query_log	ON

Demonstrate that session value has no effect

set @@session.long_query_time=1;
set @@global.long_query_time=300;
truncate mysql.slow_log;
create event ev_log_general on schedule at now() on completion not preserve
do select 'events_logs_test' as inside_event, sleep(1.5);

Nothing should be logged

select user_host, db, sql_text from mysql.slow_log
where sql_text like 'select \'events_logs_test\'%';
user_host	db	sql_text
set @@global.long_query_time=1;
truncate mysql.slow_log;
create event ev_log_general on schedule at now() on completion not preserve
do select 'events_logs_test' as inside_event, sleep(1.5);

Event sub-statement should be logged.

select user_host, db, sql_text from mysql.slow_log
where sql_text like 'select \'events_logs_test\'%';
user_host	db	sql_text
USER_HOST	events_test	select 'events_logs_test' as inside_event, sleep(1.5)
drop database events_test;
set @@global.long_query_time=default;
set @@session.long_query_time=default;
