
#
# Test of rename table
#

--source include/count_sessions.inc

--disable_warnings
drop table if exists t0,t1,t2,t3,t4;
# Clear up from other tests (to ensure that SHOW TABLES below is right)
drop table if exists t0,t5,t6,t7,t8,t9,t1_1,t1_2,t9_1,t9_2;
--enable_warnings

create table t0 SELECT 1,"table 1";
create table t2 SELECT 2,"table 2";
create table t3 SELECT 3,"table 3";
rename table t0 to t1;
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t1;
select * from t1;
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t1;
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t1;
select * from t1;

# The following should give errors
--error ER_TABLE_EXISTS_ERROR,ER_TABLE_EXISTS_ERROR
rename table t1 to t2;
--error ER_TABLE_EXISTS_ERROR,ER_TABLE_EXISTS_ERROR
rename table t1 to t1;
--error ER_TABLE_EXISTS_ERROR,ER_TABLE_EXISTS_ERROR
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t2;
show tables like "t_";
--error ER_TABLE_EXISTS_ERROR,ER_TABLE_EXISTS_ERROR
rename table t3 to t1, t2 to t3, t1 to t2, t4 to t1;
--error ER_NO_SUCH_TABLE,ER_NO_SUCH_TABLE
rename table t3 to t4, t5 to t3, t1 to t2, t4 to t1;

select * from t1;
select * from t2;
select * from t3;

# This should give a warning for t4
drop table if exists t1,t2,t3,t4;

#
# Bug #2397 RENAME TABLES is not blocked by
# FLUSH TABLES WITH READ LOCK
#

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

connection con1;
CREATE TABLE t1 (a int);
CREATE TABLE t3 (a int);
connection con2;
FLUSH TABLES WITH READ LOCK;
connection con1;
send RENAME TABLE t1 TO t2, t3 to t4;
connection con2;
show tables;
UNLOCK TABLES;
connection con1;
reap;
connection con2;

# Wait for the the tables to be renamed
# i.e the query below succeds
let $query= select * from t2, t4;
source include/wait_for_query_to_succeed.inc;

show tables;

drop table t2, t4;

disconnect con2;
disconnect con1;
connection default;


--echo End of 4.1 tests


--echo #
--echo # Bug#14959: "ALTER TABLE isn't able to rename a view"
--echo # Bug#53976: "ALTER TABLE RENAME is allowed on views
--echo #             (not documented, broken)"
--echo #
create table t1(f1 int);
create view v1 as select * from t1;
--error ER_WRONG_OBJECT
alter table v1 rename to v2;
drop view v1;
drop table t1;

--echo End of 5.0 tests

--echo #
--echo # Bug#20197870: sql_table.cc: bool mysql_rename_table ...
--echo #               assertion `error==0\' failed.
--echo # The test crashes server without the fix.
--echo #
SET @orig_innodb_file_per_table= @@innodb_file_per_table;
SET GLOBAL innodb_file_per_table = 0;
create table t1(f1 int) engine=innodb;
--error ER_BAD_DB_ERROR
rename table test.t1 to nonexistingdb.t2;
drop table t1;
SET GLOBAL innodb_file_per_table = @orig_innodb_file_per_table;

--source include/wait_until_count_sessions.inc

--echo #
--echo # Test coverage for WL#9826 "Allow RENAME TABLES under LOCK TABLES".
--echo #

--enable_connect_log
SET @old_lock_wait_timeout= @@lock_wait_timeout;
connect (con1, localhost, root,,);
SET @old_lock_wait_timeout= @@lock_wait_timeout;
connection default;

--echo #
--echo # 1) Requirements on table locking for tables renamed and
--echo #    target table names.
--echo #
--echo # 1.1) Requirements on renamed table locks.
CREATE TABLE t1 (i INT);
CREATE TABLE t2 (j INT);
LOCK TABLES t1 READ;
--echo # Renamed table must be locked.
--error ER_TABLE_NOT_LOCKED
RENAME TABLE t2 TO t3;

