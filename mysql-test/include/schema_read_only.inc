#--------------------------------------------------------------------
# Test of WL#13369.
#--------------------------------------------------------------------

--source include/have_debug.inc
--source include/not_valgrind.inc

--let $LCTN = `SELECT @@lower_case_table_names`

--let $CREATE_S = S
if ($LCTN) {
  --let $CREATE_S = s
}

--enable_connect_log
--echo #--------------------------------------------------------------------
--echo # FR1, FR2: Option name, values, persistence.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;

--echo # Invalid values.
--error ER_PARSE_ERROR
ALTER SCHEMA S read only=on;
--error ER_PARSE_ERROR
ALTER SCHEMA S read only=off;
--error ER_PARSE_ERROR
ALTER SCHEMA S read only=-1;
--error ER_PARSE_ERROR
ALTER SCHEMA S read only=2;

--echo # Hyphen invalid in option names.
--error ER_PARSE_ERROR
ALTER SCHEMA S read-only=0;
--error ER_PARSE_ERROR
ALTER SCHEMA S READ-ONLY=0;

--echo # Valid values.
ALTER SCHEMA S read only=0;
ALTER SCHEMA S read only=default;
ALTER SCHEMA S read only=1;
ALTER SCHEMA S READ ONLY=0;
ALTER SCHEMA S READ ONLY=DEFAULT;
ALTER SCHEMA S READ ONLY=1;

--echo # Value survives restart.
SHOW CREATE SCHEMA S;
--error ER_SCHEMA_READ_ONLY
CREATE TABLE S.t(i INT);
let $wait_counter= 10000;
source include/restart_mysqld.inc;
SHOW CREATE SCHEMA S;
--error ER_SCHEMA_READ_ONLY
CREATE TABLE S.t(i INT);

ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR3: Invalid option in CREATE SCHEMA.
--echo #--------------------------------------------------------------------
--error ER_PARSE_ERROR
eval CREATE SCHEMA $CREATE_S read only=0;

--echo #--------------------------------------------------------------------
--echo # FR4: ALTER privilege needed to change read only.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
CREATE USER s_usr;
--connect (s_con, localhost, s_usr,,)
--error ER_DBACCESS_DENIED_ERROR
ALTER SCHEMA S read only=1;

--connection default
GRANT ALTER ON S.* TO s_usr;

--connection s_con
ALTER SCHEMA S read only=1;
ALTER SCHEMA S read only=0;

--connection default
--disconnect s_con
DROP USER s_usr;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR5: Visibility in SHOW CREATE.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S read only=1;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S read only=0;
SHOW CREATE SCHEMA S;

--echo # Re-create based on SHOW output.
ALTER SCHEMA S READ ONLY=1;
--let $str = `SHOW CREATE SCHEMA S`
--let $str = `SELECT SUBSTRING("$str", 2)`
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;
--eval $str

--echo # Note read only option ignored in CREATE.
SHOW CREATE SCHEMA S;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR6: I_S.SCHEMATA_extensions.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
SELECT * FROM information_schema.schemata_extensions ORDER BY SCHEMA_NAME;
ALTER SCHEMA S READ ONLY=1;
SELECT * FROM information_schema.schemata_extensions ORDER BY SCHEMA_NAME;
ALTER SCHEMA S READ ONLY=0;
SELECT * FROM information_schema.schemata_extensions ORDER BY SCHEMA_NAME;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR7: The read only option affects all users.
--echo #--------------------------------------------------------------------
CREATE USER con_adm_usr;
GRANT ALL ON *.* TO con_adm_usr;
GRANT CONNECTION_ADMIN ON *.* TO con_adm_usr;

CREATE USER super_usr;
GRANT ALL ON *.* TO super_usr;
GRANT SUPER ON *.* TO super_usr;

eval CREATE SCHEMA $CREATE_S;

--connect (root_con, localhost, root,,)
CREATE TABLE S.t(i INT);
DROP TABLE S.t;
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
CREATE TABLE S.t(i INT);
ALTER SCHEMA S READ ONLY=0;

--connect (con_adm_con, localhost, con_adm_usr,,)
CREATE TABLE S.t(i INT);
DROP TABLE S.t;
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
CREATE TABLE S.t(i INT);
ALTER SCHEMA S READ ONLY=0;

--connect (super_con, localhost, super_usr,,)
CREATE TABLE S.t(i INT);
DROP TABLE S.t;
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
CREATE TABLE S.t(i INT);
ALTER SCHEMA S READ ONLY=0;

