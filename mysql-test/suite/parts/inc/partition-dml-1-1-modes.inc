let $DATA_PATH=;
let $INDEX_PATH=;

--echo # Additional tests for WL#5217 by QA, testplan 1.1

let $TABLENAME= t1;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc

--enable_connect_log

CREATE USER test_user_1 IDENTIFIED BY 'testpw';
CREATE USER test_user_2 IDENTIFIED BY 'testpw';
GRANT ALL PRIVILEGES ON test.* TO test_user_1 ;
GRANT ALL PRIVILEGES ON test.* TO test_user_2 ;

--echo # Run in empty mode
set @@global.sql_mode= '';
--echo #######################################################
--echo # UPDATE without partition selection

connect (session1, localhost, test_user_1,'testpw',test);
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--echo send
send
UPDATE t1 SET b='table-upd-1' WHERE a BETWEEN 0 AND 9;

connect (session2, localhost, test_user_2,'testpw',test);
let $wait_condition= SELECT count(*)= 4 FROM t1 WHERE b ='table-upd-1';
--source include/wait_condition.inc
UPDATE t1 SET b='table-upd-2' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
INSERT INTO t1 VALUES (11, 'table-ins');
INSERT INTO t1 VALUES (12, 'table-ins');
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
DELETE FROM t1 WHERE b= 'table-ins';
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
--echo reap
reap;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

--echo #######################################################
--echo # UPDATE with partition selection
#connect (session1, localhost, test_user_1,'testpw',test);
connection session1;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--echo send
send
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-1' WHERE a BETWEEN 0 AND 9;

#connect (session2, localhost, test_user_2,'testpw',test);
connection session2;
let $wait_condition= SELECT count(*)= 4 FROM t1 WHERE b ='p0-29-upd-1';
--source include/wait_condition.inc
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-2' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
INSERT INTO t1 PARTITION (`p0-29`, subp4) VALUES (11, '(p0-29-)ins');
INSERT INTO t1 PARTITION (`p0-29`, subp4) VALUES (12, '(p0-29-)ins');
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
DELETE FROM t1 PARTITION (`p0-29`,subp5) WHERE b= '(p0-29-)ins';
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
--echo reap
reap;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

disconnect session1;
disconnect session2;

connection default;
DROP TABLE t1;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc
--echo # Run in ANSI mode
set @@global.sql_mode= ansi;
--echo #######################################################
--echo # UPDATE without partition selection

connect (session1, localhost, test_user_1,'testpw',test);
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--echo send
send
UPDATE t1 SET b='table-upd-1' WHERE a BETWEEN 0 AND 9;

connect (session2, localhost, test_user_2,'testpw',test);
let $wait_condition= SELECT count(*)= 4 FROM t1 WHERE b ='table-upd-1';
--source include/wait_condition.inc
UPDATE t1 SET b='table-upd-2' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
INSERT INTO t1 VALUES (11, 'table-ins');
INSERT INTO t1 VALUES (12, 'table-ins');
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
DELETE FROM t1 WHERE b= 'table-ins';
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
--echo reap
reap;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

--echo #######################################################
--echo # UPDATE with partition selection
#connect (session1, localhost, test_user_1,'testpw',test);
connection session1;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--echo send
send
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-1' WHERE a BETWEEN 0 AND 9;

#connect (session2, localhost, test_user_2,'testpw',test);
connection session2;
let $wait_condition= SELECT count(*)= 4 FROM t1 WHERE b ='p0-29-upd-1';
--source include/wait_condition.inc
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-2' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
INSERT INTO t1 PARTITION (`p0-29`, subp4) VALUES (11, '(p0-29-)ins');
INSERT INTO t1 PARTITION (`p0-29`, subp4) VALUES (12, '(p0-29-)ins');
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
DELETE FROM t1 PARTITION (`p0-29`,subp5) WHERE b= '(p0-29-)ins';
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
--echo reap
reap;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

disconnect session1;
disconnect session2;

connection default;
DROP TABLE t1;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc
--echo # Run in traditional mode
set @@global.sql_mode= traditional;
--echo #######################################################
--echo # UPDATE without partition selection

connect (session1, localhost, test_user_1,'testpw',test);
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--echo send
send
UPDATE t1 SET b='table-upd-1' WHERE a BETWEEN 0 AND 9;

connect (session2, localhost, test_user_2,'testpw',test);
let $wait_condition= SELECT count(*)= 4 FROM t1 WHERE b ='table-upd-1';
--source include/wait_condition.inc
UPDATE t1 SET b='table-upd-2' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
INSERT INTO t1 VALUES (11, 'table-ins');
INSERT INTO t1 VALUES (12, 'table-ins');
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
DELETE FROM t1 WHERE b= 'table-ins';
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
--echo reap
reap;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

--echo #######################################################
--echo # UPDATE with partition selection
#connect (session1, localhost, test_user_1,'testpw',test);
connection session1;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--echo send
send
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-1' WHERE a BETWEEN 0 AND 9;

#connect (session2, localhost, test_user_2,'testpw',test);
connection session2;
let $wait_condition= SELECT count(*)= 4 FROM t1 WHERE b ='p0-29-upd-1';
--source include/wait_condition.inc
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-2' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
INSERT INTO t1 PARTITION (`p0-29`, subp4) VALUES (11, '(p0-29-)ins');
INSERT INTO t1 PARTITION (`p0-29`, subp4) VALUES (12, '(p0-29-)ins');
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
DELETE FROM t1 PARTITION (`p0-29`,subp5) WHERE b= '(p0-29-)ins';
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
--echo reap
reap;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

disconnect session1;
disconnect session2;
connection default;
DROP TABLE t1;
DROP USER test_user_1;
DROP USER test_user_2;

