#########################################
# Author: Serge Kozlov skozlov@mysql.com
# Date: 07/10/2006
# Purpose: testing the replication in mixed mode
# Requirements: define binlog format for mysqld as in example below:
# ./mysql-test-run.pl --mysqld=--binlog-format=mixed
#########################################

--source include/master-slave.inc

# Check MIXED on both master and slave
connection master;
--echo ==========MASTER==========
--source suite/rpl/include/rpl_mixed_show_binlog_format.inc
connection slave;
--echo ==========SLAVE===========
--source suite/rpl/include/rpl_mixed_show_binlog_format.inc
connection master;


CREATE DATABASE test_rpl;

--echo
--echo ******************** PREPARE TESTING ********************
USE test_rpl;
eval CREATE TABLE t1 (a int auto_increment not null, b char(254), PRIMARY KEY(a)) ENGINE=$engine_type;
eval CREATE TABLE t2 (a int auto_increment not null, b char(254), PRIMARY KEY(a)) ENGINE=$engine_type;

# DELETE 
INSERT INTO t1 VALUES(1, 't1, text 1');
INSERT INTO t1 VALUES(2, 't1, text 2');
INSERT INTO t2 VALUES(1, 't2, text 1');
--echo
--echo ******************** DELETE ********************
DELETE FROM t1 WHERE a = 1;
DELETE FROM t2 WHERE b <> UUID();
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# INSERT
--echo
--echo ******************** INSERT ********************
INSERT INTO t1 VALUES(1, 't1, text 1');
INSERT INTO t1 VALUES(2, UUID());
INSERT INTO t2 SELECT * FROM t1;
INSERT INTO t2 VALUES (1, 't1, text 1') ON DUPLICATE KEY UPDATE b = 't2, text 1';
DELETE FROM t1 WHERE a = 2;
DELETE FROM t2 WHERE a = 2;
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

--echo
--echo ******************** LOAD DATA INFILE ********************
--copy_file ./suite/rpl/data/rpl_mixed.dat $MYSQLTEST_VARDIR/tmp/rpl_mixed.dat
LOAD DATA INFILE '../tmp/rpl_mixed.dat' INTO TABLE t1 FIELDS TERMINATED BY '|' ;
--remove_file $MYSQLTEST_VARDIR/tmp/rpl_mixed.dat
SELECT * FROM t1 ORDER BY a;
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# REPLACE
--echo
--echo ******************** REPLACE ********************
INSERT INTO t1 VALUES(1, 't1, text 1');
INSERT INTO t1 VALUES(2, 't1, text 2');
INSERT INTO t1 VALUES(3, 't1, text 3');
REPLACE INTO t1 VALUES(1, 't1, text 11');
REPLACE INTO t1 VALUES(2, UUID());
REPLACE INTO t1 SET a=3, b='t1, text 33';
DELETE FROM t1 WHERE a = 2;
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# SELECT
--echo
--echo ******************** SELECT ********************
INSERT INTO t1 VALUES(1, 't1, text 1');
SELECT * FROM t1 WHERE b <> UUID() ORDER BY a;
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# JOIN
--echo
--echo ******************** JOIN ********************
INSERT INTO t1 VALUES(1, 'CCC');
INSERT INTO t1 VALUES(2, 'DDD');
INSERT INTO t2 VALUES(1, 'DDD');
INSERT INTO t2 VALUES(2, 'CCC');
SELECT * FROM t1 LEFT JOIN  t2 ON t1.a = t2.a ORDER BY t1.a,t2.a;
SELECT * FROM t1 INNER JOIN  t2 ON t1.b = t2.b ORDER BY t1.a,t2.a;
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# UNION
--echo
--echo ******************** UNION ********************
INSERT INTO t1 VALUES(1, 't1, text 1');
INSERT INTO t2 VALUES(1, 't2, text 1');
SELECT * FROM t1 UNION SELECT * FROM t2 WHERE t2.b <> UUID();
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# TRUNCATE 
--echo
--echo ******************** TRUNCATE ********************
INSERT INTO t1 VALUES(1, 't1, text 1');
--source suite/rpl/include/rpl_mixed_check_select.inc
TRUNCATE t1;
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# UPDATE 
--echo
--echo ******************** UPDATE ********************
INSERT INTO t1 VALUES(1, 't1, text 1');
INSERT INTO t2 VALUES(1, 't2, text 1');
UPDATE t1 SET b = 't1, text 1 updated' WHERE a = 1;
--source suite/rpl/include/rpl_mixed_check_select.inc
UPDATE t1, t2 SET t1.b = 'test', t2.b = 'test';
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# DESCRIBE
--echo
--echo ******************** DESCRIBE ********************
DESCRIBE t1;
DESCRIBE t2 b;

