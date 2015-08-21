#Server variable option 'lower_case_table_names' sets '0' as default value
#in case sensitive filesystem. Using 'lower_case_table_names=0' in case of
#insensitive filsystem is not allowed.
--source include/have_lowercase0.inc
--source include/have_case_sensitive_file_system.inc

CREATE TABLE t1(f1 INT, f2 INT);
INSERT INTO t1 VALUES
(1,1),(2,2),(3,3);

CREATE TABLE T1 (f1 INT NOT NULL, f2 INT, f3 VARCHAR(32),
                PRIMARY KEY(f1), KEY f2_idx(f1), KEY f3_idx(f3));
INSERT INTO T1 VALUES
(1, 1, 'qwerty'), (2, 1, 'ytrewq'),
(3, 2, 'uiop'), (4, 2, 'poiu'), (5, 2, 'lkjh'),
(6, 2, 'uiop'), (7, 2, 'poiu'), (8, 2, 'lkjh'),
(9, 2, 'uiop'), (10, 2, 'poiu'), (11, 2, 'lkjh'),
(12, 2, 'uiop'), (13, 2, 'poiu'), (14, 2, 'lkjh');
INSERT INTO T1 SELECT f1 + 20, f2, f3 FROM T1;
INSERT INTO T1 SELECT f1 + 40, f2, f3 FROM T1;

ANALYZE TABLE t1;
ANALYZE TABLE T1;

EXPLAIN SELECT /*+ NO_BNL(t1) */ * FROM t1 t1, T1 T1 WHERE T1.f1 between 1 and 3
AND t1.f2 = T1.f2;
EXPLAIN SELECT /*+ NO_BNL(T1) */ * FROM t1 t1, T1 T1 WHERE T1.f1 between 1 and 3
AND t1.f2 = T1.f2;

DROP TABLE t1, T1;
