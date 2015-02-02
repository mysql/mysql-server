--source include/have_partition.inc
SET @auto= @@global.autocommit;
SET @@global.autocommit= off;
SET @iso= @@global.tx_isolation;
SET @@global.tx_isolation= 'repeatable-read';
SET @completion= @@global.completion_type;
SET @@global.completion_type= 'no_chain';
let $DATA_PATH=;
let $INDEX_PATH=;

--echo # Additional tests for WL#5217 by QA, testplan 1.1

let $TABLENAME= t1;
--source suite/parts/inc/partition-dml_cr_tab.inc
--source suite/parts/inc/partition-dml_ins_tab.inc

--enable_connect_log

CREATE USER test_user_1 IDENTIFIED BY 'testpw';
CREATE USER test_user_2 IDENTIFIED BY 'testpw';
CREATE USER test_user_3 IDENTIFIED BY 'testpw';
GRANT ALL PRIVILEGES ON test.* TO test_user_1;
GRANT ALL PRIVILEGES ON test.* TO test_user_2;
GRANT ALL PRIVILEGES ON test.* TO test_user_3;

connect (session1, localhost, test_user_1,'testpw',test);
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-1' WHERE a BETWEEN 0 AND 9;

connect (session2, localhost, test_user_2,'testpw',test);
UPDATE t1 PARTITION (`p30-299`) SET b='p30-299-upd-1' WHERE a BETWEEN 30 AND 39;

connection session1;
--echo ####### expect only p0-29 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

connection session2;
--echo ####### expect only p30-299 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

connection session1;
COMMIT WORK;
--echo ####### expect only p0-29 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

connection session2;
--echo ####### expect only p30-299 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

connect (session3, localhost, test_user_3,'testpw',test);
--echo ####### expect only p0-29 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);
disconnect session3;

connection session2;
COMMIT WORK;
--echo ####### expect p0-29 and p30-299 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

connection session1;
--echo ####### expect only p30-299 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);
COMMIT WORK;
--echo ####### expect p0-29 and p30-299 is updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

disconnect session1;
disconnect session2;
connection default;
DROP TABLE t1;
DROP USER test_user_1;
DROP USER test_user_2;
DROP USER test_user_3;

SET @@global.autocommit= @auto;
SET @@global.tx_isolation= @iso;
SET @@global.completion_type= @completion;