--connection default
--disconnect root_con
--disconnect con_adm_con
--disconnect super_con

DROP USER con_adm_usr;
DROP USER super_usr;

DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR8: The read only option shall be replicated.
--echo #--------------------------------------------------------------------
--echo # See separate test rpl.rpl_alter_db.

--echo #--------------------------------------------------------------------
--echo # FR9: Initialization, restart, upgrade and replication not affected.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
ALTER SCHEMA S READ ONLY=1;

let BASEDIR=`select @@basedir`;
source include/shutdown_mysqld.inc;

--echo # Initialization.
let DDIR= $MYSQL_TMP_DIR/schema_readonly_test;
let MYSQLD_LOG= $MYSQLTEST_VARDIR/log/schema_readonly.log;
let extra_args= --no-defaults --console --secure-file-priv=""
    --basedir=$BASEDIR --lc-messages-dir=$MYSQL_SHAREDIR --datadir=$DDIR
    --log-error=$MYSQLD_LOG;
--exec $MYSQLD $extra_args --debug="+d,schema_read_only" --initialize-insecure
--force-rmdir $DDIR

--echo # Restart.
let $wait_counter= 10000;
let $restart_parameters= "restart: --debug=+d,schema_read_only";
source include/start_mysqld.inc;
SHOW TABLES IN S;
SHOW CREATE SCHEMA S;

--echo # Upgrade.
--echo # Copy tables since upgrade will break check testcase.
let $backup= 1;
source include/backup_tables_priv_and_users.inc;
let $wait_counter= 10000;
let $restart_parameters=
  "restart: --upgrade=force --debug=+d,schema_read_only";
source include/restart_mysqld.inc;
--echo # Restore copied tables.
let $restore= 1;
source include/backup_tables_priv_and_users.inc;
SHOW TABLES IN S;
SHOW CREATE SCHEMA S;

--echo # Replication: See separate test rpl.rpl_alter_db.

--echo #--------------------------------------------------------------------
--echo # FR10: Statements in an init file shall not be affected.
--echo #--------------------------------------------------------------------
let INIT_FILE=$MYSQLTEST_VARDIR/tmp/schema_read_only.sql;
write_file $INIT_FILE;
  CREATE SCHEMA schema_read_only;
  ALTER SCHEMA schema_read_only READ ONLY=1;
  ALTER SCHEMA schema_read_only COLLATE utf8_bin;
  CREATE TABLE schema_read_only.t(i INT) TABLESPACE innodb_system;
  DROP SCHEMA schema_read_only;
  CREATE TABLE S.init_file(i INT);
EOF

let $wait_counter= 10000;
let $restart_parameters= "restart: --init-file=$INIT_FILE";
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
source include/restart_mysqld.inc;

--error ER_BAD_DB_ERROR
SHOW CREATE SCHEMA schema_read_only;

SHOW TABLES IN S;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;

--remove_file $INIT_FILE

--echo #--------------------------------------------------------------------
--echo # FR11: No ALTER for mysql, I_S, P_S and sys.
--echo #--------------------------------------------------------------------
--error ER_NO_SYSTEM_SCHEMA_ACCESS
ALTER SCHEMA mysql READ ONLY=1;
--error ER_DBACCESS_DENIED_ERROR
ALTER SCHEMA information_schema READ ONLY=1;
--error ER_DBACCESS_DENIED_ERROR
ALTER SCHEMA performance_schema READ ONLY=1;

if ($LCTN) {
  --error ER_NO_SYSTEM_SCHEMA_ACCESS
  ALTER SCHEMA MYSQL READ ONLY=1;
  --error ER_DBACCESS_DENIED_ERROR
  ALTER SCHEMA INFORMATION_SCHEMA READ ONLY=1;
  --error ER_DBACCESS_DENIED_ERROR
  ALTER SCHEMA PERFORMANCE_SCHEMA READ ONLY=1;
}

--echo #--------------------------------------------------------------------
--echo # FR12: Restrictions on statement execution.
--echo #--------------------------------------------------------------------
--echo # Sample read only schema.
--let $MYSQLD_DATADIR=`SELECT @@datadir`
eval CREATE SCHEMA $CREATE_S;

CREATE TABLE S.t1(i INT, j INT, INDEX i1(i));
ALTER SCHEMA S read only=1;
SHOW CREATE SCHEMA S;
SELECT schema_name FROM information_schema.schemata ORDER BY schema_name;

