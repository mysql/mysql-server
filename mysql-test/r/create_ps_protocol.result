CREATE TABLE t1 (f1 INT, f2 INT);
INSERT into t1 VALUE(1,1),(1,2),(1,3),(2,1),(2,2),(2,3);
FLUSH STATUS;
CREATE TABLE t2 SELECT SQL_BIG_RESULT f1, COUNT(f2) FROM t1 GROUP BY f1;
SHOW STATUS LIKE 'handler_read%';
Variable_name	Value
Handler_read_first	1
Handler_read_key	24
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	7
DROP TABLE t1,t2;
