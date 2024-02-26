--source include/force_myisam_default.inc
--source include/have_myisam.inc

delimiter |;
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
delimiter ;|
set @reprepare_count= 0;
flush status;


--echo # SQLCOM_REPAIR:

create table t1 (a int) engine=MyISAM;

insert into t1 values (1), (2), (3);

prepare stmt from "repair table t1";

execute stmt;

drop table t1;
create table t1 (a1 int, a2 int) engine=myisam;
insert into t1 values (1, 10), (2, 20), (3, 30);

--echo # t1 has changed, and it's does not lead to reprepare
execute stmt;

alter table t1 add column b varchar(50) default NULL;
execute stmt;
call p_verify_reprepare_count(0);

alter table t1 drop column b;
execute stmt;
call p_verify_reprepare_count(0);

--echo # SQLCOM_ANALYZE:

prepare stmt from "analyze table t1";
execute stmt;

drop table t1;
create table t1 (a1 int, a2 int) engine=myisam;
insert into t1 values (1, 10), (2, 20), (3, 30);
--echo # t1 has changed, and it's not a problem
execute stmt;

alter table t1 add column b varchar(50) default NULL;
execute stmt;

alter table t1 drop column b;
execute stmt;

call p_verify_reprepare_count(0);

--echo # SQLCOM_OPTIMIZE:

prepare stmt from "optimize table t1";
execute stmt;

drop table t1;
create table t1 (a1 int, a2 int) engine=myisam;
insert into t1 values (1, 10), (2, 20), (3, 30);

--echo # t1 has changed, and it's not a problem
execute stmt;

alter table t1 add column b varchar(50) default NULL;
execute stmt;

alter table t1 drop column b;
execute stmt;
call p_verify_reprepare_count(0);

drop table t1;
drop procedure p_verify_reprepare_count;