--echo # Schema operations.
--error ER_DB_CREATE_EXISTS
eval CREATE SCHEMA $CREATE_S;
--error ER_SCHEMA_READ_ONLY
ALTER SCHEMA S COLLATE utf8_bin;
--error ER_SCHEMA_READ_ONLY
ALTER SCHEMA S CHARACTER SET ascii;
--error ER_SCHEMA_READ_ONLY
DROP SCHEMA S;

--echo # Table operations.
--error ER_SCHEMA_READ_ONLY
CREATE TABLE S.t2(i INT);
--error ER_SCHEMA_READ_ONLY
ALTER TABLE S.t1 ADD COLUMN (k INT), ALGORITHM INSTANT;
--error ER_SCHEMA_READ_ONLY
ALTER TABLE S.t1 ADD COLUMN (k INT), ALGORITHM INPLACE;
--error ER_SCHEMA_READ_ONLY
ALTER TABLE S.t1 ADD COLUMN (k INT), ALGORITHM COPY;
--error ER_SCHEMA_READ_ONLY
CREATE INDEX i2 ON S.t1(j);
--error ER_SCHEMA_READ_ONLY
DROP INDEX i1 ON S.t1;
--error ER_SCHEMA_READ_ONLY
RENAME TABLE S.t1 TO S.t2;
CREATE TABLE test.t1 LIKE S.t1;
--error ER_SCHEMA_READ_ONLY
RENAME TABLE S.t1 TO S.t2;
--error ER_SCHEMA_READ_ONLY
RENAME TABLE test.t1 TO test.t2, S.t1 TO S.t2;
--error ER_SCHEMA_READ_ONLY
TRUNCATE TABLE S.t1;
--error ER_SCHEMA_READ_ONLY
DROP TABLE S.t1;
--error ER_SCHEMA_READ_ONLY
DROP TABLE test.t1, S.t1;
DROP TABLE test.t1;
SELECT * FROM S.t1;
--error ER_SCHEMA_READ_ONLY
DELETE FROM S.t1;
--error ER_SCHEMA_READ_ONLY
INSERT INTO S.t1 VALUES (1, 1);
--error ER_SCHEMA_READ_ONLY
REPLACE INTO S.t1 VALUES (1, 1);
--error ER_SCHEMA_READ_ONLY
UPDATE S.t1 SET i = j;

--echo # Import
ALTER SCHEMA S READ ONLY=0;
CREATE TABLE S.t_imp (i INT) ENGINE=MYISAM;
INSERT INTO S.t_imp VALUES (1), (3), (5);
SELECT * FROM S.t_imp;

FLUSH TABLES WITH READ LOCK;
--copy_files_wildcard $MYSQLD_DATADIR/$CREATE_S/ $MYSQL_TMP_DIR t_imp*
UNLOCK TABLES;
DROP TABLE S.t_imp;

--copy_files_wildcard $MYSQL_TMP_DIR $MYSQLD_DATADIR/$CREATE_S/ t_imp*
ALTER SCHEMA S READ ONLY=1;
USE S;
--error ER_SCHEMA_READ_ONLY
IMPORT TABLE FROM 't_imp*.sdi';
--remove_files_wildcard $MYSQL_TMP_DIR t_imp*
--remove_files_wildcard $MYSQLD_DATADIR/$CREATE_S/ t_imp*

--echo # LOAD DATA INFILE/XML
SELECT 1, 1 INTO OUTFILE 't.txt';
--error ER_SCHEMA_READ_ONLY
LOAD DATA INFILE 't.txt' INTO TABLE S.t1;
--remove_file  $MYSQLD_DATADIR/$CREATE_S/t.txt

CREATE TABLE test.t1 LIKE S.t1;
INSERT INTO test.t1 VALUES (1, 1);
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--exec $MYSQL_DUMP --xml test t1 > "$MYSQLTEST_VARDIR/tmp/t1.xml" 2>&1
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--error ER_SCHEMA_READ_ONLY
--eval LOAD XML INFILE "$MYSQLTEST_VARDIR/tmp/t1.xml" INTO TABLE S.t1;
--remove_file $MYSQLTEST_VARDIR/tmp/t1.xml

--echo # View operations.
--error ER_SCHEMA_READ_ONLY
CREATE VIEW S.v1 AS SELECT * FROM S.t1;
ALTER SCHEMA S READ ONLY=0;
CREATE VIEW S.v1 AS SELECT * FROM test.t1;
ALTER SCHEMA S READ ONLY=1;