# USE
--echo
--echo ******************** USE ********************
USE test_rpl;

# TRANSACTION 
--echo
--echo ******************** TRANSACTION ********************
START TRANSACTION;
INSERT INTO t1 VALUES (1, 'start');
COMMIT;
--source suite/rpl/include/rpl_mixed_check_select.inc
START TRANSACTION;
INSERT INTO t1 VALUES (2, 'rollback');
ROLLBACK;
--source suite/rpl/include/rpl_mixed_check_select.inc
START TRANSACTION;
INSERT INTO t1 VALUES (3, 'before savepoint s1');
SAVEPOINT s1;
INSERT INTO t1 VALUES (4, 'after savepoint s1');
ROLLBACK TO SAVEPOINT s1;
--source suite/rpl/include/rpl_mixed_check_select.inc
START TRANSACTION;
INSERT INTO t1 VALUES (5, 'before savepoint s2');
SAVEPOINT s2;
INSERT INTO t1 VALUES (6, 'after savepoint s2');
INSERT INTO t1 VALUES (7, CONCAT('with UUID() ',UUID()));
RELEASE SAVEPOINT s2;
COMMIT;
DELETE FROM t1 WHERE a = 7;
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# LOCK TABLES
--echo
--echo ******************** LOCK TABLES ********************
LOCK TABLES t1 READ , t2 READ;
UNLOCK TABLES;

# TRANSACTION ISOLATION LEVEL
--echo
--echo ******************** TRANSACTION ISOLATION LEVEL ********************
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ; 
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;

# XA
# skipped

# CREATE USER
--echo
--echo ******************** CREATE USER ********************
CREATE USER 'user_test_rpl'@'localhost' IDENTIFIED BY PASSWORD '*1111111111111111111111111111111111111111';
--source suite/rpl/include/rpl_mixed_check_user.inc

# GRANT 
--echo
--echo ******************** GRANT ********************
GRANT SELECT ON *.* TO 'user_test_rpl'@'localhost';
--source suite/rpl/include/rpl_mixed_check_user.inc

# REVOKE
--echo
--echo ******************** REVOKE ********************
REVOKE SELECT ON *.* FROM 'user_test_rpl'@'localhost';
--source suite/rpl/include/rpl_mixed_check_user.inc

# SET PASSWORD
--echo
--echo ******************** SET PASSWORD ********************
SET PASSWORD FOR 'user_test_rpl'@'localhost' = '*0000000000000000000000000000000000000000';
--source suite/rpl/include/rpl_mixed_check_user.inc

# RENAME USER
--echo
--echo ******************** RENAME USER ********************
RENAME USER 'user_test_rpl'@'localhost' TO 'user_test_rpl_2'@'localhost';
--source suite/rpl/include/rpl_mixed_check_user.inc

# DROP USER
--echo
--echo ******************** DROP USER ********************
DROP USER 'user_test_rpl_2'@'localhost';
--source suite/rpl/include/rpl_mixed_check_user.inc

# Prepring for some following operations
INSERT INTO t1 VALUES(100, 'test');

# ANALYZE
--echo
--echo ******************** ANALYZE ********************
ANALYZE TABLE t1;

# BACKUP TABLE
# skipped because deprecated

# CHECK TABLE
--echo
--echo ******************** CHECK TABLE ********************
CHECK TABLE t1;

# CHECKSUM TABLE
--echo
--echo ******************** CHECKSUM TABLE ********************
CHECKSUM TABLE t1;

# OPTIMIZE TABLE
--echo
--echo ******************** OPTIMIZE TABLE ********************
OPTIMIZE TABLE t1;

# REPAIR TABLE
--echo
--echo ******************** REPAIR TABLE ********************
REPAIR TABLE t1;