--echo # Moreover, renamed table must be locked for write.
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
RENAME TABLE t1 TO t3;
UNLOCK TABLES;
LOCK TABLE t1 WRITE;

--echo # Renaming write-locked table is OK, there is no need
--echo # (and way) to lock target table name.
RENAME TABLE t1 TO t3;
--echo # There is no need to lock source name for the table, if it is
--echo # result of some earlier step of the same RENAME TABLES.
RENAME TABLE t3 TO t4, t4 TO t5;
UNLOCK TABLES;

--echo # Special case involving foreign keys. If RENAME TABLE is
--echo # going to provide parent for some orphan FK, then the
--echo # child table of this FK needs to be write locked, or
--echo # FK invariants for LOCK TABLES will be broken,
SET FOREIGN_KEY_CHECKS=0;
CREATE TABLE t1 (fk INT, FOREIGN KEY(fk) REFERENCES t3(pk));
SET FOREIGN_KEY_CHECKS=1;
CREATE TABLE t0 (pk INT PRIMARY KEY);
LOCK TABLES t0 WRITE;
--error ER_TABLE_NOT_LOCKED
RENAME TABLE t0 TO t3;
UNLOCK TABLES;
LOCK TABLES t1 READ, t0 WRITE;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
RENAME TABLE t0 TO t3;
UNLOCK TABLES;
LOCK TABLES t1 WRITE, t0 WRITE;
RENAME TABLE t0 TO t3;
UNLOCK TABLES;
DROP TABLES t1, t3;

--echo #
--echo # 1.2) Locking of target table.
--echo #
--echo # This part of test resides in rename_debug.test.

--echo #
--echo # 2) Failure to acquire/upgrade locks on tables involved.
--echo #
--echo # This part of test resides in rename_debug.test.

--echo #
--echo # 3) Effects of RENAME TABLES on set of locked tables and
--echo #    metadata locks held.
--echo #
--echo # 3.1) Trivial RENAME TABLE.
LOCK TABLES t5 WRITE;
RENAME TABLE t5 TO t4;
--echo # Table is available under new name under LOCK TABLES.
SELECT * FROM t4;
connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t4;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for old table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t5;
connection default;
UNLOCK TABLES;

--echo #
--echo # 3.2) RENAME TABLE in case when several tables are locked.
LOCK TABLES t2 READ, t4 WRITE;
RENAME TABLE t4 TO t5;
--echo # Table t2 should be still locked, and t4 should be available as t5
--echo # with correct lock type.
SELECT * FROM t2;
INSERT INTO t5 values (1);
UNLOCK TABLES;

--echo #
--echo # 3.3) RENAME TABLE in case when same table locked more than once.
LOCK TABLES t2 READ, t5 WRITE, t5 AS a WRITE, t5 AS b READ;
RENAME TABLE t5 TO t4;
--echo # Check that tables are locked under correct aliases and with modes.
SELECT * FROM t4 AS a, t4 AS b;
INSERT INTO t4 VALUES (2);
DELETE a FROM t4 AS a, t4 AS b;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
DELETE b FROM t4 AS a, t4 AS b;
UNLOCK TABLES;

--echo #
--echo # 3.4) RENAME TABLES which does old good table swap.
LOCK TABLES t2 WRITE, t4 WRITE;
RENAME TABLES t2 TO t0, t4 TO t2, t0 TO t4;
--echo # Tables are available under new name under LOCK TABLES.
SELECT * FROM t2;
SELECT * FROM t4;
connection con1;
--echo # Access by new names from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t2;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t4;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for auxiliary name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t0;
connection default;
UNLOCK TABLES;

