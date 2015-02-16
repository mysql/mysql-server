--source include/have_partition.inc
SET @auto= @@global.autocommit;
SET @@global.autocommit= off;
SET @iso= @@global.tx_isolation;
SET @@global.tx_isolation= 'repeatable-read';
SET @completion= @@global.completion_type;
SET @@global.completion_type= 'no_chain';
SET @lock_timeout= @@global.lock_wait_timeout;
SET @@global.lock_wait_timeout= 1;
SELECT @@global.lock_wait_timeout;
SET @innodb_lock_timeout= @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout= 1;
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
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 0 AND 9 LOCK IN SHARE MODE;
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-1' WHERE a BETWEEN 0 AND 9 ;

connect (session2, localhost, test_user_2,'testpw',test);
--echo ####### expect p0-29 has not been updated
SELECT * FROM t1 PARTITION (`p0-29`);
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 0 AND 9 LOCK IN SHARE MODE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 20 AND 29 FOR UPDATE;

connection session1;
COMMIT WORK;
--echo ####### expect p0-29 (0-9) has been updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session2;
--echo ####### expect p0-29 has not been updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 20 AND 29 FOR UPDATE;
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-2' WHERE a BETWEEN 20 AND 29 ;

connection session1;
--echo ####### expect only p0-29 (0-9) has been updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 0 AND 9 LOCK IN SHARE MODE;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 PARTITION (`p0-29`) WHERE a BETWEEN 20 AND 29 FOR UPDATE;

connection session2;
COMMIT WORK;
--echo ####### expect p0-29 is completely updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
--echo ####### expect only p0-29 (0-9) has been updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);
COMMIT WORK;
--echo ####### expect p0-29 is completely updated
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`);

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
SET @@global.lock_wait_timeout= @lock_timeout;
SET @@global.innodb_lock_wait_timeout= @innodb_lock_timeout;

