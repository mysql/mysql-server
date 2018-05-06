#
# Bug #20476395 DICT_LOAD_FOREIGNS() FAILED IN
# COMMIT_INPLACE_ALTER_TABLE
#
call mtr.add_suppression("Failed to load table");
create database bug;
use bug;
create table parent(a serial) engine=innodb;
create table child(a serial, foreign key fk (a) references parent(a));
insert into parent values(1);
insert into child values(1);
SET DEBUG_SYNC='innodb_rename_table_ready SIGNAL s1 WAIT_FOR s2 EXECUTE 2';
ALTER TABLE child ROW_FORMAT=DYNAMIC, ALGORITHM=COPY;
SET DEBUG_SYNC='now WAIT_FOR s1';
SET DEBUG_SYNC='now SIGNAL s2 WAIT_FOR s1';
# Kill and restart
show tables;
Tables_in_bug
child
parent
alter table parent row_format=dynamic;
drop table child;
drop table parent;
drop database bug;
