###############################################################################
#                                                                             #
#  This script executes partial_revokes scenarios in following modes :        #
#  1. Normal        :   Script executes on single server                      #
#  2. Replication   :   Script executes in replication setup                  #
#                       on a master and a slave                               #
#                                                                             #
#   Arguments required:                                                       #
#   1. $TEST_MODE - [NORMAL | RPL]                                            #
#                                                                             #
###############################################################################

# If required parameters are not set then throw error and exit.
--let PARAM_PERL_QUIT=0
if ($TEST_MODE == '')
{
--let VARIABLE_NAME = TEST_MODE
--let PARAM_PERL_QUIT=1
}

--perl
    $variable_name = "$ENV{VARIABLE_NAME}";
    die "Error: partial_revokes_dml.inc: input variable $variable_name is missing!"
             if ($ENV{PARAM_PERL_QUIT} == 1);
EOF

--echo # -----------------------------------------------------------------------
--echo # Start : DML Tests for Partial Revokes
--echo # -----------------------------------------------------------------------
--echo # -----------------------------------------------------------------------
--echo # Setup
--echo # Create database and objects
CREATE DATABASE partial_revokes_db;
CREATE DATABASE partial_revokes_db_2;
--echo # Create users
CREATE USER foo@localhost IDENTIFIED BY 'abcd';
CREATE USER bar@localhost IDENTIFIED BY 'efgh';

# Format : Case: <type of grant> + Partial revoke info + Operation

--echo # -----------------------------------------------------------------------
--echo # Start: Test cases for Base tables.
CREATE TABLE partial_revokes_db.t1(c1 int, c2 int);
INSERT INTO partial_revokes_db.t1 values (1,2), (3,4);
CREATE TABLE partial_revokes_db_2.t1(a1 int, a2 int);
INSERT INTO partial_revokes_db_2.t1 values (1,2), (5,6), (9,10), (11,12);
CREATE TABLE partial_revokes_db.t2 (a date, b date, c date not null, d date);
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on INSERT + INSERT

# setup
--connection default
GRANT INSERT, SELECT ON *.* TO foo@localhost;
REVOKE INSERT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# Insert should block
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db.t1 VALUES (5,6);
# Select should not
SELECT COUNT(*) FROM partial_revokes_db.t1;

# cleanup
--connection default
--disconnect foo_conn
REVOKE INSERT, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on SELECT + INSERT ... SELECT

# setup
--connection default
GRANT INSERT, SELECT ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# INSERT ... SELECT should not work
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db_2.t1 SELECT * FROM partial_revokes_db.t1;
# Regular INSERT should succeed
INSERT INTO partial_revokes_db_2.t1 VALUES (1,2), (3,4);
# Even on the other table
INSERT INTO partial_revokes_db.t1 VALUES (5,6), (7,8);

# cleanup
--connection default
--disconnect foo_conn
REVOKE INSERT, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on UPDATE + UPDATE

# setup
--connection default
GRANT UPDATE, SELECT ON *.* TO foo@localhost;
REVOKE UPDATE ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# Simple UPDATE should work
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE partial_revokes_db.t1 SET c1 = c1 + 1;
# UPDATE with SELECT should too
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE partial_revokes_db.t1 SET c2 = c2 + 1 WHERE c1 = 2;

# cleanup
--connection default
--disconnect foo_conn
REVOKE UPDATE, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on SELECT + UPDATE ... SELECT

# setup
--connection default
GRANT UPDATE, SELECT ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# Simple UPDATE should work
UPDATE partial_revokes_db.t1 SET c1 = 1;
# UPDATE with SELECT should not
--error ER_COLUMNACCESS_DENIED_ERROR
UPDATE partial_revokes_db.t1 SET c1 = c1 + 1;
# UPDATE with SELECT should not
--error ER_COLUMNACCESS_DENIED_ERROR
UPDATE partial_revokes_db.t1 SET c2 = c2 + 1 WHERE c1 = 2;

# cleanup
--connection default
--disconnect foo_conn
REVOKE UPDATE, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on DELETE + DELETE

# setup
--connection default
GRANT DELETE, SELECT ON *.* TO foo@localhost;
REVOKE DELETE ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# DELETE with SELECT should not work
--error ER_TABLEACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.t1 WHERE c1 = 1;
# Simple DELETE should too
--error ER_TABLEACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.t1;