--echo #
--echo # 3.5) RENAME TABLES which renames same table several times.
LOCK TABLE t2 WRITE;
RENAME TABLES t2 TO t1, t1 TO t3, t3 TO t5;
--echo # Table is available under new name under LOCK TABLES.
SELECT * FROM t5;
--echo # But not under other names.
--error ER_TABLE_NOT_LOCKED
SELECT * FROM t1;
--error ER_TABLE_NOT_LOCKED
SELECT * FROM t2;
--error ER_TABLE_NOT_LOCKED
SELECT * FROM t3;
connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t5;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for other names
--error ER_NO_SUCH_TABLE
SELECT * FROM t1;
--error ER_NO_SUCH_TABLE
SELECT * FROM t2;
--error ER_NO_SUCH_TABLE
SELECT * FROM t3;
connection default;
UNLOCK TABLES;

--echo #
--echo # 3.6) RENAME TABLES which renames 2 tables while additional
--echo #      table is locked.
CREATE TABLE t6(k INT);
LOCK TABLES t4 WRITE, t5 WRITE, t6 WRITE;
RENAME TABLES t4 TO t1, t5 TO t2;
--echo # Renamed tables are available under new names.
SELECT * FROM t1;
SELECT * FROM t2;
--echo # But not under other names.
--error ER_TABLE_NOT_LOCKED
SELECT * FROM t4;
--error ER_TABLE_NOT_LOCKED
SELECT * FROM t5;
--echo # Untouched table is still available.
SELECT * FROM t6;
connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t2;
--echo # And access to untouched table too.
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t6;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # Old names should not be locked.
--error ER_NO_SUCH_TABLE
SELECT * FROM t4;
--error ER_NO_SUCH_TABLE
SELECT * FROM t5;
connection default;
UNLOCK TABLES;
DROP TABLES t1, t2, t6;

--echo #
--echo # 4) Effects of failed RENAME TABLES on set of locked tables and
--echo #    metadata locks held.
--echo #
--echo # 4.1) Atomic RENAME TABLES which fails at late stage should be
--echo #      fully rolled back.
CREATE TABLE t1 (i INT) ENGINE=InnoDB;
CREATE TABLE t2 (j INT) ENGINE=InnoDB;
CREATE TABLE t3 (k INT) ENGINE=InnoDB;
CREATE TABLE t4 (l INT) ENGINE=InnoDB;
LOCK TABLES t1 WRITE, t2 WRITE, t3 WRITE;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLES t1 TO t0, t2 TO t4;
--echo # Tables are available under old names.
SELECT * FROM t1;
SELECT * FROM t2;
--echo # Including untouched table.
SELECT * FROM t3;
--echo # And not under new names.
--error ER_TABLE_NOT_LOCKED
SELECT * FROM t0;
--error ER_TABLE_NOT_LOCKED
SELECT * FROM t4;
connection con1;
--echo # Access by old names from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t2;
--echo # And access to untouched table too.
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t3;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # New names should not be locked.
--error ER_NO_SUCH_TABLE
SELECT * FROM t0;
SELECT * FROM t4;
connection default;
UNLOCK TABLES;
DROP TABLES t1, t2, t3, t4;

--echo #
--echo # 4.3) Non-atomic RENAME TABLES which fails at late stage and
--echo #      is NOT fully reverted.
--echo #
--echo # This part of test resides in rename_debug.test.

--echo #
--echo # 5) RENAME TABLES under LOCK TABLES and views.
--echo #
--echo # 5.1) Requirements on locking is similar to tables.
CREATE TABLE t1 (i INT);
CREATE TABLE t2 (j INT);
CREATE VIEW v1 AS SELECT * FROM t1;
CREATE VIEW v2 AS SELECT * FROM t2;
LOCK TABLES t1 WRITE;
--error ER_TABLE_NOT_LOCKED
RENAME TABLE v1 TO v2;
UNLOCK TABLES;
LOCK TABLES v1 READ;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
RENAME TABLE v1 TO v2;
UNLOCK TABLES;

--echo #
--echo # Coverage for target name resides in rename_debug.test.

