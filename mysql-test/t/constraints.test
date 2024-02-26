 #Get deafult engine value
--let $DEFAULT_ENGINE = `select @@global.default_storage_engine`

#
# Testing of constraints
# Currently MySQL only ignores the syntax.
#
--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (a int check (a>0));
insert into t1 values (1);
--error ER_CHECK_CONSTRAINT_VIOLATED
insert into t1 values (0);
drop table t1;
create table t1 (a int, b int, check (a>b));
insert into t1 values (1,0);
--error ER_CHECK_CONSTRAINT_VIOLATED
insert into t1 values (0,1);
drop table t1;
create table t1 (a int ,b int, constraint abc check (a>b));
insert into t1 values (1,0);
--error ER_CHECK_CONSTRAINT_VIOLATED
insert into t1 values (0,1);
drop table t1;
create table t1 (a int null);
insert into t1 values (1),(NULL);
drop table t1;
create table t1 (a int null);
alter table t1 add constraint constraint_1 unique (a);
alter table t1 add constraint unique key_1(a);
alter table t1 add constraint constraint_2 unique key_2(a);

#Replace default engine value with static engine string 
--replace_result $DEFAULT_ENGINE ENGINE
show create table t1;
drop table t1;

# End of 4.1 tests

#
# Bug#35578 (Parser allows useless/illegal CREATE TABLE syntax)
#

--disable_warnings
drop table if exists t_illegal;
--enable_warnings

--error ER_PARSE_ERROR
create table t_illegal (a int, b int, check a>b);

--error ER_PARSE_ERROR
create table t_illegal (a int, b int, constraint abc check a>b);

--error ER_PARSE_ERROR
create table t_illegal (a int, b int, constraint abc);

#
# Bug#11714 (Non-sensical ALTER TABLE ADD CONSTRAINT allowed)
#

--disable_warnings
drop table if exists t_11714;
--enable_warnings

create table t_11714(a int, b int);

--error ER_PARSE_ERROR
alter table t_11714 add constraint cons1;

drop table t_11714;

#
# Bug#38696 (CREATE TABLE ... CHECK ... allows illegal syntax)

--error ER_PARSE_ERROR
CREATE TABLE t_illegal (col_1 INT CHECK something (whatever));

--error ER_PARSE_ERROR
CREATE TABLE t_illegal (col_1 INT CHECK something);


--echo ###############################################################################
--echo # WL#12798 - Implement ALTER TABLE ... DROP/ALTER CONSTRAINT syntax.          #
--echo #            Test cases to verify ALTER TABLE ... DROP CONSTRAINT clause.     #
--echo ###############################################################################

CREATE TABLE t1 (f1 INT, f2 INT);
CREATE TABLE t2 (f1 INT PRIMARY KEY);

