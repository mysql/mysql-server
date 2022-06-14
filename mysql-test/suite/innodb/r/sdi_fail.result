#
# Bug #27493634 FAILURE DURING SDI DELETION CAUSES ASSERT
#
create table t1(c1 int primary key, c2 char(1), c3 int unsigned);
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  `c2` char(1) DEFAULT NULL,
  `c3` int unsigned DEFAULT NULL,
  PRIMARY KEY (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET DEBUG='+d,ib_create_table_fail_too_many_trx';
ALTER TABLE t1 ADD INDEX (c2), ADD INDEX (c3);
ERROR HY000: Too many active concurrent transactions
SET DEBUG='-d,ib_create_table_fail_too_many_trx';
drop table t1;
