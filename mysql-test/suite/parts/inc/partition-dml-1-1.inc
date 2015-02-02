let $DATA_PATH=;
let $INDEX_PATH=;
--source include/have_partition.inc

--echo # Additional tests for WL#5217 by QA, testplan 1.1

let $TABLENAME= t1;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc

--enable_connect_log

CREATE USER test_user_1 IDENTIFIED BY 'testpw';
CREATE USER test_user_2 IDENTIFIED BY 'testpw';
GRANT ALL PRIVILEGES ON test.* TO test_user_1;
GRANT ALL PRIVILEGES ON test.* TO test_user_2;

connect (session1, localhost, test_user_1,'testpw',test);
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--echo send
send
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-1' WHERE a BETWEEN 0 AND 9;

connect (session2, localhost, test_user_2,'testpw',test);
let $wait_condition= SELECT count(*)= 4 FROM t1 WHERE b ='p0-29-upd-1';
--source include/wait_condition.inc
--sorted_result
SELECT * FROM t1 PARTITION (`p30-299`);
UPDATE t1 PARTITION (`p30-299`) SET b='p30-299-upd' WHERE a BETWEEN 30 AND 39;
--sorted_result
SELECT * FROM t1 PARTITION (`p30-299`);
--sorted_result
SELECT * FROM t1 PARTITION (`p300-2999`);
UPDATE t1 PARTITION (`p300-2999`) SET b='p300-2999-upd' WHERE a BETWEEN 300 AND 309;
--sorted_result
SELECT * FROM t1 PARTITION (`p300-2999`);
--sorted_result
SELECT * FROM t1 PARTITION (`p3000-299999`);
UPDATE t1 PARTITION (`p3000-299999`) SET b='p3000-299999-upd' WHERE a BETWEEN 3000 AND 3009;
--sorted_result
SELECT * FROM t1 PARTITION (`p3000-299999`);
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-2' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
UPDATE t1 PARTITION (subp3) SET b='subp3-upd';
UPDATE t1 PARTITION (subp4) SET b='subp4-upd';
UPDATE t1 PARTITION (subp5) SET b='subp5-upd';
--sorted_result
SELECT * FROM t1 PARTITION (subp3);
--sorted_result
SELECT * FROM t1 PARTITION (subp4);
--sorted_result
SELECT * FROM t1 PARTITION (subp5);
SELECT * FROM t1 PARTITION (subp4,subp5,subp3) order by b,a;

connection session1;
--echo reap
reap;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
SELECT 
(SELECT count(*) FROM t1 PARTITION (pneg,`p0-29`,`p30-299`,`p300-2999`,`p3000-299999`)) = 
(SELECT count(*) FROM t1) as `MUST BE TRUE (1)` ;

# 2) empty (sub)partitions 
let $TABLENAME= t2;
--source suite/parts/inc/partition-dml_cr_tab.inc

SELECT * FROM t2 PARTITION (`p30-299`);
UPDATE t2 PARTITION (`p30-299`) SET b='p30-299-upd' WHERE a BETWEEN 30 AND 39;
DELETE FROM t2 PARTITION (`p30-299`) WHERE a BETWEEN 30 AND 39;
SELECT * FROM t2 PARTITION (`p300-2999`);
UPDATE t2 PARTITION (`p300-2999`) SET b='p300-2999-upd' WHERE a BETWEEN 300 AND 309;
DELETE FROM t2 PARTITION (`p300-2999`) WHERE a BETWEEN 300 AND 309;
SELECT * FROM t2 PARTITION (`p3000-299999`);
UPDATE t2 PARTITION (`p3000-299999`) SET b='p3000-299999-upd' WHERE a BETWEEN 3000 AND 3009;
DELETE FROM t2 PARTITION (`p3000-299999`) WHERE a BETWEEN 3000 AND 3009;
SELECT * FROM t2 PARTITION (`p0-29`);
UPDATE t2 PARTITION (`p0-29`) SET b='p0-29-upd-2' WHERE a BETWEEN 0 AND 9;
DELETE FROM t2 PARTITION (`p0-29`) WHERE a BETWEEN 0 AND 9;
UPDATE t2 PARTITION (subp3) SET b='subp3-upd';
DELETE FROM t2 PARTITION (subp3);
UPDATE t2 PARTITION (subp4) SET b='subp4-upd';
DELETE FROM t2 PARTITION (subp4);
UPDATE t2 PARTITION (subp5) SET b='subp5-upd';
DELETE FROM t2 PARTITION (subp5);
UPDATE t2 PARTITION (subp4,subp5,subp3) SET b='subp-upd';
DELETE FROM t2 PARTITION (subp4,subp5,subp3);
SELECT * FROM t2 PARTITION (subp3);
SELECT * FROM t2 PARTITION (subp4);
SELECT * FROM t2 PARTITION (subp5);

disconnect session1;
disconnect session2;
connection default;
DROP TABLE t1, t2;
DROP USER test_user_1;
DROP USER test_user_2;

