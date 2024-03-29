drop function if exists test.f1;
drop procedure if exists test.p1;
create function test.f1() returns int
begin
call test.p1();
return 1;
end
$$
create procedure test.p1()
begin end;
begin;
select test.f1();
test.f1()
1
drop procedure test.p1;
call dump_one_thread('user1');
username	event_name	sql_text
user1	statement/sql/select	select test.f1()
username	event_name	nesting_event_type
username	event_name	nesting_event_type
call dump_one_thread('user2');
username	event_name	sql_text
user2	statement/sql/drop_procedure	drop procedure test.p1
username	event_name	nesting_event_type
user2	stage/sql/Waiting for stored procedure metadata lock	STATEMENT
username	event_name	nesting_event_type
user2	stage/sql/starting	STATEMENT
user2	stage/sql/Executing hook on transaction begin.	STATEMENT
user2	stage/sql/starting	STATEMENT
select test.f1();
test.f1()
1
commit;
drop function test.f1;