--echo # Operations altering the validity of a view in a read only schema are
--echo # rejected.
--error ER_SCHEMA_READ_ONLY
DROP TABLE test.t1;
ALTER SCHEMA S READ ONLY=0;
DROP TABLE test.t1;
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
CREATE TABLE test.t1 LIKE S.t1;

--error ER_SCHEMA_READ_ONLY
ALTER VIEW S.v1 AS SELECT * FROM S.t1;
--error ER_SCHEMA_READ_ONLY
DROP VIEW S.v1;
ALTER SCHEMA S READ ONLY=0;
DROP VIEW S.v1;
ALTER SCHEMA S READ ONLY=1;

--echo # Inserts in updatable view will fail.
CREATE VIEW test.v AS SELECT * FROM S.t1;
SELECT * FROM test.v;
--error ER_SCHEMA_READ_ONLY
INSERT INTO test.v VALUES(1, 1);
DROP VIEW test.v;

--echo # Functions. Recursion not allowed, DDL causing implicit commit not
--echo # allowed.
--error ER_SCHEMA_READ_ONLY
CREATE FUNCTION S.f() RETURNS INT RETURN 1;

CREATE TABLE test.t1 LIKE S.t1;

ALTER SCHEMA S READ ONLY=0;
delimiter |;
CREATE FUNCTION S.f(stmt VARCHAR(20)) RETURNS INT
BEGIN
  CASE stmt
    WHEN 'INSERT s' THEN INSERT INTO S.t1 VALUES(1, 1);
    WHEN 'INSERT test' THEN INSERT INTO test.t1 VALUES(1, 1);
    ELSE BEGIN END;
  END CASE;
  RETURN 1;
END|
delimiter ;|
CREATE VIEW S.v_s AS SELECT S.f('INSERT s');
CREATE VIEW S.v_test AS SELECT S.f('INSERT test');
ALTER SCHEMA S READ ONLY=1;

delimiter |;
CREATE FUNCTION test.f(stmt VARCHAR(20)) RETURNS INT
BEGIN
  CASE stmt
    WHEN 'INSERT s' THEN INSERT INTO S.t1 VALUES(1, 1);
    WHEN 'INSERT test' THEN INSERT INTO test.t1 VALUES(1, 1);
    ELSE BEGIN END;
  END CASE;
  RETURN 1;
END|
delimiter ;|
CREATE VIEW test.v_s AS SELECT test.f('INSERT s');
CREATE VIEW test.v_test AS SELECT test.f('INSERT test');

--echo # Fails due to prelocking strategy.
--error ER_SCHEMA_READ_ONLY
SELECT S.f('INSERT test');
--error ER_SCHEMA_READ_ONLY
SELECT S.f('INSERT s');
--error ER_SCHEMA_READ_ONLY
SELECT * FROM S.v_test;
--error ER_SCHEMA_READ_ONLY
SELECT * FROM S.v_s;
--error ER_SCHEMA_READ_ONLY
DROP FUNCTION S.f;

--echo # Fails due to prelocking strategy.
--error ER_SCHEMA_READ_ONLY
SELECT test.f('INSERT test');
--error ER_SCHEMA_READ_ONLY
SELECT test.f('INSERT s');
--error ER_SCHEMA_READ_ONLY
SELECT * FROM test.v_test;
--error ER_SCHEMA_READ_ONLY
SELECT * FROM test.v_s;

ALTER SCHEMA S READ ONLY=0;
SELECT S.f('INSERT test');
SELECT S.f('INSERT s');
SELECT * FROM S.v_test;
SELECT * FROM S.v_s;

SELECT test.f('INSERT test');
SELECT test.f('INSERT s');
SELECT * FROM test.v_test;
SELECT * FROM test.v_s;

DROP VIEW S.v_s;
DROP VIEW S.v_test;
DROP FUNCTION S.f;
ALTER SCHEMA S READ ONLY=1;

DROP VIEW test.v_s;
DROP VIEW test.v_test;
DROP FUNCTION test.f;
DROP TABLE test.t1;

--echo # Procedures.
--error ER_SCHEMA_READ_ONLY
CREATE PROCEDURE S.p() BEGIN END;

CREATE TABLE test.t1 LIKE S.t1;

ALTER SCHEMA S READ ONLY=0;
delimiter |;
CREATE PROCEDURE S.p(stmt VARCHAR(20))
BEGIN
  CASE stmt
    WHEN 'INSERT s' THEN INSERT INTO S.t1 VALUES(1, 1);
    WHEN 'INSERT test' THEN INSERT INTO test.t1 VALUES(1, 1);
    ELSE BEGIN END;
  END CASE;