--echo #-----------------------------------------------------------------------
--echo # Testing scenario where DROP CONSTRAINT fail.
--echo #-----------------------------------------------------------------------
--error ER_PARSE_ERROR
ALTER TABLE t1 DROP CONSTRAINT;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop constraint with unknown (or non-existing)
--echo # constraint.
--echo #-----------------------------------------------------------------------
--error ER_CONSTRAINT_NOT_FOUND
ALTER TABLE t1 DROP CONSTRAINT unknown;
ALTER TABLE t1 ADD CONSTRAINT f2_check CHECK (f2 > 0);
--error ER_CONSTRAINT_NOT_FOUND
ALTER TABLE t1 DROP CONSTRAINT unknown;
ALTER TABLE t1 DROP CONSTRAINT f2_check;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop constraint with all types of table
--echo # constraints.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_unique UNIQUE (f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
ALTER TABLE t1 DROP CONSTRAINT fk;
ALTER TABLE t1 DROP CONSTRAINT `primary`;
ALTER TABLE t1 DROP CONSTRAINT f2_unique;
ALTER TABLE t1 DROP CONSTRAINT f2_check;
SHOW CREATE TABLE t1;

ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_unique UNIQUE (f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;
ALTER TABLE t1 DROP CONSTRAINT `primary`,
	       DROP CONSTRAINT f2_unique,
	       DROP CONSTRAINT fk,
	       DROP CONSTRAINT f2_check;
SHOW CREATE TABLE t1;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop multiple type constraints with the same name.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT name2 UNIQUE (f1),
               ADD CONSTRAINT `primary` FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT name2 CHECK (f2 > 0);
SHOW CREATE TABLE t1;

--error ER_MULTIPLE_CONSTRAINTS_WITH_SAME_NAME
ALTER TABLE t1 DROP CONSTRAINT `primary`;

--error ER_MULTIPLE_CONSTRAINTS_WITH_SAME_NAME
ALTER TABLE t1 DROP CONSTRAINT name2;

ALTER TABLE t1 DROP PRIMARY KEY,
               DROP INDEX name2,
               DROP FOREIGN KEY `primary`,
               DROP CHECK name2;
SHOW CREATE TABLE t1;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop constraint with constraint specific drop
--echo # clause and DROP CONSTRAINT clause.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_unique UNIQUE (f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;

ALTER TABLE t1 DROP CONSTRAINT `primary`,
               DROP FOREIGN KEY fk,
	       DROP CONSTRAINT f2_unique,
	       DROP CHECK f2_check;
SHOW CREATE TABLE t1;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop constraint and add constraint with same name
--echo # in the ALTER statement.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_unique UNIQUE (f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_check CHECK (f2 > 0),
               ADD COLUMN f3 INT;
SHOW CREATE TABLE t1;

ALTER TABLE t1 DROP CONSTRAINT `primary`, ADD CONSTRAINT PRIMARY KEY (f1),
               DROP CONSTRAINT f2_unique, ADD CONSTRAINT f2_unique UNIQUE (f3),
               DROP CONSTRAINT f2_check,
               ADD CONSTRAINT f2_check CHECK ((f3 + f2 + f1) < 999);
SHOW CREATE TABLE t1;

ALTER TABLE t1 DROP CONSTRAINT `primary`,
               DROP CONSTRAINT fk,
               DROP CONSTRAINT f2_unique,
               DROP CONSTRAINT f2_check,
               DROP COLUMN f3;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop primary constraint operation when
--echo # sql_require_primary_key = true.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1);

SET SESSION sql_require_primary_key = true;

--echo # Drop primary key operation fails when sql_require_primary_key = true.
--error ER_TABLE_WITHOUT_PK
ALTER TABLE t1 DROP PRIMARY KEY;
--echo # Drop primary key operation fails when sql_require_primary_key = true.
--error ER_TABLE_WITHOUT_PK
ALTER TABLE t1 DROP CONSTRAINT `primary`;

SET SESSION sql_require_primary_key = default;
ALTER TABLE t1 DROP CONSTRAINT `primary`;

--echo #-----------------------------------------------------------------------
--echo # Test case to check if generated hidden columns are dropped on
--echo # functional index drop operation.
--echo #-----------------------------------------------------------------------
CREATE TABLE t3 (col1 INT, col2 INT GENERATED ALWAYS AS (col1) STORED);
ALTER TABLE t3 ADD UNIQUE INDEX idx (((COS( col2 ))) DESC);
ALTER TABLE t3 DROP CONSTRAINT idx;
--echo # Assert condition is hit in debug build if generated hidden column is
--echo # not dropped in index idx drop operation.
ALTER TABLE t3 ADD UNIQUE INDEX idx (((COS( col1 ))) DESC);
DROP TABLE t3;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify DROP CONSTRAINT operations on temporary table.
--echo #-----------------------------------------------------------------------
CREATE TEMPORARY TABLE tmp (f1 INT, f2 INT,
                            CONSTRAINT PRIMARY KEY (f1),
                            CONSTRAINT f2_unique UNIQUE(f2),
                            CONSTRAINT f2_check CHECK (f2 > 0));
SHOW CREATE TABLE tmp;
ALTER TABLE tmp DROP CONSTRAINT `primary`,
                DROP CONSTRAINT f2_check,
                DROP CONSTRAINT f2_unique;
SHOW CREATE TABLE tmp;
DROP TABLE tmp;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop constraint in stored procedures.
--echo #-----------------------------------------------------------------------
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

CALL drop_constraint_proc();

ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_check UNIQUE(f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;

--echo # Constraint names are changed in ALTER TABLE operations. Re-execution of
--echo # a stored procedure should set correct constraint type.
CALL drop_constraint_proc();

SHOW CREATE TABLE t1;
DROP PROCEDURE drop_constraint_proc;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop constraint in nested stored procedure call.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN f3 INT GENERATED ALWAYS AS (f1) STORED;

CREATE PROCEDURE drop_constraint_proc()
  ALTER TABLE t1 DROP CONSTRAINT constraint_name;

DELIMITER $;
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
DELIMITER ;$

--echo # Re-execution of drop_constraint_proc() in test_drop_constraint() should
--echo # set correct constraint type.
CALL test_drop_constraint();

DROP PROCEDURE drop_constraint_proc;
DROP PROCEDURE test_drop_constraint;
ALTER TABLE t1 DROP COLUMN f3;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify drop constraint in prepared statements.
--echo #-----------------------------------------------------------------------
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

EXECUTE drop_constraint_stmt;

ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_check UNIQUE(f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;

--echo # Constraint names are changed in ALTER TABLE operations. Re-execution of
--echo # prepared statement should set correct constraint type.
EXECUTE drop_constraint_stmt;

SHOW CREATE TABLE t1;
DROP PREPARE drop_constraint_stmt;


--echo ###############################################################################
--echo # WL#12798 - Implement ALTER TABLE ... DROP/ALTER CONSTRAINT syntax.          #
--echo #            Test cases to verify ALTER TABLE ... ALTER CONSTRAINT clause.    #
--echo ###############################################################################
--echo #-----------------------------------------------------------------------
--echo # Testing scenario where ALTER CONSTRAINT fail.
--echo #-----------------------------------------------------------------------
--error ER_PARSE_ERROR
ALTER TABLE t1 ALTER CONSTRAINT;
--error ER_PARSE_ERROR
ALTER TABLE t1 ALTER CONSTRAINT unknown NOT;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter constraint with unknown(or non-existing)
--echo # constraint.
--echo #-----------------------------------------------------------------------
--error ER_CONSTRAINT_NOT_FOUND
ALTER TABLE t1 ALTER CONSTRAINT unknown ENFORCED;
ALTER TABLE t1 ADD CONSTRAINT f2_check CHECK (f2 > 0);
--error ER_CONSTRAINT_NOT_FOUND
ALTER TABLE t1 ALTER CONSTRAINT unknown ENFORCED;
ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED;
ALTER TABLE t1 DROP CONSTRAINT f2_check;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter multiple type constraints with the same name.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT name2 UNIQUE (f1),
               ADD CONSTRAINT `primary` FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT name2 CHECK (f2 > 0);
SHOW CREATE TABLE t1;

--error ER_MULTIPLE_CONSTRAINTS_WITH_SAME_NAME
ALTER TABLE t1 ALTER CONSTRAINT name2 NOT ENFORCED;

ALTER TABLE t1 DROP PRIMARY KEY,
               DROP INDEX name2,
               DROP FOREIGN KEY `primary`,
               DROP CHECK name2;
SHOW CREATE TABLE t1;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter unsupported constraint.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_unique UNIQUE (f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;

--error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED
ALTER TABLE t1 ALTER CONSTRAINT `primary` NOT ENFORCED;

--error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED
ALTER TABLE t1 ALTER CONSTRAINT f2_unique NOT ENFORCED;

--error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED
ALTER TABLE t1 ALTER CONSTRAINT fk NOT ENFORCED;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter supported constraint.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED;
SHOW CREATE TABLE t1;

ALTER TABLE t1 DROP CONSTRAINT `primary`,
	       DROP CONSTRAINT f2_unique,
	       DROP CONSTRAINT fk,
	       DROP CONSTRAINT f2_check;
SHOW CREATE TABLE t1;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter constraint with constraint specific alter
--echo # clause and ALTER CONSTRAINT clause.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD CONSTRAINT f2_check CHECK (f2 > 0);
SHOW CREATE TABLE t1;

ALTER TABLE t1 ALTER CHECK f2_check NOT ENFORCED, ALTER CHECK f2_check ENFORCED;
SHOW CREATE TABLE t1;

ALTER TABLE t1 ALTER CONSTRAINT f2_check NOT ENFORCED, ALTER CONSTRAINT f2_check ENFORCED;
SHOW CREATE TABLE t1;

ALTER TABLE t1 ALTER CHECK f2_check NOT ENFORCED, ALTER CONSTRAINT f2_check ENFORCED;
SHOW CREATE TABLE t1;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify order of execution when ALTER statement contains
--echo # both DROP CONSTRAINT and ALTER CONSTRAINT operation on the same
--echo # constraint.
--echo #-----------------------------------------------------------------------
--echo # First drop operations are executed and the alter operations. So alter
--echo # operation reports an error here.
--error ER_CHECK_CONSTRAINT_NOT_FOUND
ALTER TABLE t1 DROP CONSTRAINT f2_check, ALTER CONSTRAINT f2_check NOT ENFORCED;
ALTER TABLE t1 DROP CONSTRAINT f2_check;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter constraint and drop constraint in stored
--echo # procedures.
--echo #-----------------------------------------------------------------------
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

CALL alter_constraint_proc();
CALL drop_constraint_proc();

ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_check UNIQUE(f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;

--echo # Constraint names are changed in ALTER TABLE operations. Re-execution of
--echo # a stored procedure should set correct constraint type.
--error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED
CALL alter_constraint_proc();
CALL drop_constraint_proc();

SHOW CREATE TABLE t1;
DROP PROCEDURE alter_constraint_proc;
DROP PROCEDURE drop_constraint_proc;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter constraint in nested stored procedure call.
--echo #-----------------------------------------------------------------------
ALTER TABLE t1 ADD COLUMN f3 INT GENERATED ALWAYS AS (f1) STORED;

CREATE PROCEDURE alter_constraint_proc()
  ALTER TABLE t1 ALTER CONSTRAINT constraint_name NOT ENFORCED;

DELIMITER $;
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
DELIMITER ;$

--echo # Re-execution of alter_constraint_proc() in test_alter_constraint() should
--echo # set correct constraint type.
CALL test_alter_constraint();

DROP PROCEDURE alter_constraint_proc;
DROP PROCEDURE test_alter_constraint;
ALTER TABLE t1 DROP COLUMN f3;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify alter constraint and drop constraint in prepared
--echo # statements.
--echo #-----------------------------------------------------------------------
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

EXECUTE alter_constraint_stmt;
EXECUTE drop_constraint_stmt;

ALTER TABLE t1 ADD CONSTRAINT PRIMARY KEY (f1),
               ADD CONSTRAINT f2_check UNIQUE(f2),
               ADD CONSTRAINT fk FOREIGN KEY (f1) REFERENCES t2(f1),
               ADD CONSTRAINT f2_unique CHECK (f2 > 0);
SHOW CREATE TABLE t1;

--echo # Constraint names are changed in ALTER TABLE operations. Re-execution of
--echo # prepared statement should set correct constraint type.
--error ER_ALTER_CONSTRAINT_ENFORCEMENT_NOT_SUPPORTED
EXECUTE alter_constraint_stmt;
EXECUTE drop_constraint_stmt;

SHOW CREATE TABLE t1;
DROP PREPARE alter_constraint_stmt;
DROP PREPARE drop_constraint_stmt;

--echo #-----------------------------------------------------------------------
--echo # Test case to verify ALTER CONSTRAINT operations on temporary table.
--echo #-----------------------------------------------------------------------
CREATE TEMPORARY TABLE tmp (f1 INT, f2 INT,
                            CONSTRAINT PRIMARY KEY (f1),
                            CONSTRAINT f2_unique UNIQUE(f2),
                            CONSTRAINT f2_check CHECK (f2 > 0));
SHOW CREATE TABLE tmp;
ALTER TABLE tmp ALTER CONSTRAINT f2_check NOT ENFORCED;
SHOW CREATE TABLE tmp;
DROP TABLE tmp;

#Clean-up
DROP TABLE t1, t2;
