create procedure p_verify_reprepare_count(expected int)
begin
declare old_reprepare_count int default @reprepare_count;
select variable_value from
performance_schema.session_status where
variable_name='com_stmt_reprepare'
  into @reprepare_count;
if old_reprepare_count + expected <> @reprepare_count then
select concat("Expected: ", expected,
", actual: ", @reprepare_count - old_reprepare_count)
as "ERROR";
else
select '' as "SUCCESS";
end if;
end|
set @reprepare_count= 0;
flush status;
# SQLCOM_REPAIR:
create table t1 (a int) engine=MyISAM;
insert into t1 values (1), (2), (3);
prepare stmt from "repair table t1";
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
drop table t1;
create table t1 (a1 int, a2 int) engine=myisam;
insert into t1 values (1, 10), (2, 20), (3, 30);
# t1 has changed, and it's does not lead to reprepare
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
alter table t1 add column b varchar(50) default NULL;
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
call p_verify_reprepare_count(0);
SUCCESS

alter table t1 drop column b;
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
call p_verify_reprepare_count(0);
SUCCESS

# SQLCOM_ANALYZE:
prepare stmt from "analyze table t1";
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
drop table t1;
create table t1 (a1 int, a2 int) engine=myisam;
insert into t1 values (1, 10), (2, 20), (3, 30);
# t1 has changed, and it's not a problem
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
alter table t1 add column b varchar(50) default NULL;
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
alter table t1 drop column b;
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
call p_verify_reprepare_count(0);
SUCCESS

# SQLCOM_OPTIMIZE:
prepare stmt from "optimize table t1";
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	Table is already up to date
drop table t1;
create table t1 (a1 int, a2 int) engine=myisam;
insert into t1 values (1, 10), (2, 20), (3, 30);
# t1 has changed, and it's not a problem
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	OK
alter table t1 add column b varchar(50) default NULL;
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	OK
alter table t1 drop column b;
execute stmt;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	OK
call p_verify_reprepare_count(0);
SUCCESS

drop table t1;
drop procedure p_verify_reprepare_count;