END|
delimiter ;|
ALTER SCHEMA S READ ONLY=1;

delimiter |;
CREATE PROCEDURE test.p(stmt VARCHAR(20))
BEGIN
  CASE stmt
    WHEN 'INSERT s' THEN INSERT INTO S.t1 VALUES(1, 1);
    WHEN 'INSERT test' THEN INSERT INTO test.t1 VALUES(1, 1);
    ELSE BEGIN END;
  END CASE;
END|
delimiter ;|

--error ER_SCHEMA_READ_ONLY
CALL S.p('INSERT s');

--echo # Succeeds due to no prelocking for CALL.
CALL S.p('INSERT test');

--error ER_SCHEMA_READ_ONLY
DROP PROCEDURE S.p;

--error ER_SCHEMA_READ_ONLY
CALL test.p('INSERT s');

--echo # Succeeds due to no prelocking for CALL.
CALL test.p('INSERT test');

ALTER SCHEMA S READ ONLY=0;
CALL S.p('INSERT s');
CALL S.p('INSERT test');
DROP PROCEDURE S.p;

CALL test.p('INSERT s');
CALL test.p('INSERT test');
ALTER SCHEMA S READ ONLY=1;

DROP PROCEDURE test.p;
DROP TABLE test.t1;

--echo # Triggers.
CREATE TABLE test.t1 LIKE S.t1;

--error ER_SCHEMA_READ_ONLY
CREATE TRIGGER S.eq BEFORE INSERT ON S.t1 FOR EACH ROW SET NEW.j = NEW.i;

ALTER SCHEMA S READ ONLY=0;
CREATE TRIGGER S.ins_upd
  BEFORE INSERT ON S.t1 FOR EACH ROW UPDATE test.t1 SET i = j;
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
DROP TRIGGER S.ins_upd;

CREATE TRIGGER test.ins_upd
  BEFORE INSERT ON test.t1 FOR EACH ROW UPDATE S.t1 SET j = i;

--error ER_SCHEMA_READ_ONLY
INSERT INTO S.t1 VALUES (1, 2);
--error ER_SCHEMA_READ_ONLY
INSERT INTO test.t1 VALUES (3, 4);

ALTER SCHEMA S READ ONLY=0;
INSERT INTO S.t1 VALUES (1, 2);
INSERT INTO test.t1 VALUES (3, 4);
DROP TRIGGER S.ins_upd;
ALTER SCHEMA S READ ONLY=1;

DROP TRIGGER test.ins_upd;
DROP TABLE test.t1;

--echo # Events.

--echo # Restart to get a separate log file.
--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/event_error.log
--let $wait_counter= 10000
--let $restart_parameters= "restart: --log-error=$MYSQLD_LOG"
--replace_result $MYSQLD_LOG MYSQLD_LOG
--source include/restart_mysqld.inc

CREATE TABLE test.t1 LIKE S.t1;
CREATE EVENT test.ev
  ON SCHEDULE EVERY 1 SECOND DO INSERT INTO S.t1 VALUES (1, 2);

--sleep 5

--error ER_SCHEMA_READ_ONLY
CREATE EVENT S.ev
  ON SCHEDULE EVERY 1 SECOND DO INSERT INTO test.t1 VALUES (1, 2);

ALTER SCHEMA S READ ONLY=0;
CREATE EVENT S.ev
  ON SCHEDULE EVERY 1 SECOND DO INSERT INTO test.t1 VALUES (1, 2);
ALTER SCHEMA S READ ONLY=1;

--error ER_SCHEMA_READ_ONLY
ALTER EVENT S.ev DISABLE;

--error ER_SCHEMA_READ_ONLY
DROP EVENT S.ev;

--sleep 5

--echo # Reject executing events located in a read only schema because
--echo # last_executed timestamp must be updated.
--let $grep_file= $MYSQLD_LOG
let $grep_pattern= Event Scheduler: Unable to schedule event: Schema '.' is in
 read only mode.;
--let $grep_output= boolean
--source include/grep_pattern.inc

--let $grep_file= $MYSQLD_LOG
let $grep_pattern= Event Scheduler: Serious error during getting next event
 to execute. Stopping;
--let $grep_output= boolean
--source include/grep_pattern.inc

--echo # Reject executing events accessing read only entities.
--let $grep_file= $MYSQLD_LOG
let $grep_pattern= Event Scheduler: \[root@localhost\]\[test.ev\] Schema '.'
 is in read only mode.;
