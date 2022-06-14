drop table if exists t1;
create table t1 (a int check (a>0));
insert into t1 values (1);
insert into t1 values (0);
ERROR HY000: Check constraint 't1_chk_1' is violated.
drop table t1;
create table t1 (a int, b int, check (a>b));
insert into t1 values (1,0);
insert into t1 values (0,1);
ERROR HY000: Check constraint 't1_chk_1' is violated.
drop table t1;
create table t1 (a int ,b int, constraint abc check (a>b));
insert into t1 values (1,0);
insert into t1 values (0,1);
ERROR HY000: Check constraint 'abc' is violated.
drop table t1;
create table t1 (a int null);
insert into t1 values (1),(NULL);
drop table t1;
create table t1 (a int null);
alter table t1 add constraint constraint_1 unique (a);
alter table t1 add constraint unique key_1(a);
Warnings:
Warning	1831	Duplicate index 'key_1' defined on the table 'test.t1'. This is deprecated and will be disallowed in a future release.
alter table t1 add constraint constraint_2 unique key_2(a);
Warnings:
Warning	1831	Duplicate index 'key_2' defined on the table 'test.t1'. This is deprecated and will be disallowed in a future release.
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  UNIQUE KEY `constraint_1` (`a`),
  UNIQUE KEY `key_1` (`a`),
  UNIQUE KEY `key_2` (`a`)
) ENGINE=ENGINE DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
drop table t1;
drop table if exists t_illegal;
create table t_illegal (a int, b int, check a>b);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'a>b)' at line 1
create table t_illegal (a int, b int, constraint abc check a>b);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'a>b)' at line 1
create table t_illegal (a int, b int, constraint abc);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1
drop table if exists t_11714;
create table t_11714(a int, b int);
alter table t_11714 add constraint cons1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
drop table t_11714;
CREATE TABLE t_illegal (col_1 INT CHECK something (whatever));
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'something (whatever))' at line 1
CREATE TABLE t_illegal (col_1 INT CHECK something);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'something)' at line 1
###############################################################################
# WL#12798 - Implement ALTER TABLE ... DROP/ALTER CONSTRAINT syntax.          #
#            Test cases to verify ALTER TABLE ... DROP CONSTRAINT clause.     #
###############################################################################
CREATE TABLE t1 (f1 INT, f2 INT);
CREATE TABLE t2 (f1 INT PRIMARY KEY);
#-----------------------------------------------------------------------
# Testing scenario where DROP CONSTRAINT fail.
#-----------------------------------------------------------------------
ALTER TABLE t1 DROP CONSTRAINT;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
#-----------------------------------------------------------------------
# Test case to verify drop constraint with unknown (or non-existing)
# constraint.
#-----------------------------------------------------------------------
ALTER TABLE t1 DROP CONSTRAINT unknown;
ERROR HY000: Constraint 'unknown' does not exist.
ALTER TABLE t1 ADD CONSTRAINT f2_check CHECK (f2 > 0);
ALTER TABLE t1 DROP CONSTRAINT unknown;
ERROR HY000: Constraint 'unknown' does not exist.
ALTER TABLE t1 DROP CONSTRAINT f2_check;
#-----------------------------------------------------------------------
# Test case to verify drop constraint with all types of table
# constraints.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE (f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 DROP CONSTRAINT fk;
ALTER TABLE t1 DROP CONSTRAINT `primary`;
ALTER TABLE t1 DROP CONSTRAINT f2_unique;
ALTER TABLE t1 DROP CONSTRAINT f2_check;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE (f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 DROP CONSTRAINT `primary`,
DROP CONSTRAINT f2_unique,
DROP CONSTRAINT fk,
DROP CONSTRAINT f2_check;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#-----------------------------------------------------------------------
# Test case to verify drop multiple type constraints with the same name.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT name2 UNIQUE (f1),
ADD CONSTRAINT `primary` FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT name2 CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `name2` (`f1`),
  CONSTRAINT `primary` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `name2` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 DROP CONSTRAINT `primary`;
ERROR HY000: Table has multiple constraints with the name 'primary'. Please use constraint specific 'DROP' clause.
ALTER TABLE t1 DROP CONSTRAINT name2;
ERROR HY000: Table has multiple constraints with the name 'name2'. Please use constraint specific 'DROP' clause.
ALTER TABLE t1 DROP PRIMARY KEY,
DROP INDEX name2,
DROP FOREIGN KEY `primary`,
DROP CHECK name2;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#-----------------------------------------------------------------------
# Test case to verify drop constraint with constraint specific drop
# clause and DROP CONSTRAINT clause.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE (f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 DROP CONSTRAINT `primary`,
DROP FOREIGN KEY fk,
DROP CONSTRAINT f2_unique,
DROP CHECK f2_check;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#-----------------------------------------------------------------------
# Test case to verify drop constraint and add constraint with same name
# in the ALTER statement.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE (f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0),
ADD COLUMN f3 INT;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  `f3` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 DROP CONSTRAINT `primary`, ADD CONSTRAINT PRIMARY KEY (f1),
DROP CONSTRAINT f2_unique, ADD CONSTRAINT f2_unique UNIQUE (f3),
DROP CONSTRAINT f2_check,
ADD CONSTRAINT f2_check CHECK ((f3 + f2 + f1) < 999);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  `f3` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f3`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((((`f3` + `f2`) + `f1`) < 999))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 DROP CONSTRAINT `primary`,
DROP CONSTRAINT fk,
DROP CONSTRAINT f2_unique,
DROP CONSTRAINT f2_check,
DROP COLUMN f3;
#-----------------------------------------------------------------------
# Test case to verify drop primary constraint operation when
# sql_require_primary_key = true.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1);
SET SESSION sql_require_primary_key = true;
# Drop primary key operation fails when sql_require_primary_key = true.
ALTER TABLE t1 DROP PRIMARY KEY;
ERROR HY000: Unable to create or change a table without a primary key, when the system variable 'sql_require_primary_key' is set. Add a primary key to the table or unset this variable to avoid this message. Note that tables without a primary key can cause performance problems in row-based replication, so please consult your DBA before changing this setting.
# Drop primary key operation fails when sql_require_primary_key = true.
ALTER TABLE t1 DROP CONSTRAINT `primary`;
ERROR HY000: Unable to create or change a table without a primary key, when the system variable 'sql_require_primary_key' is set. Add a primary key to the table or unset this variable to avoid this message. Note that tables without a primary key can cause performance problems in row-based replication, so please consult your DBA before changing this setting.
SET SESSION sql_require_primary_key = default;
ALTER TABLE t1 DROP CONSTRAINT `primary`;
#-----------------------------------------------------------------------
# Test case to check if generated hidden columns are dropped on
# functional index drop operation.
#-----------------------------------------------------------------------
CREATE TABLE t3 (col1 INT, col2 INT GENERATED ALWAYS AS (col1) STORED);
ALTER TABLE t3 ADD UNIQUE INDEX idx (((COS( col2 ))) DESC);
ALTER TABLE t3 DROP CONSTRAINT idx;
# Assert condition is hit in debug build if generated hidden column is
# not dropped in index idx drop operation.
ALTER TABLE t3 ADD UNIQUE INDEX idx (((COS( col1 ))) DESC);
DROP TABLE t3;
#-----------------------------------------------------------------------
# Test case to verify DROP CONSTRAINT operations on temporary table.
#-----------------------------------------------------------------------
CREATE TEMPORARY TABLE tmp (f1 INT, f2 INT,
CONSTRAINT PRIMARY KEY (f1),
CONSTRAINT f2_unique UNIQUE(f2),
CONSTRAINT f2_check CHECK (f2 > 0));
SHOW CREATE TABLE tmp;
Table	Create Table
tmp	CREATE TEMPORARY TABLE `tmp` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE tmp DROP CONSTRAINT `primary`,
DROP CONSTRAINT f2_check,
DROP CONSTRAINT f2_unique;
SHOW CREATE TABLE tmp;
Table	Create Table
tmp	CREATE TEMPORARY TABLE `tmp` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE tmp;
#-----------------------------------------------------------------------
# Test case to verify drop constraint in stored procedures.
#-----------------------------------------------------------------------
CREATE PROCEDURE drop_constraint_proc()
ALTER TABLE t1 DROP CONSTRAINT `primary`,
DROP CONSTRAINT f2_unique,
DROP CONSTRAINT fk,
DROP CONSTRAINT f2_check;
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
CALL drop_constraint_proc();
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_check UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_check` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_unique` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Constraint names are changed in ALTER TABLE operations. Re-execution of
# a stored procedure should set correct constraint type.
CALL drop_constraint_proc();
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP PROCEDURE drop_constraint_proc;
#-----------------------------------------------------------------------
# Test case to verify drop constraint in nested stored procedure call.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN f3 INT GENERATED ALWAYS AS (f1) STORED;
CREATE PROCEDURE drop_constraint_proc()
ALTER TABLE t1 DROP CONSTRAINT constraint_name;
CREATE PROCEDURE test_drop_constraint()
BEGIN
ALTER TABLE t1 ADD CONSTRAINT constraint_name UNIQUE(f2);
call drop_constraint_proc();
ALTER TABLE t1 ADD UNIQUE INDEX constraint_name (((COS(f3))) DESC);
call drop_constraint_proc();
ALTER TABLE t1 ADD CONSTRAINT constraint_name CHECK (f2 > 0);
call drop_constraint_proc();
ALTER TABLE t1 ADD CONSTRAINT constraint_name FOREIGN KEY (f1) REFERENCES t2(f1);
call drop_constraint_proc();
END$
# Re-execution of drop_constraint_proc() in test_drop_constraint() should
# set correct constraint type.
CALL test_drop_constraint();
DROP PROCEDURE drop_constraint_proc;
DROP PROCEDURE test_drop_constraint;
ALTER TABLE t1 DROP COLUMN f3;
#-----------------------------------------------------------------------
# Test case to verify drop constraint in prepared statements.
#-----------------------------------------------------------------------
PREPARE drop_constraint_stmt FROM
'ALTER TABLE t1 DROP CONSTRAINT `primary`,
                  DROP CONSTRAINT f2_unique,
                  DROP CONSTRAINT fk,
                  DROP CONSTRAINT f2_check';
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXECUTE drop_constraint_stmt;
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_check UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_check` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_unique` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Constraint names are changed in ALTER TABLE operations. Re-execution of
# prepared statement should set correct constraint type.
EXECUTE drop_constraint_stmt;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP PREPARE drop_constraint_stmt;
###############################################################################
# WL#12798 - Implement ALTER TABLE ... DROP/ALTER CONSTRAINT syntax.          #
#            Test cases to verify ALTER TABLE ... ALTER CONSTRAINT clause.    #
###############################################################################
#-----------------------------------------------------------------------
# Testing scenario where ALTER CONSTRAINT fail.
#-----------------------------------------------------------------------
ALTER TABLE t1 ALTER CONSTRAINT;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
ALTER TABLE t1 ALTER CONSTRAINT unknown NOT;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
#-----------------------------------------------------------------------
# Test case to verify alter constraint with unknown(or non-existing)
# constraint.
#-----------------------------------------------------------------------
ALTER TABLE t1 ALTER CONSTRAINT unknown ENFORCED;
ERROR HY000: Constraint 'unknown' does not exist.
ALTER TABLE t1 ADD CONSTRAINT f2_check CHECK (f2 > 0);
ALTER TABLE t1 ALTER CONSTRAINT unknown ENFORCED;
ERROR HY000: Constraint 'unknown' does not exist.
ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED;
ALTER TABLE t1 DROP CONSTRAINT f2_check;
#-----------------------------------------------------------------------
# Test case to verify alter multiple type constraints with the same name.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT name2 UNIQUE (f1),
ADD CONSTRAINT `primary` FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT name2 CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `name2` (`f1`),
  CONSTRAINT `primary` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `name2` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ALTER CONSTRAINT name2 NOT ENFORCED;
ERROR HY000: Table has multiple constraints with the name 'name2'. Please use constraint specific 'ALTER' clause.
ALTER TABLE t1 DROP PRIMARY KEY,
DROP INDEX name2,
DROP FOREIGN KEY `primary`,
DROP CHECK name2;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#-----------------------------------------------------------------------
# Test case to verify alter unsupported constraint.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE (f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ALTER CONSTRAINT `primary` NOT ENFORCED;
ERROR HY000: Altering constraint enforcement is not supported for the constraint 'primary'. Enforcement state alter is not supported for the PRIMARY, UNIQUE and FOREIGN KEY type constraints.
ALTER TABLE t1 ALTER CONSTRAINT f2_unique NOT ENFORCED;
ERROR HY000: Altering constraint enforcement is not supported for the constraint 'f2_unique'. Enforcement state alter is not supported for the PRIMARY, UNIQUE and FOREIGN KEY type constraints.
ALTER TABLE t1 ALTER CONSTRAINT fk NOT ENFORCED;
ERROR HY000: Altering constraint enforcement is not supported for the constraint 'fk'. Enforcement state alter is not supported for the PRIMARY, UNIQUE and FOREIGN KEY type constraints.
#-----------------------------------------------------------------------
# Test case to verify alter supported constraint.
#-----------------------------------------------------------------------
ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 DROP CONSTRAINT `primary`,
DROP CONSTRAINT f2_unique,
DROP CONSTRAINT fk,
DROP CONSTRAINT f2_check;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#-----------------------------------------------------------------------
# Test case to verify alter constraint with constraint specific alter
# clause and ALTER CONSTRAINT clause.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ALTER CHECK f2_check NOT ENFORCED, ALTER CHECK f2_check ENFORCED;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED, ALTER CONSTRAINT f2_check ENFORCED;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 ALTER CHECK f2_check NOT ENFORCED, ALTER CONSTRAINT f2_check ENFORCED;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#-----------------------------------------------------------------------
# Test case to verify order of execution when ALTER statement contains
# both DROP CONSTRAINT and ALTER CONSTRAINT operation on the same
# constraint.
#-----------------------------------------------------------------------
# First drop operations are executed and the alter operations. So alter
# operation reports an error here.
ALTER TABLE t1 DROP CONSTRAINT f2_check, ALTER CONSTRAINT f2_check NOT ENFORCED;
ERROR HY000: Check constraint 'f2_check' is not found in the table.
ALTER TABLE t1 DROP CONSTRAINT f2_check;
#-----------------------------------------------------------------------
# Test case to verify alter constraint and drop constraint in stored
# procedures.
#-----------------------------------------------------------------------
CREATE PROCEDURE drop_constraint_proc()
ALTER TABLE t1 DROP CONSTRAINT `primary`,
DROP CONSTRAINT f2_unique,
DROP CONSTRAINT fk,
DROP CONSTRAINT f2_check;
CREATE PROCEDURE alter_constraint_proc()
ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED;
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
CALL alter_constraint_proc();
CALL drop_constraint_proc();
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_check UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_check` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_unique` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Constraint names are changed in ALTER TABLE operations. Re-execution of
# a stored procedure should set correct constraint type.
CALL alter_constraint_proc();
ERROR HY000: Altering constraint enforcement is not supported for the constraint 'f2_check'. Enforcement state alter is not supported for the PRIMARY, UNIQUE and FOREIGN KEY type constraints.
CALL drop_constraint_proc();
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP PROCEDURE alter_constraint_proc;
DROP PROCEDURE drop_constraint_proc;
#-----------------------------------------------------------------------
# Test case to verify alter constraint in nested stored procedure call.
#-----------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN f3 INT GENERATED ALWAYS AS (f1) STORED;
CREATE PROCEDURE alter_constraint_proc()
ALTER TABLE t1 ALTER CONSTRAINT constraint_name NOT ENFORCED;
CREATE PROCEDURE test_alter_constraint()
BEGIN
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
SELECT "Error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED is reported."
    AS error_message;
ALTER TABLE t1 ADD CONSTRAINT constraint_name UNIQUE(f2);
call alter_constraint_proc();
ALTER TABLE t1 DROP CONSTRAINT constraint_name,
ADD UNIQUE INDEX constraint_name (((COS(f3))) DESC);
call alter_constraint_proc();
ALTER TABLE t1 DROP CONSTRAINT constraint_name,
ADD CONSTRAINT constraint_name FOREIGN KEY (f1) REFERENCES t2(f1);
call alter_constraint_proc();
ALTER TABLE t1 DROP CONSTRAINT constraint_name,
ADD CONSTRAINT constraint_name CHECK (f2 > 0);
call alter_constraint_proc();
ALTER TABLE t1 DROP CONSTRAINT constraint_name;
END;
$
# Re-execution of alter_constraint_proc() in test_alter_constraint() should
# set correct constraint type.
CALL test_alter_constraint();
error_message
Error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED is reported.
error_message
Error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED is reported.
error_message
Error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED is reported.
DROP PROCEDURE alter_constraint_proc;
DROP PROCEDURE test_alter_constraint;
ALTER TABLE t1 DROP COLUMN f3;
#-----------------------------------------------------------------------
# Test case to verify alter constraint and drop constraint in prepared
# statements.
#-----------------------------------------------------------------------
PREPARE drop_constraint_stmt FROM
'ALTER TABLE t1 DROP CONSTRAINT `primary`,
                  DROP CONSTRAINT f2_unique,
                  DROP CONSTRAINT fk,
                  DROP CONSTRAINT f2_check';
PREPARE alter_constraint_stmt FROM
'ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED';
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_unique UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXECUTE alter_constraint_stmt;
EXECUTE drop_constraint_stmt;
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
ADD CONSTRAINT f2_check UNIQUE(f2),
ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_check` (`f2`),
  CONSTRAINT `fk` FOREIGN KEY (`f1`) REFERENCES `t2` (`f1`),
  CONSTRAINT `f2_unique` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Constraint names are changed in ALTER TABLE operations. Re-execution of
# prepared statement should set correct constraint type.
EXECUTE alter_constraint_stmt;
ERROR HY000: Altering constraint enforcement is not supported for the constraint 'f2_check'. Enforcement state alter is not supported for the PRIMARY, UNIQUE and FOREIGN KEY type constraints.
EXECUTE drop_constraint_stmt;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP PREPARE alter_constraint_stmt;
DROP PREPARE drop_constraint_stmt;
#-----------------------------------------------------------------------
# Test case to verify ALTER CONSTRAINT operations on temporary table.
#-----------------------------------------------------------------------
CREATE TEMPORARY TABLE tmp (f1 INT, f2 INT,
CONSTRAINT PRIMARY KEY (f1),
CONSTRAINT f2_unique UNIQUE(f2),
CONSTRAINT f2_check CHECK (f2 > 0));
SHOW CREATE TABLE tmp;
Table	Create Table
tmp	CREATE TEMPORARY TABLE `tmp` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE tmp ALTER CONSTRAINT f2_check NOT ENFORCED;
SHOW CREATE TABLE tmp;
Table	Create Table
tmp	CREATE TEMPORARY TABLE `tmp` (
  `f1` int NOT NULL,
  `f2` int DEFAULT NULL,
  PRIMARY KEY (`f1`),
  UNIQUE KEY `f2_unique` (`f2`),
  CONSTRAINT `f2_check` CHECK ((`f2` > 0)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE tmp;
DROP TABLE t1, t2;
