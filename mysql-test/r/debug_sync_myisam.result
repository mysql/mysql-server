CREATE TABLE t1 (c1 INT) ENGINE=myisam;
INSERT INTO t1 VALUES (1);
SELECT GET_LOCK('mysqltest_lock', 100);
GET_LOCK('mysqltest_lock', 100)
1
connection con1
# Sending:
UPDATE t1 SET c1=GET_LOCK('mysqltest_lock', 100);;
connection con2
SET DEBUG_SYNC= 'wait_for_lock SIGNAL locked EXECUTE 2';
INSERT INTO t1 VALUES (1);
connection default
SET DEBUG_SYNC= 'now WAIT_FOR locked';
SELECT RELEASE_LOCK('mysqltest_lock');
RELEASE_LOCK('mysqltest_lock')
1
connection con1
# Reaping UPDATE
SELECT RELEASE_LOCK('mysqltest_lock');
RELEASE_LOCK('mysqltest_lock')
1
connection con2
retrieve INSERT result.
connection default
DROP TABLE t1;