--let $grep_output= boolean
--source include/grep_pattern.inc

--let $grep_file= $MYSQLD_LOG
let $grep_pattern= Event Scheduler: \[root@localhost\].\[test.ev\] event
 execution failed.;
--let $grep_output= boolean
--source include/grep_pattern.inc

ALTER SCHEMA S READ ONLY=0;
ALTER EVENT S.ev DISABLE;
DROP EVENT S.ev;
DROP EVENT test.ev;
DROP TABLE test.t1;
ALTER SCHEMA S READ ONLY=1;

--echo # Non-cascading foreign keys.
CREATE TABLE test.p (
    id INT NOT NULL,
    PRIMARY KEY (id)
);
INSERT INTO test.p VALUES (1), (2), (3), (4);

ALTER SCHEMA S READ ONLY=0;
CREATE TABLE S.c (
    p_id INT,
    FOREIGN KEY (p_id)
        REFERENCES test.p(id)
);
INSERT INTO S.c VALUES (1), (2);
ALTER SCHEMA S READ ONLY=1;

--error ER_ROW_IS_REFERENCED_2
DELETE FROM test.p WHERE id=1;
DELETE FROM test.p WHERE id=4;

SELECT * FROM test.p;
SELECT * FROM S.c;

--echo # Parent of non-cascading FK may be locked.
LOCK TABLE test.p WRITE;
UNLOCK TABLES;

ALTER SCHEMA S READ ONLY=0;
DROP TABLE S.c;

--echo # Cascading foreign keys.
INSERT INTO test.p VALUES (4);
CREATE TABLE S.c (
    p_id INT,
    FOREIGN KEY (p_id)
        REFERENCES test.p(id)
        ON DELETE CASCADE
        ON UPDATE CASCADE
);
INSERT INTO S.c VALUES (1), (2);
ALTER SCHEMA S READ ONLY=1;

--echo # Prelocking will reject any parent update/delete + LOCK TABLE.
--error ER_SCHEMA_READ_ONLY
DELETE FROM test.p WHERE id=1;
--error ER_SCHEMA_READ_ONLY
DELETE FROM test.p WHERE id=4;
--error ER_SCHEMA_READ_ONLY
UPDATE test.p SET id=5 WHERE id=2;
--error ER_SCHEMA_READ_ONLY
UPDATE test.p SET id=5 WHERE id=4;
--error ER_SCHEMA_READ_ONLY
LOCK TABLE test.p WRITE;

--echo # Turning off FKC will allow changes and skip updating child, but still
--echo # reject LOCK TABLE.
SET @@session.foreign_key_checks=0;

DELETE FROM test.p WHERE id=1;
DELETE FROM test.p WHERE id=4;
UPDATE test.p SET id=5 WHERE id=2;
UPDATE test.p SET id=2 WHERE id=3;

SELECT * FROM test.p;
SELECT * FROM S.c;

--error ER_SCHEMA_READ_ONLY
LOCK TABLE test.p WRITE;

SET @@session.foreign_key_checks=1;

--echo # Turning off read only will allow changes and update child + allow
--echo # LOCK TABLE.
ALTER SCHEMA S READ ONLY=0;
UPDATE test.p SET id=6 WHERE id=2;

SELECT * FROM test.p;
SELECT * FROM S.c;

DELETE FROM test.p WHERE id=6;

SELECT * FROM test.p;
SELECT * FROM S.c;

LOCK TABLE test.p WRITE;
--echo # LOCK will block altering schema from same connection.
--error ER_LOCK_OR_ACTIVE_TRANSACTION
ALTER SCHEMA S READ ONLY=1;
UNLOCK TABLES;

ALTER SCHEMA S READ ONLY=1;

--echo # Trigger deleting/updating parent with a cascading FK child in
--echo # read only schema.
CREATE TABLE test.t(i INT);

CREATE TRIGGER test.ins_upd
  BEFORE INSERT ON test.t FOR EACH ROW
    UPDATE test.p SET id = id + 1 WHERE id = NEW.i;
CREATE TRIGGER test.ins_del
  AFTER INSERT ON test.t FOR EACH ROW DELETE FROM test.p WHERE id = NEW.i-1;

SELECT * FROM test.p;
SELECT * FROM S.c;

--error ER_SCHEMA_READ_ONLY
INSERT INTO test.t VALUES (4);
--error ER_SCHEMA_READ_ONLY
LOCK TABLE test.t WRITE;

