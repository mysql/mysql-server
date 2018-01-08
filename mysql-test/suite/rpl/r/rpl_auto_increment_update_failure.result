include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Test case1: INVOKES A TRIGGER with after insert action
create table t1(a int, b int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr1 after insert on t1 for each row insert into t2(a) values(6);
create table t3(a int, b int) engine=innodb;
create table t4(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t5(a int) engine=innodb;
create trigger tr2 after insert on t3 for each row begin
insert into t4(a) values(f1_insert_triggered());
insert into t4(a) values(f1_insert_triggered());
insert into t5(a) values(8);
end |
create table t6(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_insert_triggered() RETURNS INTEGER
BEGIN
INSERT INTO t6(a) values(2),(3);
RETURN 1;
END//
begin;
insert into t1(a,b) values(1,1),(2,1);
insert into t3(a,b) values(1,1),(2,1);
update t1 set a = a + 5 where b = 1;
update t3 set a = a + 5 where b = 1;
delete from t1 where b = 1;
delete from t3 where b = 1;
insert into t2(a) values(3);
insert into t4(a) values(3);
commit;
insert into t1(a,b) values(4,2);
insert into t3(a,b) values(4,2);
update t1 set a = a + 5 where b = 2;
update t3 set a = a + 5 where b = 2;
delete from t1 where b = 2;
delete from t3 where b = 2;
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; insert into t4(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 1
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 1
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 1
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 1
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INVOKES A TRIGGER with after insert action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t4, slave:t4]
include/diff_tables.inc [master:t6, slave:t6]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
DROP TABLE t6;
DROP FUNCTION f1_insert_triggered;
include/sync_slave_sql_with_master.inc
# Test case2: INVOKES A TRIGGER with before insert action
create table t1(a int, b int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr1 before insert on t1 for each row insert into t2(a) values(6);
create table t3(a int, b int) engine=innodb;
create table t4(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t5(a int) engine=innodb;
create trigger tr2 before insert on t3 for each row begin
insert into t4(a) values(f1_insert_triggered());
insert into t4(a) values(f1_insert_triggered());
insert into t5(a) values(8);
end |
create table t6(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_insert_triggered() RETURNS INTEGER
BEGIN
INSERT INTO t6(a) values(2),(3);
RETURN 1;
END//
begin;
insert into t1(a,b) values(1,1),(2,1);
insert into t3(a,b) values(1,1),(2,1);
update t1 set a = a + 5 where b = 1;
update t3 set a = a + 5 where b = 1;
delete from t1 where b = 1;
delete from t3 where b = 1;
insert into t2(a) values(3);
insert into t4(a) values(3);
commit;
insert into t1(a,b) values(4,2);
insert into t3(a,b) values(4,2);
update t1 set a = a + 5 where b = 2;
update t3 set a = a + 5 where b = 2;
delete from t1 where b = 2;
delete from t3 where b = 2;
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; insert into t4(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 1
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 1
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 1
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 1
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INVOKES A TRIGGER with before insert action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t4, slave:t4]
include/diff_tables.inc [master:t6, slave:t6]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
DROP TABLE t6;
DROP FUNCTION f1_insert_triggered;
include/sync_slave_sql_with_master.inc
# Test case3: INVOKES A TRIGGER with after update action
create table t1(a int, b int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr1 after update on t1 for each row insert into t2(a) values(6);
create table t3(a int, b int) engine=innodb;
create table t4(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t5(a int) engine=innodb;
create trigger tr2 after update on t3 for each row begin
insert into t4(a) values(f1_insert_triggered());
insert into t4(a) values(f1_insert_triggered());
insert into t5(a) values(8);
end |
create table t6(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_insert_triggered() RETURNS INTEGER
BEGIN
INSERT INTO t6(a) values(2),(3);
RETURN 1;
END//
begin;
insert into t1(a,b) values(1,1),(2,1);
insert into t3(a,b) values(1,1),(2,1);
update t1 set a = a + 5 where b = 1;
update t3 set a = a + 5 where b = 1;
delete from t1 where b = 1;
delete from t3 where b = 1;
insert into t2(a) values(3);
insert into t4(a) values(3);
commit;
insert into t1(a,b) values(4,2);
insert into t3(a,b) values(4,2);
update t1 set a = a + 5 where b = 2;
update t3 set a = a + 5 where b = 2;
delete from t1 where b = 2;
delete from t3 where b = 2;
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; insert into t4(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(1,1),(2,1)
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(1,1),(2,1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 1
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 1
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INVOKES A TRIGGER with after update action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t4, slave:t4]
include/diff_tables.inc [master:t6, slave:t6]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
DROP TABLE t6;
DROP FUNCTION f1_insert_triggered;
include/sync_slave_sql_with_master.inc
# Test case4: INVOKES A TRIGGER with before update action
create table t1(a int, b int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr1 before update on t1 for each row insert into t2(a) values(6);
create table t3(a int, b int) engine=innodb;
create table t4(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t5(a int) engine=innodb;
create trigger tr2 before update on t3 for each row begin
insert into t4(a) values(f1_insert_triggered());
insert into t4(a) values(f1_insert_triggered());
insert into t5(a) values(8);
end |
create table t6(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_insert_triggered() RETURNS INTEGER
BEGIN
INSERT INTO t6(a) values(2),(3);
RETURN 1;
END//
begin;
insert into t1(a,b) values(1,1),(2,1);
insert into t3(a,b) values(1,1),(2,1);
update t1 set a = a + 5 where b = 1;
update t3 set a = a + 5 where b = 1;
delete from t1 where b = 1;
delete from t3 where b = 1;
insert into t2(a) values(3);
insert into t4(a) values(3);
commit;
insert into t1(a,b) values(4,2);
insert into t3(a,b) values(4,2);
update t1 set a = a + 5 where b = 2;
update t3 set a = a + 5 where b = 2;
delete from t1 where b = 2;
delete from t3 where b = 2;
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; insert into t4(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(1,1),(2,1)
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(1,1),(2,1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 1
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 1
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Update_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t1 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; delete from t3 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INVOKES A TRIGGER with before update action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t4, slave:t4]
include/diff_tables.inc [master:t6, slave:t6]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
DROP TABLE t6;
DROP FUNCTION f1_insert_triggered;
include/sync_slave_sql_with_master.inc
# Test case5: INVOKES A TRIGGER with after delete action
create table t1(a int, b int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr1 after delete on t1 for each row insert into t2(a) values(6);
create table t3(a int, b int) engine=innodb;
create table t4(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t5(a int) engine=innodb;
create trigger tr2 after delete on t3 for each row begin
insert into t4(a) values(f1_insert_triggered());
insert into t4(a) values(f1_insert_triggered());
insert into t5(a) values(8);
end |
create table t6(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_insert_triggered() RETURNS INTEGER
BEGIN
INSERT INTO t6(a) values(2),(3);
RETURN 1;
END//
begin;
insert into t1(a,b) values(1,1),(2,1);
insert into t3(a,b) values(1,1),(2,1);
update t1 set a = a + 5 where b = 1;
update t3 set a = a + 5 where b = 1;
delete from t1 where b = 1;
delete from t3 where b = 1;
insert into t2(a) values(3);
insert into t4(a) values(3);
commit;
insert into t1(a,b) values(4,2);
insert into t3(a,b) values(4,2);
update t1 set a = a + 5 where b = 2;
update t3 set a = a + 5 where b = 2;
delete from t1 where b = 2;
delete from t3 where b = 2;
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; insert into t4(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(1,1),(2,1)
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(1,1),(2,1)
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 1
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 1
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INVOKES A TRIGGER with after delete action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t4, slave:t4]
include/diff_tables.inc [master:t6, slave:t6]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
DROP TABLE t6;
DROP FUNCTION f1_insert_triggered;
include/sync_slave_sql_with_master.inc
# Test case6: INVOKES A TRIGGER with before delete action
create table t1(a int, b int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr1 before delete on t1 for each row insert into t2(a) values(6);
create table t3(a int, b int) engine=innodb;
create table t4(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t5(a int) engine=innodb;
create trigger tr2 before delete on t3 for each row begin
insert into t4(a) values(f1_insert_triggered());
insert into t4(a) values(f1_insert_triggered());
insert into t5(a) values(8);
end |
create table t6(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_insert_triggered() RETURNS INTEGER
BEGIN
INSERT INTO t6(a) values(2),(3);
RETURN 1;
END//
begin;
insert into t1(a,b) values(1,1),(2,1);
insert into t3(a,b) values(1,1),(2,1);
update t1 set a = a + 5 where b = 1;
update t3 set a = a + 5 where b = 1;
delete from t1 where b = 1;
delete from t3 where b = 1;
insert into t2(a) values(3);
insert into t4(a) values(3);
commit;
insert into t1(a,b) values(4,2);
insert into t3(a,b) values(4,2);
update t1 set a = a + 5 where b = 2;
update t3 set a = a + 5 where b = 2;
delete from t1 where b = 2;
delete from t3 where b = 2;
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; insert into t4(a) values(3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(1,1),(2,1)
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(1,1),(2,1)
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 1
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 1
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into t3(a,b) values(4,2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t1 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; update t3 set a = a + 5 where b = 2
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t4)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t5)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t6)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Delete_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INVOKES A TRIGGER with before delete action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t4, slave:t4]
include/diff_tables.inc [master:t6, slave:t6]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
DROP TABLE t6;
DROP FUNCTION f1_insert_triggered;
include/sync_slave_sql_with_master.inc
# Test case7: CALLS A FUNCTION which INVOKES A TRIGGER with after insert action
create table t1(a int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t3(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_two_inserts_trigger() RETURNS INTEGER
BEGIN
INSERT INTO t2(a) values(2),(3);
INSERT INTO t2(a) values(2),(3);
RETURN 1;
END |
create trigger tr11 after insert on t2 for each row begin
insert into t3(a) values(new.a);
insert into t3(a) values(new.a);
end |
begin;
insert into t1(a) values(f1_two_inserts_trigger());
insert into t2(a) values(4),(5);
commit;
insert into t1(a) values(f1_two_inserts_trigger());
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'CALLS A FUNCTION which INVOKES A TRIGGER with after insert action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t3, slave:t3]
drop table t1;
drop table t2;
drop table t3;
drop function f1_two_inserts_trigger;
include/sync_slave_sql_with_master.inc
# Test case8: CALLS A FUNCTION which INVOKES A TRIGGER with before insert action
create table t1(a int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create table t3(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_two_inserts_trigger() RETURNS INTEGER
BEGIN
INSERT INTO t2(a) values(2),(3);
INSERT INTO t2(a) values(2),(3);
RETURN 1;
END |
create trigger tr11 before insert on t2 for each row begin
insert into t3(a) values(new.a);
insert into t3(a) values(new.a);
end |
begin;
insert into t1(a) values(f1_two_inserts_trigger());
insert into t2(a) values(4),(5);
commit;
insert into t1(a) values(f1_two_inserts_trigger());
# To verify if insert/update in an autoinc column causes statement to be logged in row format
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t2)
master-bin.000001	#	Table_map	#	#	table_id: # (test.t3)
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: #
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'CALLS A FUNCTION which INVOKES A TRIGGER with before insert action'
include/diff_tables.inc [master:t2, slave:t2]
include/diff_tables.inc [master:t3, slave:t3]
drop table t1;
drop table t2;
drop table t3;
drop function f1_two_inserts_trigger;
include/sync_slave_sql_with_master.inc
# Test case9: INSERT DATA INTO VIEW WHICH INVOKES TRIGGERS with after insert action
CREATE TABLE t1(i1 int not null auto_increment, c1 INT, primary key(i1)) engine=innodb;
CREATE TABLE t2(i1 int not null auto_increment, c2 INT, primary key(i1)) engine=innodb;
CREATE TABLE t3(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr16 after insert on t1 for each row insert into t3(a) values(new.c1);
create trigger tr17 after insert on t2 for each row insert into t3(a) values(new.c2);
begin;
INSERT INTO t1(c1) VALUES (11), (12);
INSERT INTO t2(c2) VALUES (13), (14);
CREATE VIEW v16 AS SELECT c1, c2 FROM t1, t2;
INSERT INTO v16(c1) VALUES (15),(16);
INSERT INTO v16(c2) VALUES (17),(18);
INSERT INTO v16(c1) VALUES (19),(20);
INSERT INTO v16(c2) VALUES (21),(22);
INSERT INTO v16(c1) VALUES (23), (24);
INSERT INTO v16(c1) VALUES (25), (26);
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INSERT DATA INTO VIEW WHICH INVOKES TRIGGERS'
include/diff_tables.inc [master:t3, slave:t3]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP VIEW  v16;
include/sync_slave_sql_with_master.inc
# Test case10: INSERT DATA INTO VIEW WHICH INVOKES TRIGGERS with before insert action
CREATE TABLE t1(i1 int not null auto_increment, c1 INT, primary key(i1)) engine=innodb;
CREATE TABLE t2(i1 int not null auto_increment, c2 INT, primary key(i1)) engine=innodb;
CREATE TABLE t3(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
create trigger tr16 before insert on t1 for each row insert into t3(a) values(new.c1);
create trigger tr17 before insert on t2 for each row insert into t3(a) values(new.c2);
begin;
INSERT INTO t1(c1) VALUES (11), (12);
INSERT INTO t2(c2) VALUES (13), (14);
CREATE VIEW v16 AS SELECT c1, c2 FROM t1, t2;
INSERT INTO v16(c1) VALUES (15),(16);
INSERT INTO v16(c2) VALUES (17),(18);
INSERT INTO v16(c1) VALUES (19),(20);
INSERT INTO v16(c2) VALUES (21),(22);
INSERT INTO v16(c1) VALUES (23), (24);
INSERT INTO v16(c1) VALUES (25), (26);
commit;
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INSERT DATA INTO VIEW WHICH INVOKES TRIGGERS'
include/diff_tables.inc [master:t3, slave:t3]
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP VIEW  v16;
include/sync_slave_sql_with_master.inc
# Test case11: INVOKES A FUNCTION TO INSERT TWO OR MORE VALUES INTO A TABLE WITH AUTOINC COLUMN
create table t1(a int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_two_inserts() RETURNS INTEGER
BEGIN
INSERT INTO t2(a) values(2);
INSERT INTO t2(a) values(2);
RETURN 1;
END//
begin;
insert into t1(a) values(f1_two_inserts());
insert into t2(a) values(4),(5);
commit;
insert into t1(a) values(f1_two_inserts());
commit;
#Test result for INVOKES A FUNCTION TO INSERT TWO OR MORE VALUES on master
select * from t2 ORDER BY i1;
i1	a
1	2
2	2
3	4
4	5
5	2
6	2
include/sync_slave_sql_with_master.inc
#Test result for INVOKES A FUNCTION TO INSERT TWO OR MORE VALUES on slave
select * from t2 ORDER BY i1;
i1	a
1	2
2	2
3	4
4	5
5	2
6	2
drop table t1;
drop table t2;
drop function f1_two_inserts;
include/sync_slave_sql_with_master.inc
# Test case12: INVOKES A FUNCTION TO UPDATE TWO OR MORE VALUES OF A TABLE WITH AUTOINC COLUMN
create table t1(a int) engine=innodb;
create table t2(i1 int not null auto_increment, a int, b int, primary key(i1)) engine=innodb;
CREATE FUNCTION f1_two_updates() RETURNS INTEGER
BEGIN
update t2 set a = a + 5 where b = 1;
update t2 set a = a + 5 where b = 2;
update t2 set a = a + 5 where b = 3;
update t2 set a = a + 5 where b = 4;
RETURN 1;
END//
insert into t2(a,b) values(1,1);
insert into t2(a,b) values(2,2);
insert into t2(a,b) values(3,3);
insert into t2(a,b) values(4,4);
insert into t1(a) values(f1_two_updates());
begin;
insert into t1(a) values(f1_two_updates());
commit;
#Test result for INVOKES A FUNCTION TO UPDATE TWO OR MORE VALUES on master
select * from t2 ORDER BY i1;
i1	a	b
1	11	1
2	12	2
3	13	3
4	14	4
include/sync_slave_sql_with_master.inc
#Test result for INVOKES A FUNCTION TO UPDATE TWO OR MORE VALUES on slave
select * from t2 ORDER BY i1;
i1	a	b
1	11	1
2	12	2
3	13	3
4	14	4
drop table t1;
drop table t2;
drop function f1_two_updates;
include/sync_slave_sql_with_master.inc
# Test case13: UPDATE MORE THAN ONE TABLES ON TOP-STATEMENT
create table t1(i1 int not null auto_increment, a int, b int, primary key(i1)) engine=innodb;
create table t2(i1 int not null auto_increment, a int, b int, primary key(i1)) engine=innodb;
begin;
insert into t1(a,b) values(1,1),(2,2);
insert into t2(a,b) values(1,1),(2,2);
update t1,t2 set t1.a=t1.a+5, t2.a=t2.a+5 where t1.b=t2.b;
insert into t1(a,b) values(3,3);
insert into t2(a,b) values(3,3);
commit;
# To verify if it works fine when these statements are not be marked as unsafe
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=1
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(1,1),(2,2)
master-bin.000001	#	Intvar	#	#	INSERT_ID=1
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a,b) values(1,1),(2,2)
master-bin.000001	#	Query	#	#	use `test`; update t1,t2 set t1.a=t1.a+5, t2.a=t2.a+5 where t1.b=t2.b
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t1(a,b) values(3,3)
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; insert into t2(a,b) values(3,3)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'UPDATE MORE THAN ONE TABLES ON TOP-STATEMENT'
include/diff_tables.inc [master:t1, slave:t1]
include/diff_tables.inc [master:t2, slave:t2]
drop table t1;
drop table t2;
include/sync_slave_sql_with_master.inc
# Test case14: INSERT DATA INTO VIEW WHICH INVOLVED MORE THAN ONE TABLES
CREATE TABLE t1(i1 int not null auto_increment, c1 INT, primary key(i1)) engine=innodb;
CREATE TABLE t2(i1 int not null auto_increment, c2 INT, primary key(i1)) engine=innodb;
begin;
INSERT INTO t1(c1) VALUES (11), (12);
INSERT INTO t2(c2) VALUES (13), (14);
CREATE VIEW v15 AS SELECT c1, c2 FROM t1, t2;
INSERT INTO v15(c1) VALUES (15),(16);
INSERT INTO v15(c2) VALUES (17),(18);
INSERT INTO v15(c1) VALUES (19),(20);
INSERT INTO v15(c2) VALUES (21),(22);
INSERT INTO v15(c1) VALUES (23), (24);
INSERT INTO v15(c2) VALUES (25), (26);
commit;
# To verify if it works fine when these statements are not be marked as unsafe
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=1
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1(c1) VALUES (11), (12)
master-bin.000001	#	Intvar	#	#	INSERT_ID=1
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t2(c2) VALUES (13), (14)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v15` AS SELECT c1, c2 FROM t1, t2
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO v15(c1) VALUES (15),(16)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=3
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO v15(c2) VALUES (17),(18)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO v15(c1) VALUES (19),(20)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=5
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO v15(c2) VALUES (21),(22)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=7
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO v15(c1) VALUES (23), (24)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Intvar	#	#	INSERT_ID=7
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO v15(c2) VALUES (25), (26)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
include/sync_slave_sql_with_master.inc
#Test if the results are consistent on master and slave
#for 'INSERT DATA INTO VIEW WHICH INVOLVED MORE THAN ONE TABLES'
include/diff_tables.inc [master:t1, slave:t1]
include/diff_tables.inc [master:t2, slave:t2]
drop table t1;
drop table t2;
drop view  v15;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
