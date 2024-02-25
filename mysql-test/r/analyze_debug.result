#
# BUG#32224917: ANALYZE TABLE TAKES TABLE LOCK DURING INDEX STATS UPDATE,
#               CAUSES QUERY PILEUP
#
CREATE TABLE t1 (c1 INT PRIMARY KEY) ENGINE=INNODB;
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (c2 INT PRIMARY KEY) ENGINE=INNODB
PARTITION BY HASH (c2)
PARTITIONS 4;
INSERT INTO t2 VALUES (2);
connect  con1,localhost,root;
SET DEBUG_SYNC="before_reset_query_plan SIGNAL first_select_ongoing WAIT_FOR second_select_finished";
SELECT c1 FROM t1;
connection default;
SET DEBUG_SYNC="now WAIT_FOR first_select_ongoing";
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT c1 FROM t1;
c1
1
SET DEBUG_SYNC="now SIGNAL second_select_finished";
connection con1;
c1
1
SET DEBUG_SYNC="before_reset_query_plan SIGNAL first_select_ongoing WAIT_FOR second_select_finished";
SELECT c2 FROM t2;
connection default;
SET DEBUG_SYNC="now WAIT_FOR first_select_ongoing";
ANALYZE TABLE t2;
Table	Op	Msg_type	Msg_text
test.t2	analyze	status	OK
SELECT c2 FROM t2;
c2
2
SET DEBUG_SYNC="now SIGNAL second_select_finished";
connection con1;
c2
2
disconnect con1;
connection default;
DROP TABLE t1, t2;