ALTER SCHEMA S READ ONLY=0;
INSERT INTO test.t VALUES (4);

SELECT * FROM test.p;
SELECT * FROM S.c;

--echo # LOCK TABLE WRITE blocking ALTER SCHEMA from another connection.
LOCK TABLE test.t WRITE;

--connect (con_1, localhost, root,, test)
set @@session.lock_wait_timeout = 1;
--error ER_LOCK_WAIT_TIMEOUT
ALTER SCHEMA S READ ONLY=1;

--echo # LOCK TABLE READ will not block ALTER SCHEMA S from another connection,
--echo # but ALTER SCHEMA test will be blocked.
--connection default
UNLOCK TABLES;
LOCK TABLE test.t READ;

--connection con_1
ALTER SCHEMA S READ ONLY=1;
ALTER SCHEMA S READ ONLY=0;
--error ER_LOCK_WAIT_TIMEOUT
ALTER SCHEMA test READ ONLY=1;

--connection default
--disconnect con_1
UNLOCK TABLES;
set @@session.lock_wait_timeout=DEFAULT;

--echo # LOCK TABLES of table in read only schema.
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
LOCK TABLE S.c WRITE;

--echo # Restart with default log file.
--let $wait_counter= 10000
--let $restart_parameters= "restart:"
--source include/restart_mysqld.inc

--echo # Intermediate cleanup.
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;
DROP TABLE test.t;
DROP TABLE test.p;

--echo #--------------------------------------------------------------------
--echo # FR13: The option shall not have any affect on temporary tables.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
ALTER SCHEMA S READ ONLY=1;
CREATE TEMPORARY TABLE S.t(i INT);
INSERT INTO S.t VALUES(1);
SELECT * FROM S.t;
DROP TABLE S.t;
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR14: Concurrency.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
CREATE TABLE S.t(i INT);

connect (con1, localhost, root,,);
connect (con2, localhost, root,,);

--echo # From a separate connection, issue an INSERT, and halt after getting
--echo # lock.
connection con1;
SET DEBUG_SYNC =
  'after_open_table_mdl_shared SIGNAL got_lock WAIT_FOR continue';
--send INSERT INTO S.t VALUES (1);

--echo # From another connection, issue an ALTER SCHEMA.
connection con2;
SET DEBUG_SYNC = 'now WAIT_FOR got_lock';
--send ALTER SCHEMA S READ ONLY=1;

--echo # Verify lock state and let the INSERT continue.
connection default;

let $wait_condition = SELECT count(*) = 3 from
   performance_schema.metadata_locks where object_schema in ('s', 'S');
--source include/wait_condition.inc

SELECT object_schema, object_name, lock_type, lock_status
  FROM performance_schema.metadata_locks WHERE object_schema in ('s', 'S');
SET DEBUG_SYNC = 'now SIGNAL continue';

--echo # Verify that the statements complete. Note deadlock for the insert
--echo # due to acquiring IX on schema name.
connection con1;
--error ER_LOCK_DEADLOCK
--reap

connection con2;
--reap

connection default;
SHOW CREATE SCHEMA S;

--echo # Halt ALTER SCHEMA after locking schema tables.
connection con1;
SET DEBUG_SYNC =
  'after_wait_locked_tablespace_name_for_table SIGNAL got_lock
   WAIT_FOR continue';
--send ALTER SCHEMA S READ ONLY=0;

--echo # From a separate connection, issue an INSERT.
connection con2;
SET DEBUG_SYNC = 'now WAIT_FOR got_lock';
--send INSERT INTO S.t VALUES (1);

--echo # Verify lock state and let the ALTER continue.
connection default;

let $wait_condition = SELECT count(*) = 3
  from performance_schema.metadata_locks where object_schema in ('s', 'S');
--source include/wait_condition.inc

SELECT object_schema, object_name, lock_type, lock_status
  FROM performance_schema.metadata_locks WHERE object_schema in ('s', 'S');
SET DEBUG_SYNC = 'now SIGNAL continue';

--echo # Verify that the statements complete.
connection con1;
--reap

connection con2;
--reap

connection default;
SHOW CREATE SCHEMA S;
disconnect con1;
disconnect con2;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR15: Conflicting options.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
ALTER SCHEMA S READ ONLY = 0 READ ONLY = 0;
--error ER_CONFLICTING_DECLARATIONS
ALTER SCHEMA S READ ONLY = 0 READ ONLY = 1;
ALTER SCHEMA S READ ONLY = 0 READ ONLY = DEFAULT;

