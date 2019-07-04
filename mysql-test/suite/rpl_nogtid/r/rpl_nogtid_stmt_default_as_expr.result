include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
# Create tables on master
CREATE TABLE v1 (i VARCHAR(36) DEFAULT (RAND()), j VARCHAR(36) DEFAULT (PI()));
# No warning expected as both columns are defined
INSERT INTO v1(i, j) VALUES ("a","b");
# No warning expected as both columns are defined
INSERT INTO v1 VALUES ("a","b");
# No warning expected as j is deterministic
INSERT INTO v1(i) VALUES ("a");
# No warning expected as j is deterministic
INSERT INTO v1(i,j) VALUES ("a",DEFAULT);
# Warning expected as i is non-deterministic
INSERT INTO v1(j) VALUES ("b");
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
# Warning expected as i is non-deterministic
INSERT INTO v1(i,j) VALUES (DEFAULT,"b");
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
# Warning expected as i is non-deterministic
INSERT INTO v1() VALUES();
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
DROP TABLE v1;
CREATE TABLE t1(
id INTEGER NOT NULL AUTO_INCREMENT,
token VARCHAR(255) DEFAULT (UUID()),
PRIMARY KEY (id),
KEY token (token)
);
# Warning expected as token is non-deterministic
INSERT INTO t1 VALUES (3, "abc"), (DEFAULT, DEFAULT);
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
# No warning expected as token is defined
INSERT INTO t1 VALUES (1, "abc"), (2, "def");
DROP TABLE t1;
CREATE TABLE t2 (i VARCHAR(36) DEFAULT (UUID()));
# Regular insert, no warnings
INSERT INTO t2(i) VALUES('MySQL');
# Making sure that warnings are shown
INSERT INTO t2(i) VALUES(DEFAULT);
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
INSERT INTO t2() VALUES();
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
SELECT COUNT(*) from t2;
COUNT(*)
3
create table t1 ( i double DEFAULT ( RAND() ) ) ;
insert into t1 values (1),(2) ;
update t1 set i = DEFAULT where i = 2 ;
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
drop table t1;
SET @start_enforce_gtid_consistency = @@GLOBAL.ENFORCE_GTID_CONSISTENCY;
SET GLOBAL ENFORCE_GTID_CONSISTENCY = WARN;
ALTER TABLE t2 ADD COLUMN c char(36) DEFAULT (PI());
SET GLOBAL ENFORCE_GTID_CONSISTENCY = @start_enforce_gtid_consistency;
include/sync_slave_sql_with_master.inc
[ slave connection ]
SHOW TABLES;
Tables_in_test
t2
SHOW CREATE TABLE t2 ;
Table	Create Table
t2	CREATE TABLE `t2` (
  `i` varchar(36) DEFAULT (uuid()),
  `c` char(36) DEFAULT (pi())
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT COUNT(*) from t2;
COUNT(*)
3
# clean up
[connection master]
DROP TABLE t2;
include/rpl_end.inc