--echo #
--echo # 5.2) Effects of RENAME TABLE on a view on the
--echo #      set of locked tables and metadata locks.
LOCK TABLES v1 WRITE;
RENAME TABLE v1 TO v3;
--echo # View is available under new name under LOCK TABLES.
INSERT INTO v3 VALUES (1);
--echo # And not under old name.
--error ER_TABLE_NOT_LOCKED
SELECT * FROM v1;
connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM v3;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for old table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM v1;
connection default;
UNLOCK TABLES;
--echo #
--echo # A bit more complex RENAME TABLE which swaps two views.
LOCK TABLES v2 WRITE, v3 WRITE;
RENAME TABLE v2 TO v0, v3 TO v2, v0 TO v3;
--echo # Views are available under new name under LOCK TABLES.
SELECT * FROM v2;
SELECT * FROM v3;
connection con1;
--echo # Access by new names from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM v2;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM v3;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for auxiliary name.
--error ER_NO_SUCH_TABLE
SELECT * FROM v0;
connection default;
UNLOCK TABLES;
DROP VIEW v2;
DROP VIEW v3;
DROP TABLES t1, t2;

--echo #
--echo # 6) RENAME TABLES under LOCK TABLES and foreign keys.
--echo #
--echo # 6.1) Renaming of child table updates parent definition.
CREATE TABLE t1 (pk INT PRIMARY KEY);
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (fk INT, FOREIGN KEY (fk) REFERENCES t1 (pk));
INSERT INTO t2 VALUES (1);
CREATE TABLE t3 (fk INT);
CREATE TABLE t4 (pk INT NOT NULL, UNIQUE(pk));
INSERT INTO t4 VALUES (2);
LOCK TABLES t1 WRITE, t2 WRITE, t3 WRITE, t4 WRITE;
RENAME TABLES t2 TO t0, t3 TO t2, t0 TO t3;
--echo # The above RENAME TABLES should have updated parent definition
--echo # so below DELETE goes to new 't3' (old 't2') when checking if
--echo # rows can be deleted.
--error ER_ROW_IS_REFERENCED_2
DELETE FROM t1;

--echo #
--echo # 6.2) Renaming of parent table updates parent definition as well.
RENAME TABLE t1 TO t0, t4 TO t1, t0 TO t4;
--echo # The above RENAME TABLES should have updated child definition
--echo # so below INSERT goes to new 't4' (old 't1') when checking if
--echo # row can be inserted.
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t3 VALUES (2);
SHOW CREATE TABLE t3;
--echo # Parent key name still should be PRIMARY.
SELECT unique_constraint_name FROM information_schema.referential_constraints WHERE table_name = 't3';
UNLOCK TABLES;

--echo #
--echo # 6.3) RENAME TABLES which adds parent to orphan FK updates
--echo #      child definition.
--echo # Make FK on t3 orphan.
SET foreign_key_checks = 0;
DROP TABLES t1, t2, t4;
SET foreign_key_checks = 1;
CREATE TABLE t1 (pk INT NOT NULL, UNIQUE(pk));
INSERT INTO t1 VALUES (1), (2);
LOCK TABLES t1 WRITE, t3 WRITE;
--echo # Add parent for orphan FK.
RENAME TABLE t1 TO t4;
--echo # Child definition should be updated to reflect new parent.
INSERT INTO t3 VALUES (2);
--error ER_NO_REFERENCED_ROW_2
INSERT INTO t3 VALUES (3);
--echo # Parent key name should be 'pk'.
SELECT unique_constraint_name FROM information_schema.referential_constraints WHERE table_name = 't3';
--echo # Parent definition should be aware of new child as well.
--error ER_ROW_IS_REFERENCED_2
DELETE FROM t4;
UNLOCK TABLES;
DROP TABLE t3, t4;

--echo # Clean-up.
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
connection default;
--disable_connect_log
