#
# Create related tests that result in an different output when run under --ps.
#
#

--source include/have_ps_protocol.inc
#
# Bug#15130:CREATE .. SELECT was denied to use advantages of the
# SQL_BIG_RESULT.
#

CREATE TABLE t1 (f1 INT, f2 INT);
INSERT into t1 VALUE(1,1),(1,2),(1,3),(2,1),(2,2),(2,3);
FLUSH STATUS;
CREATE TABLE t2 SELECT SQL_BIG_RESULT f1, COUNT(f2) FROM t1 GROUP BY f1;
SHOW STATUS LIKE 'handler_read%';
DROP TABLE t1,t2;