# SET VARIABLE
--echo
--echo ******************** SET VARIABLE ********************
SET @test_rpl_var = 1;
SHOW VARIABLES LIKE 'test_rpl_var';

# SHOW
--echo
--echo ******************** SHOW ********************
--source suite/rpl/include/rpl_mixed_check_db.inc


# PROCEDURE
--echo
--echo ******************** PROCEDURE ********************
DELIMITER |;
CREATE PROCEDURE p1 ()
BEGIN
  UPDATE t1 SET b = 'test' WHERE a = 201;
END|
CREATE PROCEDURE p2 ()
BEGIN
  UPDATE t1 SET b = UUID() WHERE a = 202;
END|
DELIMITER ;|
INSERT  INTO t1 VALUES(201, 'test 201');
CALL p1();
INSERT  INTO t1 VALUES(202, 'test 202');
CALL p2();
DELETE FROM t1 WHERE a = 202;
--source suite/rpl/include/rpl_mixed_check_select.inc
ALTER PROCEDURE p1 COMMENT 'p1';
DROP PROCEDURE p1;
DROP PROCEDURE p2;
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# TRIGGER
--echo
--echo ******************** TRIGGER ********************
DELIMITER |;
CREATE TRIGGER tr1 BEFORE INSERT ON t1
FOR EACH ROW BEGIN
  INSERT INTO t2 SET a = NEW.a, b = NEW.b;
END|
DELIMITER ;|
INSERT INTO t1 VALUES (1, 'test');
--source suite/rpl/include/rpl_mixed_check_select.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc
DROP TRIGGER tr1;

# EVENTS
--echo
--echo
--echo ******************** EVENTS ********************
GRANT EVENT ON *.* TO 'root'@'localhost';
INSERT INTO t1 VALUES(1, 'test1');
CREATE EVENT e1 ON SCHEDULE EVERY '1' SECOND COMMENT 'e_second_comment' DO DELETE FROM t1;
--source suite/rpl/include/rpl_mixed_check_event.inc
--source suite/rpl/include/rpl_mixed_check_select.inc
--sleep 2
--source suite/rpl/include/rpl_mixed_check_select.inc
ALTER EVENT e1 RENAME TO e2;
--sleep 2
--source suite/rpl/include/rpl_mixed_check_event.inc
--source suite/rpl/include/rpl_mixed_check_select.inc
DROP EVENT e2;
--source suite/rpl/include/rpl_mixed_check_event.inc
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# VIEWS
--echo
--echo ******************** VIEWS ********************
INSERT INTO t1 VALUES(1, 'test1');
INSERT INTO t1 VALUES(2, 'test2');
CREATE VIEW v1 AS SELECT * FROM t1 WHERE a = 1;
CREATE VIEW v2 AS SELECT * FROM t1 WHERE b <> UUID();
--source suite/rpl/include/rpl_mixed_check_view.inc
ALTER VIEW v1 AS SELECT * FROM t1 WHERE a = 2;
--source suite/rpl/include/rpl_mixed_check_view.inc
DROP VIEW v1;
DROP VIEW v2;
--source suite/rpl/include/rpl_mixed_clear_tables.inc

# BINLOG EVENTS
--echo
--echo
--echo ******************** SHOW BINLOG EVENTS ********************
--replace_column 2 # 5 #
--replace_regex /Server ver: .+/Server ver: #/ /table_id: [0-9]+/table_id: #/ /COMMIT.+xid=[0-9]+.+/#/ /file_id=[0-9]+/file_id=#/ /block_len=[0-9]+/block_len=#/
show binlog events from 1;
sync_slave_with_master;
# as we're using UUID we don't SELECT but use "diff" like in rpl_row_UUID
--exec $MYSQL_DUMP --compact --order-by-primary --skip-extended-insert --no-create-info test_rpl > $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_master.sql
--exec $MYSQL_DUMP_SLAVE --compact --order-by-primary --skip-extended-insert --no-create-info test_rpl > $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_slave.sql

connection master;
drop database test_rpl;
sync_slave_with_master;

# Let's compare. Note: If they match test will pass, if they do not match
# the test will show that the diff statement failed and not reject file
# will be created. You will need to go to the mysql-test dir and diff
# the files your self to see what is not matching

--exec diff $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_master.sql $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_slave.sql