# cleanup
--connection default
--disconnect foo_conn
REVOKE DELETE, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on SELECT + DELETE ... SELECT

# setup
--connection default
GRANT DELETE, SELECT ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# DELETE with SELECT should not work
--error ER_COLUMNACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.t1 WHERE c1 = 1;
# Simple DELETE should
DELETE FROM partial_revokes_db.t1;

# cleanup
--connection default
--disconnect foo_conn
REVOKE DELETE, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on DELETE + REPLACE

# setup
--connection default
GRANT SELECT, INSERT, DELETE ON *.* TO foo@localhost;
REVOKE DELETE ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# REPLACE requires INSERT + DELETE
--error ER_TABLEACCESS_DENIED_ERROR
REPLACE INTO partial_revokes_db.t1 VALUES (1,3);
--error ER_TABLEACCESS_DENIED_ERROR
REPLACE INTO partial_revokes_db.t1 VALUES (5,6);
# Following should work
REPLACE INTO partial_revokes_db_2.t1 VALUES (1,3);
if($TEST_MODE != RPL)
{
# Avoid warning in server log in case of BINLOG_FORMAT= statement
REPLACE INTO partial_revokes_db_2.t1 SELECT * FROM partial_revokes_db.t1 ORDER BY c1;
}

# cleanup
--connection default
--disconnect foo_conn
REVOKE SELECT, INSERT, DELETE ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on INSERT + REPLACE

# setup
--connection default
GRANT SELECT, INSERT, DELETE ON *.* TO foo@localhost;
REVOKE INSERT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# REPLACE requires INSERT + DELETE
--error ER_TABLEACCESS_DENIED_ERROR
REPLACE INTO partial_revokes_db.t1 VALUES (1,3);
--error ER_TABLEACCESS_DENIED_ERROR
REPLACE INTO partial_revokes_db.t1 VALUES (5,6);
# Following should work
REPLACE INTO partial_revokes_db_2.t1 VALUES (1,3);
if($TEST_MODE != RPL)
{
REPLACE INTO partial_revokes_db_2.t1 SELECT * FROM partial_revokes_db.t1 ORDER BY c1;
}
# cleanup
--connection default
--disconnect foo_conn
REVOKE SELECT, INSERT, DELETE ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on INSERT + LOAD

# setup
--connection default
GRANT INSERT, SELECT, FILE ON *.* TO foo@localhost;
REVOKE INSERT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# Should fail
--error ER_TABLEACCESS_DENIED_ERROR
LOAD DATA INFILE '../../std_data/loaddata1.dat' IGNORE INTO TABLE partial_revokes_db.t2 FIELDS TERMINATED BY ',';
SELECT * FROM partial_revokes_db.t2;

# cleanup
--connection default
--disconnect foo_conn
REVOKE INSERT, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on INSERT + CTE

# setup
--connection default
GRANT SELECT ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
--error ER_TABLEACCESS_DENIED_ERROR
WITH cte1 AS (SELECT c1, c2 FROM partial_revokes_db.t1),
     cte2 AS (SELECT a1, a2 FROM partial_revokes_db_2.t1)
SELECT c2, a2 FROM cte1 JOIN cte2 WHERE cte1.c1 = cte2.a1;

# cleanup
--connection default
--disconnect foo_conn
REVOKE SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # End: Test cases for Base tables.
# cleanup
--connection default
DROP TABLE partial_revokes_db.t1;
DROP TABLE partial_revokes_db.t2;
DROP TABLE partial_revokes_db_2.t1;
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Start: Test cases for views.
CREATE TABLE partial_revokes_db.t1(c1 int, c2 int);
INSERT INTO partial_revokes_db.t1 values (1,2), (3,4), (5,6), (7,8);
CREATE TABLE partial_revokes_db_2.t2(a1 int, a2 int);
INSERT INTO partial_revokes_db_2.t2 values (1,2), (5,6), (9,10), (11,12);
CREATE SQL SECURITY DEFINER VIEW partial_revokes_db.v1t1d AS SELECT * FROM partial_revokes_db.t1;
CREATE SQL SECURITY INVOKER VIEW partial_revokes_db.v1t1i AS SELECT * FROM partial_revokes_db.t1;
CREATE SQL SECURITY DEFINER VIEW partial_revokes_db.v1t2d AS SELECT * FROM partial_revokes_db_2.t2;
CREATE SQL SECURITY INVOKER VIEW partial_revokes_db.v1t2i AS SELECT * FROM partial_revokes_db_2.t2;

