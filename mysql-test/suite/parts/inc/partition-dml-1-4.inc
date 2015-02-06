--source include/have_partition.inc
SET @auto= @@global.autocommit;
SET @@global.autocommit= off;
SET @iso= @@global.tx_isolation;
SET @@global.tx_isolation= 'serializable';
SET @completion= @@global.completion_type;
SET @@global.completion_type= 'no_chain';
SET @lock_timeout= @@global.lock_wait_timeout;
SET @@global.lock_wait_timeout= 1;
SELECT @@global.lock_wait_timeout;
SET @innodb_lock_timeout= @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout= 1;
SELECT @@global.innodb_lock_wait_timeout;
let $DATA_PATH=;
let $INDEX_PATH=;

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
UPDATE t1 PARTITION (`p0-29`) SET b='p0-29-upd-1' WHERE a BETWEEN 0 AND 9;
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

connect (session2, localhost, test_user_1,'testpw',test);
--echo ####### expect p30-299 is readable, p0-29 is locked
--sorted_result
SELECT * FROM t1 PARTITION (`p30-299`);
--error ER_LOCK_WAIT_TIMEOUT 
SELECT * FROM t1 PARTITION (`p0-29`);

connection session1;
COMMIT WORK;

connection session2;
--echo ####### expect p0-29 and p30-299 are readable
--sorted_result
SELECT * FROM t1 PARTITION (`p0-29`,`p30-299`);

disconnect session1;
disconnect session2;
connection default;
DROP TABLE t1;
DROP USER test_user_1;
DROP USER test_user_2;

SET @@global.autocommit= @auto;
SET @@global.tx_isolation= @iso;
SET @@global.completion_type= @completion;
SET @@global.lock_wait_timeout= @lock_timeout;
SET @@global.innodb_lock_wait_timeout= @innodb_lock_timeout;

