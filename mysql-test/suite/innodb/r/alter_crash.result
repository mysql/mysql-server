#
# Bug#20015132 ALTER TABLE FAILS TO CHECK IF TABLE IS CORRUPTED
#
CREATE TABLE t1(c1 INT PRIMARY KEY, c2 CHAR(1), c3 INT UNSIGNED) ENGINE=InnoDB;
ALTER TABLE t1 ADD INDEX (c2), ADD INDEX (c3);
SET DEBUG='+d,dict_set_index_corrupted';
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	Warning	InnoDB: The B-tree of index c2 is corrupted.
test.t1	check	Warning	InnoDB: The B-tree of index c3 is corrupted.
test.t1	check	error	Corrupt
# restart
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	Warning	InnoDB: Index c2 is marked as corrupted
test.t1	check	Warning	InnoDB: Index c3 is marked as corrupted
test.t1	check	error	Corrupt
ALTER TABLE t1 DROP INDEX c2;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	Warning	InnoDB: Index c3 is marked as corrupted
test.t1	check	error	Corrupt
ALTER TABLE t1 ADD INDEX (c2,c3);
ERROR 42000: Can't open table
ALTER TABLE t1 CHANGE c3 c3 INT NOT NULL;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
ALTER TABLE t1 ADD INDEX (c2,c3);
DROP TABLE t1;
#
# Bug #14669848 CRASH DURING ALTER MAKES ORIGINAL TABLE INACCESSIBLE
#
# -- Scenario 1:
# Crash the server in ha_innobase::commit_inplace_alter_table()
# just after committing the dictionary changes.
CREATE TABLE t1 (f1 INT NOT NULL, f2 INT NOT NULL) ENGINE=innodb;
INSERT INTO t1 VALUES (1,2),(3,4);
SET DEBUG='d,innodb_alter_commit_crash_after_commit';
ALTER TABLE t1 ADD PRIMARY KEY (f2, f1);
ERROR HY000: Lost connection to MySQL server during query
# Restart mysqld after the crash and reconnect.
# restart
# Files in datadir after manual recovery.
t1.ibd
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1 VALUES (5,6),(7,8);
SELECT * FROM t1;
f1	f2
1	2
3	4
5	6
7	8
DROP TABLE t1;
CREATE TABLE t1 (f1 INT NOT NULL, f2 INT NOT NULL) ENGINE=InnoDB;
ALTER TABLE t1 ADD PRIMARY KEY (f2, f1);
DROP TABLE t1;
# -- Scenario 2:
# Crash the server in ha_innobase::commit_inplace_alter_table()
# just before committing the dictionary changes, but after
# writing the MLOG_FILE_RENAME records. As the mini-transaction
# is not committed, the renames will not be replayed.
CREATE TABLE t2 (f1 int not null, f2 int not null) ENGINE=InnoDB;
INSERT INTO t2 VALUES (1,2),(3,4);
SET DEBUG='d,innodb_alter_commit_crash_before_commit';
ALTER TABLE t2 ADD PRIMARY KEY (f2, f1);
ERROR HY000: Lost connection to MySQL server during query
# Startup the server after the crash
# restart
SHOW TABLES;
Tables_in_test
t2
INSERT INTO t2 VALUES (5,6),(7,8);
SELECT * from t2;
f1	f2
1	2
3	4
5	6
7	8
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int NOT NULL,
  `f2` int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t2;
CREATE TABLE t2 (f1 INT NOT NULL, f2 INT NOT NULL) ENGINE=InnoDB;
ALTER TABLE t2 ADD PRIMARY KEY (f2, f1);
DROP TABLE t2;
# -------------------------
# End of Testing Scenario 2
# -------------------------
#
# Bug#19330255 CRASH DURING ALTER TABLE LEADS TO
# DATA DICTIONARY INCONSISTENCY
#
CREATE TABLE t1(a int PRIMARY KEY, b varchar(150), c int NOT NULL);
INSERT INTO t1 SET a=1,c=2;
SET DEBUG='d,innodb_alter_commit_crash_after_commit';
ALTER TABLE t1 ADD INDEX (b), CHANGE c d int;
ERROR HY000: Lost connection to MySQL server during query
# Restart mysqld after the crash and reconnect.
# restart
# Files in datadir after manual recovery.
t1.ibd
SHOW TABLES;
Tables_in_test
t1
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` varchar(150) DEFAULT NULL,
  `c` int NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
a	b	c
1	NULL	2
DROP TABLE t1;