CREATE SQL SECURITY DEFINER VIEW partial_revokes_db_2.v1t1d AS SELECT * FROM partial_revokes_db.t1;
CREATE SQL SECURITY INVOKER VIEW partial_revokes_db_2.v1t1i AS SELECT * FROM partial_revokes_db.t1;
CREATE SQL SECURITY DEFINER VIEW partial_revokes_db_2.v1t2d AS SELECT * FROM partial_revokes_db_2.t2;
CREATE SQL SECURITY INVOKER VIEW partial_revokes_db_2.v1t2i AS SELECT * FROM partial_revokes_db_2.t2;
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on SELECT + SELECT, SHOW VIEW

#setup
--connection default
GRANT SELECT, SHOW VIEW ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# should fail : View is in partial_revokes_db
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM partial_revokes_db.v1t1d;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM partial_revokes_db.v1t1i;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM partial_revokes_db.v1t2d;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM partial_revokes_db.v1t2i;

# should work : Base table is in partial_revokes_db but this is a DEFINER view
SELECT * FROM partial_revokes_db_2.v1t1d;
# should fail : Base table is in partial_revokes_db and this is an INVOKER view
--error ER_VIEW_INVALID
SELECT * FROM partial_revokes_db_2.v1t1i;

# should work : view and base table are in partial_revokes_db_2
SELECT * FROM partial_revokes_db_2.v1t2d;
SELECT * FROM partial_revokes_db_2.v1t2i;

# Show create view
--error ER_TABLEACCESS_DENIED_ERROR
SHOW CREATE VIEW partial_revokes_db.v1t1d;
--error ER_TABLEACCESS_DENIED_ERROR
SHOW CREATE VIEW partial_revokes_db.v1t1i;
--error ER_TABLEACCESS_DENIED_ERROR
SHOW CREATE VIEW partial_revokes_db.v1t2d;
--error ER_TABLEACCESS_DENIED_ERROR
SHOW CREATE VIEW partial_revokes_db.v1t2i;

SHOW CREATE VIEW partial_revokes_db_2.v1t1d;
SHOW CREATE VIEW partial_revokes_db_2.v1t1i;
SHOW CREATE VIEW partial_revokes_db_2.v1t2d;
SHOW CREATE VIEW partial_revokes_db_2.v1t2i;

# cleanup
--connection default
--disconnect foo_conn
REVOKE SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on INSERT + INSERT

#setup
--connection default
GRANT INSERT ON *.* TO foo@localhost;
REVOKE INSERT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# should fail : View is in partial_revokes_db
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db.v1t1d VALUES (1,2);
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db.v1t1i VALUES (1,2);
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db.v1t2i VALUES (1,2);
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db.v1t2d VALUES (1,2);

# should work : Base table is in partial_revokes_db but this is a DEFINER view
INSERT INTO partial_revokes_db_2.v1t1d VALUES (1,2);
# should fail : Base table is in partial_revokes_db and this is an INVOKER view
--error ER_VIEW_INVALID
INSERT INTO partial_revokes_db_2.v1t1i VALUES (1,2);

# should work : view and base table are in partial_revokes_db_2
INSERT INTO partial_revokes_db_2.v1t2d VALUES(1,2);
INSERT INTO partial_revokes_db_2.v1t2i VALUES(1,2);

# cleanup
--connection default
--disconnect foo_conn
REVOKE INSERT ON *.* FROM foo@localhost;
DELETE FROM partial_revokes_db.t1;
DELETE FROM partial_revokes_db_2.t2;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on INSERT + INSERT ... SELECT

#setup
--connection default
INSERT INTO partial_revokes_db.t1 values (1,2), (3,4), (5,6), (7,8);
GRANT INSERT, SELECT ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)

# Following should fail because we are trying to select from a view in partial_revokes_db
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db_2.v1t2d SELECT * FROM partial_revokes_db.v1t1d;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db_2.v1t2d SELECT * FROM partial_revokes_db.v1t1i;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db_2.v1t2d SELECT * FROM partial_revokes_db.v1t2d;
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db_2.v1t2d SELECT * FROM partial_revokes_db.v1t2i;

# Following should fail because view is INVOKER view and base table is in partial_revokes_db
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO partial_revokes_db_2.v1t2i SELECT * FROM partial_revokes_db.v1t1i;