--error ER_CONFLICTING_DECLARATIONS
ALTER SCHEMA S READ ONLY = 1 READ ONLY = 0;
ALTER SCHEMA S READ ONLY = 1 READ ONLY = 1;
--error ER_CONFLICTING_DECLARATIONS
ALTER SCHEMA S READ ONLY = 1 READ ONLY = DEFAULT;

ALTER SCHEMA S READ ONLY = DEFAULT READ ONLY = 0;
--error ER_CONFLICTING_DECLARATIONS
ALTER SCHEMA S READ ONLY = DEFAULT READ ONLY = 1;
ALTER SCHEMA S READ ONLY = DEFAULT READ ONLY = DEFAULT;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR16: ALTER READ ONLY allowed for read only schema.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
ALTER SCHEMA S READ ONLY=1;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S READ ONLY=DEFAULT;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S READ ONLY=1;
--echo Repeated READ ONLY = 1 is allowed
ALTER SCHEMA S READ ONLY=1;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR17: ALTER SCHEMA rejected for read only schema.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
ALTER SCHEMA S READ ONLY=1;
SHOW CREATE SCHEMA S;
--error ER_SCHEMA_READ_ONLY
ALTER SCHEMA S COLLATE utf8_bin;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR18: ALTER SCHEMA for a mix of options.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
ALTER SCHEMA S READ ONLY = 1 COLLATE utf8_bin;
ALTER SCHEMA S READ ONLY = 0 COLLATE utf8_bin;
SHOW CREATE SCHEMA S;
ALTER SCHEMA S READ ONLY = 1 COLLATE utf8_general_ci;
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR19: A priori check for read only schema.
--echo #--------------------------------------------------------------------
eval CREATE SCHEMA $CREATE_S;
ALTER SCHEMA S READ ONLY=1;
--error ER_SCHEMA_READ_ONLY
CREATE TABLE S.t(i INT) ENGINE MyISAM;
--let $MYSQLD_DATADIR = `select @@datadir`
--list_files $MYSQLD_DATADIR/$CREATE_S
ALTER SCHEMA S READ ONLY=0;
DROP SCHEMA S;

--echo #--------------------------------------------------------------------
--echo # FR20: No impact from schema read only on cloning, but state kept.
--echo #--------------------------------------------------------------------
--echo # See extension of test clone.local_basic.

--echo #--------------------------------------------------------------------
--echo # Misc: Using lower_case_table_names.
--echo #--------------------------------------------------------------------
--echo # See separate test wrappers schema_read_only_{cs,ci}.

--echo #--------------------------------------------------------------------
--echo # Misc: Merge engine.
--echo #--------------------------------------------------------------------
USE test;
eval CREATE SCHEMA $CREATE_S;

CREATE TABLE t1(str varchar(10)) ENGINE MyISAM;
CREATE TABLE t2(str varchar(10)) ENGINE MyISAM;
CREATE TABLE S.t1(str varchar(10)) ENGINE MyISAM;

CREATE TABLE tt_last(str varchar(10))
  ENGINE MERGE UNION = (t1, S.t1), INSERT_METHOD=LAST;
CREATE TABLE tt_first(str varchar (10))
  ENGINE MERGE UNION = (t1, S.t1), INSERT_METHOD=FIRST;
CREATE TABLE S.tt(str varchar (10))
  ENGINE MERGE UNION = (t1, t2), INSERT_METHOD=LAST;

INSERT INTO tt_last VALUES ('last');
INSERT INTO tt_first VALUES ('first');
INSERT INTO S.tt VALUES ('s.tt');

SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM S.t1;
SELECT * FROM tt_last;
SELECT * FROM tt_first;
SELECT * FROM S.tt;

ALTER SCHEMA S READ ONLY=1;

--error ER_SCHEMA_READ_ONLY
INSERT INTO tt_last VALUES ('2 last');
--error ER_SCHEMA_READ_ONLY
INSERT INTO tt_first VALUES ('2 first');
--error ER_SCHEMA_READ_ONLY
INSERT INTO S.tt VALUES ('2 s.tt');

SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM S.t1;
SELECT * FROM tt_last;
SELECT * FROM tt_first;
SELECT * FROM S.tt;

--echo # Allowed to drop merge tables referring a read only schema.
DROP TABLE tt_first;
DROP TABLE tt_last;

ALTER SCHEMA S READ ONLY=0;

DROP TABLE t2;
DROP TABLE t1;
DROP SCHEMA S;
--disable_connect_log
