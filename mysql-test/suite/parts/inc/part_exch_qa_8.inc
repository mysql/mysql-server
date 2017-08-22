CREATE USER test2@localhost;

--disable_result_log
--disable_query_log
--source suite/parts/inc/part_exch_tabs.inc
--enable_result_log
--enable_query_log

CREATE DATABASE testdb;
USE testdb;
--disable_result_log
--disable_query_log
--source suite/parts/inc/part_exch_tabs.inc
--enable_result_log
--enable_query_log

USE test;
GRANT CREATE, ALTER, DROP, INSERT, SELECT on test.* TO test2@localhost;
GRANT CREATE, ALTER, DROP, INSERT, SELECT on testdb.* TO test2@localhost;

# 8) Exchanges partition and table and back in 2 sessions with 2 databases.
--echo connect (test2,localhost,test2,,test,MASTER_MYPORT,MASTER_MYSOCK);
connect (test2,localhost,test2,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
# Privileges on both DB's
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE testdb.t_10;
--sorted_result
SELECT * FROM testdb.t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Exchange back.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE testdb.t_10;
--sorted_result
SELECT * FROM testdb.t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;

ALTER TABLE testdb.tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM testdb.tp WHERE a BETWEEN 0 AND 10;
# Exchange back.
ALTER TABLE testdb.tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM testdb.tp WHERE a BETWEEN 0 AND 10;
--echo disconnect test2;
disconnect test2;

--echo connection default;
connection default;
REVOKE INSERT ON testdb.* FROM test2@localhost;

--echo connect (test2,localhost,test2,,test,MASTER_MYPORT,MASTER_MYSOCK);
connect (test2,localhost,test2,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE testdb.t_10;
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE testdb.tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--echo disconnect test2;
disconnect test2;

--echo connection default;
connection default;
--source suite/parts/inc/part_exch_drop_tabs.inc

DROP USER test2@localhost;
DROP DATABASE testdb;