# All of the following should succeed
INSERT INTO partial_revokes_db_2.v1t2i SELECT * FROM partial_revokes_db_2.v1t1d;
INSERT INTO partial_revokes_db_2.v1t2i SELECT * FROM partial_revokes_db_2.v1t2d;
INSERT INTO partial_revokes_db_2.v1t2i SELECT * FROM partial_revokes_db_2.v1t2i;

# cleanup
--connection default
--disconnect foo_conn
DELETE FROM partial_revokes_db.t1;
DELETE FROM partial_revokes_db_2.t2;
REVOKE INSERT, SELECT ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on DELETE + DELETE

#setup
--connection default
INSERT INTO partial_revokes_db.t1 values (1,2), (3,4), (5,6), (7,8);
INSERT INTO partial_revokes_db_2.t2 values (1,2), (3,4), (5,6), (7,8);
GRANT DELETE ON *.* TO foo@localhost;
REVOKE DELETE ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# should fail : View is in partial_revokes_db
--error ER_TABLEACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t1d;
--error ER_TABLEACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t1i;
--error ER_TABLEACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t2d;
--error ER_TABLEACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t2i;

# should work : Base table is in partial_revokes_db but this is a DEFINER view
DELETE FROM partial_revokes_db_2.v1t1d;
# should fail : Base table is in partial_revokes_db and this is an INVOKER view
--error ER_VIEW_INVALID
DELETE FROM partial_revokes_db_2.v1t1i;

# should work : view and base table are in partial_revokes_db_2
DELETE FROM partial_revokes_db_2.v1t2d;
DELETE FROM partial_revokes_db_2.v1t2i;

# cleanup
--connection default
--disconnect foo_conn
DELETE FROM partial_revokes_db.t1;
DELETE FROM partial_revokes_db_2.t2;
REVOKE DELETE ON *.* FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on SELECT + DELETE ... SELECT

#setup
--connection default
INSERT INTO partial_revokes_db.t1 values (1,2), (3,4), (5,6), (7,8);
INSERT INTO partial_revokes_db_2.t2 values (1,2), (3,4), (5,6), (7,8);
GRANT DELETE, SELECT ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
# should fail : View is in partial_revokes_db
--error ER_COLUMNACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t1d WHERE c1 = 1;
--error ER_COLUMNACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t1i WHERE c1 = 1;
--error ER_COLUMNACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t2d WHERE a1 = 1;
--error ER_COLUMNACCESS_DENIED_ERROR
DELETE FROM partial_revokes_db.v1t2i WHERE a1 = 1;

# should work : Base table is in partial_revokes_db but this is a DEFINER view
DELETE FROM partial_revokes_db_2.v1t1d;
# should fail : Base table is in partial_revokes_db and this is an INVOKER view
--error ER_VIEW_INVALID
DELETE FROM partial_revokes_db_2.v1t1i WHERE c1 = 1;

# should work : view and base table are in partial_revokes_db_2
DELETE FROM partial_revokes_db_2.v1t2d WHERE a1 = 1;
DELETE FROM partial_revokes_db_2.v1t2i WHERE a1 = 1;

# cleanup
--connection default
--disconnect foo_conn
DELETE FROM partial_revokes_db.t1;
DELETE FROM partial_revokes_db_2.t2;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM foo@localhost;

