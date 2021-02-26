# We need the Debug Sync Facility.
--source include/have_debug_sync.inc

--echo #
--echo # BUG#32224917: ANALYZE TABLE TAKES TABLE LOCK DURING INDEX STATS UPDATE,
--echo #               CAUSES QUERY PILEUP
--echo #

CREATE TABLE t1 (c1 INT PRIMARY KEY) ENGINE=INNODB;
INSERT INTO t1 VALUES (1);

CREATE TABLE t2 (c2 INT PRIMARY KEY) ENGINE=INNODB
PARTITION BY HASH (c2)
PARTITIONS 4;
INSERT INTO t2 VALUES (2);

--enable_connect_log
--connect (con1,localhost,root)
SET DEBUG_SYNC="before_reset_query_plan SIGNAL first_select_ongoing WAIT_FOR second_select_finished";
--send SELECT c1 FROM t1

--connection default
SET DEBUG_SYNC="now WAIT_FOR first_select_ongoing";
ANALYZE TABLE t1;
# Without the patch, this SELECT would wait indefinitely.
SELECT c1 FROM t1;

SET DEBUG_SYNC="now SIGNAL second_select_finished";

--connection con1
reap;

SET DEBUG_SYNC="before_reset_query_plan SIGNAL first_select_ongoing WAIT_FOR second_select_finished";
--send SELECT c2 FROM t2

--connection default
SET DEBUG_SYNC="now WAIT_FOR first_select_ongoing";
ANALYZE TABLE t2;
# Without the patch, this SELECT would wait indefinitely.
SELECT c2 FROM t2;

SET DEBUG_SYNC="now SIGNAL second_select_finished";

--connection con1
reap;

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default
--disable_connect_log

DROP TABLE t1, t2;
