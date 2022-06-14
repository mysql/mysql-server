#
# Bug #27624990 ASSERTION `FALSE' FAILED AT BOOL WRAPPER_TO_STRING() IN SQL/JSON_DOM.CC
#
#
# Scenario 1: The row format is DYNAMIC (no blob prefix).  The READ
# UNCOMMITTED transaction will see the new value of column f1 but
# old value of column f2.
#
create table t1 (f1 int, f2 longblob) row_format=dynamic;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` longblob
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC
insert into t1 values (1, repeat('x', 40000));
SET DEBUG_SYNC = 'blob_write_middle SIGNAL do_select WAIT_FOR continue_insert';
update t1 set f1 = 2, f2 = repeat('y', 100000) where f1 = 1;;
SET DEBUG_SYNC = 'now WAIT_FOR do_select';
SET SESSION transaction_isolation = 'READ-UNCOMMITTED';
begin;
select f1, f2 from t1;
f1	f2
2	
commit;
SET DEBUG_SYNC = 'now SIGNAL continue_insert';
drop table t1;
#
# Scenario 2: The row format is REDUNDANT (no blob prefix).  The READ
# UNCOMMITTED transaction will see only the local prefix of f2.
#
create table t1 (f1 int primary key, f2 longblob) row_format=redundant;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` longblob,
  PRIMARY KEY (`f1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=REDUNDANT
insert into t1 values (1, repeat('x', 40000));
SET DEBUG_SYNC = 'blob_write_middle SIGNAL do_select WAIT_FOR continue_insert EXECUTE 2';
update t1 set f2 = repeat('y', 100000) where f1 = 1;;
SET DEBUG_SYNC = 'now WAIT_FOR do_select';
SET DEBUG_SYNC = 'now SIGNAL continue_insert';
SET DEBUG_SYNC = 'now WAIT_FOR do_select';
SET SESSION transaction_isolation = 'READ-UNCOMMITTED';
begin;
select length(f2) from t1 where f1 = 1;
length(f2)
0
commit;
SET DEBUG_SYNC = 'now SIGNAL continue_insert';
drop table t1;
#
# Scenario 3: The row format is REDUNDANT (no blob prefix).  The READ
# UNCOMMITTED transaction will see the local prefix of new blob and
# the external part of the old blob.
#
create table t1 (f1 int primary key, f2 longblob) row_format=redundant;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` longblob,
  PRIMARY KEY (`f1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=REDUNDANT
insert into t1 values (1, repeat('x', 40000));
SET DEBUG_SYNC = 'blob_write_middle SIGNAL do_select WAIT_FOR continue_insert';
update t1 set f2 = repeat('y', 100000) where f1 = 1;;
SET DEBUG_SYNC = 'now WAIT_FOR do_select';
SET SESSION transaction_isolation = 'READ-UNCOMMITTED';
begin;
select f2 from t1 where f1 = 1;
f2

commit;
SET DEBUG_SYNC = 'now SIGNAL continue_insert';
drop table t1;
#
# Scenario 4: The row format is REDUNDANT (no blob prefix).
# The READ UNCOMMITTED transaction will see the local prefix
# of new blob and external part of old blob.  It is different
# from Scenario 3 because this involves blob operation code
# OPCODE_INSERT_UPDATE.
#
create table t1 (f1 int primary key, f2 longblob) row_format=redundant;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` longblob,
  PRIMARY KEY (`f1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=REDUNDANT
start transaction;
insert into t1 values (1, repeat('x', 40000));
delete from t1 where f1 = 1;
SET DEBUG_SYNC = 'blob_write_middle SIGNAL do_select WAIT_FOR continue_insert';
insert into t1 values (1, repeat('y', 40000));;
SET DEBUG_SYNC = 'now WAIT_FOR do_select';
SET SESSION transaction_isolation = 'READ-UNCOMMITTED';
begin;
select f2 from t1 where f1 = 1;
f2

commit;
SET DEBUG_SYNC = 'now SIGNAL continue_insert';
commit;
drop table t1;