--echo # -----------------------------------------------------------------------
--echo # End: Test cases for views.
# cleanup
--connection default
DROP TABLE IF EXISTS partial_revokes_db.t1;
DROP TABLE IF EXISTS partial_revokes_db_2.t1;
DROP VIEW IF EXISTS partial_revokes_db.v1t1d;
DROP VIEW IF EXISTS partial_revokes_db.v1t1i;
DROP VIEW IF EXISTS partial_revokes_db.v1t2d;
DROP VIEW IF EXISTS partial_revokes_db.v1t2i;
DROP VIEW IF EXISTS partial_revokes_db_2.v1t1d;
DROP VIEW IF EXISTS partial_revokes_db_2.v1t1i;
DROP VIEW IF EXISTS partial_revokes_db_2.v1t2d;
DROP VIEW IF EXISTS partial_revokes_db_2.v1t2i;
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke + SHOW DATABASE
CREATE USER baz@localhost IDENTIFIED BY 'abcd';
CREATE ROLE r3;
GRANT INSERT,SELECT ON *.* TO baz@localhost;
GRANT INSERT,SELECT ON *.* TO r3;
REVOKE INSERT,SELECT ON partial_revokes_db.* FROM baz@localhost;
REVOKE INSERT,SELECT ON partial_revokes_db_2.* FROM r3;
GRANT r3 TO baz@localhost;
--connect(baz_conn, localhost, baz, abcd,,,,)
# partial_revokes_db should not be visible.
SHOW GRANTS;
SHOW DATABASES;
SET ROLE r3;
# All dbs should be visible now.
SHOW DATABASES;
--connection default
--disconnect baz_conn
# partial_revokes_db should be visible now since it has at least one
# global privilege without any restriction on that.
GRANT INSERT ON `partial_revokes_db`.* TO baz@localhost;
--connect(baz_conn, localhost, baz, abcd,,,,)
SHOW GRANTS;
SHOW DATABASES;
SET ROLE r3;
# partial_revokes_db_2 should not visible now.
SHOW DATABASES;
--connection default
--disconnect baz_conn
GRANT SELECT ON `partial_revokes_db`.* TO baz@localhost;
--connect(baz_conn, localhost, baz, abcd,,,,)
SHOW GRANTS;
# partial_revokes_db should be visible of course
SHOW DATABASES;
SET ROLE r3;
# All dbs should be visible now.
SHOW DATABASES;
--echo # -----------------------------------------------------------------------

#cleanup
--connection default
--disconnect baz_conn
DROP USER baz@localhost;
DROP ROLE r3;
--echo # End: Test cases for Show database.
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Start: Test cases for stored routines.
--echo # -----------------------------------------------------------------------
CREATE TABLE partial_revokes_db.tp1(c1 int, c2 int);
INSERT INTO partial_revokes_db.tp1 VALUES (1,2), (3,4);
CREATE TABLE partial_revokes_db_2.tp2(c1 int, c2 int);
INSERT INTO partial_revokes_db_2.tp2 VALUES (5,6), (7,8);

CREATE PROCEDURE partial_revokes_db.p1tp1d_count() SELECT COUNT(*) FROM partial_revokes_db.tp1;
CREATE PROCEDURE partial_revokes_db.p1tp1i_count() SELECT COUNT(*) FROM partial_revokes_db.tp1;
ALTER PROCEDURE partial_revokes_db.p1tp1i_count SQL SECURITY INVOKER;
CREATE PROCEDURE partial_revokes_db.p1tp2d_count() SELECT COUNT(*) FROM partial_revokes_db_2.tp2;
CREATE PROCEDURE partial_revokes_db.p1tp2i_count() SELECT COUNT(*) FROM partial_revokes_db_2.tp2;
ALTER PROCEDURE partial_revokes_db.p1tp2i_count SQL SECURITY INVOKER;

CREATE PROCEDURE partial_revokes_db_2.p2tp1d_count() SELECT COUNT(*) FROM partial_revokes_db.tp1;
CREATE PROCEDURE partial_revokes_db_2.p2tp1i_count() SELECT COUNT(*) FROM partial_revokes_db.tp1;
ALTER PROCEDURE partial_revokes_db_2.p2tp1i_count SQL SECURITY INVOKER;
CREATE PROCEDURE partial_revokes_db_2.p2tp2d_count() SELECT COUNT(*) FROM partial_revokes_db_2.tp2;
CREATE PROCEDURE partial_revokes_db_2.p2tp2i_count() SELECT COUNT(*) FROM partial_revokes_db_2.tp2;
ALTER PROCEDURE partial_revokes_db_2.p2tp2i_count SQL SECURITY INVOKER;
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Case: *.* + Partial Revoke on EXECUTE

#setup
--connection default
GRANT EXECUTE, SELECT ON *.* TO foo@localhost;
REVOKE EXECUTE, SELECT ON partial_revokes_db.* FROM foo@localhost;
SHOW GRANTS FOR foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)
--error ER_PROCACCESS_DENIED_ERROR
CALL partial_revokes_db.p1tp1d_count();
--error ER_PROCACCESS_DENIED_ERROR
CALL partial_revokes_db.p1tp1i_count();
--error ER_PROCACCESS_DENIED_ERROR
CALL partial_revokes_db.p1tp2d_count();
--error ER_PROCACCESS_DENIED_ERROR
CALL partial_revokes_db.p1tp2i_count();

CALL partial_revokes_db_2.p2tp1d_count();
--error ER_TABLEACCESS_DENIED_ERROR
CALL partial_revokes_db_2.p2tp1i_count();
CALL partial_revokes_db_2.p2tp2d_count();
CALL partial_revokes_db_2.p2tp2i_count();

