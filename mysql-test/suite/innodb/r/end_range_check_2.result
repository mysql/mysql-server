#
# Bug #26300119	ASSERTION IN PREBUILT->TRX->ISOLATION_LEVEL == TRX_ISO_READ_UNCOMMITTED
#
CREATE PROCEDURE populate_t1(IN BASE INT, IN SIZE INT)
BEGIN
DECLARE i INT DEFAULT BASE;
WHILE (i <= SIZE) DO
INSERT INTO t1 VALUES  (i, repeat('10101010101010101010',2000));
SET i = i + 1;
END WHILE;
END|
CREATE TABLE t1 (
pk int,
col_blob_key blob,
primary key (pk)
);
CALL populate_t1(1, 100);
START TRANSACTION;
CALL populate_t1(101, 199);
SET DEBUG_SYNC='before_insertion_of_blob SIGNAL halted_insert WAIT_FOR proceed';
INSERT INTO t1 VALUES (200, repeat('10101010101010101010',2000));;
SET DEBUG_SYNC='now WAIT_FOR halted_insert';
SET DEBUG_SYNC='allow_insert SIGNAL proceed';
SELECT pk, col_blob_key FROM t1 WHERE pk between 99 and 100;
COMMIT;
SET DEBUG_SYNC='RESET';
DROP PROCEDURE populate_t1;
DROP TABLE t1;
#
# Bug#32291506 ASSERTION FAILURE: ROW0SEL.CC: MYSQL_COL_LEN == LEN
#
CREATE TABLE t1 (
c0 int,
c1 char(200) CHARACTER SET utf32,
c2 int GENERATED ALWAYS AS (c1) VIRTUAL,
c3 int,
PRIMARY KEY (c0, c1),
KEY i1 (c2, c1(1))
);
INSERT  INTO t1(c0,c1,c3) VALUES (1,'1',1);
START TRANSACTION;
DELETE FROM t1;
SET DEBUG = "+d, compare_end_range";
# No lock Range select with KEY i1 and end range check
SELECT * FROM t1  WHERE c2 < 0;
c0	c1	c2	c3
# X lock Range select with KEY i1 and end range check
DELETE FROM t1  WHERE c2 < 0;
SET DEBUG = "-d, compare_end_range";
COMMIT;
DROP TABLE t1;
#
# Bug#33384537 prebuilt->m_end_range should be reset at fetch_cache initializing at row_search_mvcc()
#
create table t10 (i int);
insert into t10 values (0), (1), (2), (3), (4), (5), (6), (7), (8), (9);
create table t1000 (i int);
insert into t1000 select a.i + b.i*10 + c.i*100 from t10 a, t10 b, t10 c;
create table t (i int, j int, k int, primary key(k, j, i), key(i, j, k));
insert into t select 1, 1, i from t10;
insert into t select 1, 2, i from t10;
insert into t select 2, 1, i from t10;
insert into t select 2, 2, i from t10;
insert into t select 3, 1, i from t10;
insert into t select 3, 2, i from t10;
begin;
select * from t where i = 4;
i	j	k
insert into t select 1, 1, i+10 from t1000 limit 400;
insert into t select 1, 2, i+10 from t1000;
insert into t select 2, 1, i+10 from t1000;
insert into t select 2, 2, i+10 from t1000;
insert into t select 3, 1, i+10 from t1000;
insert into t select 3, 2, i+10 from t1000;
select /*+ SKIP_SCAN(t i) */ * from t where k > 2 and k < 8 order by i, j, k;
i	j	k
1	1	3
1	1	4
1	1	5
1	1	6
1	1	7
1	2	3
1	2	4
1	2	5
1	2	6
1	2	7
2	1	3
2	1	4
2	1	5
2	1	6
2	1	7
2	2	3
2	2	4
2	2	5
2	2	6
2	2	7
3	1	3
3	1	4
3	1	5
3	1	6
3	1	7
3	2	3
3	2	4
3	2	5
3	2	6
3	2	7
drop table t, t10, t1000;
