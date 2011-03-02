CREATE USER test_2@localhost;

--source include/not_embedded.inc
--disable_result_log
--disable_query_log
--source suite/parts/inc/part_exch_tabs.inc
--enable_result_log
--enable_query_log

# 8) Exchanges partition and table and back in 2 sessions with an insert.
# Parallel INSERT and SELECT
# LOCK behaviour when exchanging different partitons.
--echo send
--send
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;

--echo connect (test_2,localhost,test_2,,test,MASTER_MYPORT,MASTER_MYSOCK);
connect (test_2,localhost,test_2,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
# Wait until exchange has been done.
let $wait_condition= SELECT count(a)>0 FROM tp WHERE a=1;
--source include/wait_condition.inc
# Expect 1,3,5,9 in tp and 2,4,6,8 in t_10
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Exchange back.
INSERT INTO tp VALUES (7,"Seven");
# Expect 2,4,6,8 in tp
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;

--echo connection default;
connection default;
--echo reap;
reap;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;

--echo connection test_2;
connection test_2;
let $wait_condition= SELECT count(a)>0 FROM tp WHERE a=2;
--source include/wait_condition.inc
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
--echo disconnect test_2;
disconnect test_2;

--echo connection default;
connection default;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
--source suite/parts/inc/part_exch_drop_tabs.inc

DROP USER test_2@localhost;