--connection default
--disconnect foo_conn
REVOKE EXECUTE, SELECT ON *.* FROM foo@localhost;
--echo # -----------------------------------------------------------------------
#cleanup
--connection default
DROP PROCEDURE partial_revokes_db.p1tp1d_count;
DROP PROCEDURE partial_revokes_db.p1tp1i_count;
DROP PROCEDURE partial_revokes_db.p1tp2d_count;
DROP PROCEDURE partial_revokes_db.p1tp2i_count;
DROP PROCEDURE partial_revokes_db_2.p2tp1d_count;
DROP PROCEDURE partial_revokes_db_2.p2tp1i_count;
DROP PROCEDURE partial_revokes_db_2.p2tp2d_count;
DROP PROCEDURE partial_revokes_db_2.p2tp2i_count;
DROP TABLE partial_revokes_db.tp1;
DROP TABLE partial_revokes_db_2.tp2;
--echo # End: Test cases for stored routines.
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Start: Test case for table and column grants
--echo # -----------------------------------------------------------------------
--connection default
REVOKE ALL PRIVILEGES, GRANT OPTION FROM foo@localhost;
SHOW GRANTS FOR foo@localhost;

CREATE TABLE partial_revokes_db.t1(c1 int);
INSERT INTO partial_revokes_db.t1 VALUES (1), (2), (3);
CREATE SQL SECURITY INVOKER VIEW partial_revokes_db_2.v1t1i AS SELECT * FROM partial_revokes_db.t1;
CREATE PROCEDURE partial_revokes_db_2.p1t1i_c1() SELECT COUNT(*) FROM partial_revokes_db.t1;
ALTER PROCEDURE partial_revokes_db_2.p1t1i_c1 SQL SECURITY INVOKER;

CREATE TABLE partial_revokes_db.t2(c1 int, c2 int);
INSERT INTO partial_revokes_db.t2 VALUES (1,2), (3,4), (5,6);
CREATE SQL SECURITY INVOKER VIEW partial_revokes_db_2.v2t2i AS SELECT c1 FROM partial_revokes_db.t2;
CREATE PROCEDURE partial_revokes_db_2.p2t2i_c1() SELECT c1 FROM partial_revokes_db.t2;
ALTER PROCEDURE partial_revokes_db_2.p2t2i_c1 SQL SECURITY INVOKER;

GRANT SELECT, EXECUTE ON *.* TO foo@localhost;
REVOKE SELECT ON partial_revokes_db.* FROM foo@localhost;
GRANT SELECT ON partial_revokes_db.t1 TO foo@localhost;
GRANT SELECT(c1) ON partial_revokes_db.t2 TO foo@localhost;
SHOW GRANTS FOR foo@localhost;

--connect(foo_conn, localhost, foo, abcd,,,,)

SELECT * FROM partial_revokes_db.t1;
SELECT * FROM partial_revokes_db_2.v1t1i;
CALL partial_revokes_db_2.p1t1i_c1();

SELECT c1 FROM partial_revokes_db.t2;
SELECT * FROM partial_revokes_db_2.v2t2i;
CALL partial_revokes_db_2.p2t2i_c1();

--echo # -----------------------------------------------------------------------
# Cleanup
--connection default
--disconnect foo_conn
REVOKE ALL PRIVILEGES, GRANT OPTION FROM foo@localhost;
DROP TABLE partial_revokes_db.t1;
DROP VIEW partial_revokes_db_2.v1t1i;
DROP PROCEDURE partial_revokes_db_2.p1t1i_c1;
DROP TABLE partial_revokes_db.t2;
DROP VIEW partial_revokes_db_2.v2t2i;
DROP PROCEDURE partial_revokes_db_2.p2t2i_c1;

--echo # End: Test cases for table and column grants
--echo # -----------------------------------------------------------------------

--echo # -----------------------------------------------------------------------
--echo # Cleanup
--connection default
--echo # Drop database
DROP DATABASE partial_revokes_db;
DROP DATABASE partial_revokes_db_2;
--echo # Drop users
DROP USER foo@localhost, bar@localhost;
--echo # -----------------------------------------------------------------------
--echo # -----------------------------------------------------------------------
--echo # End : DML Tests for Partial Revokes
--echo # -----------------------------------------------------------------------
