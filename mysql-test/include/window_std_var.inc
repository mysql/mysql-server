--echo Single window function (plus ORDER BY).
CREATE TABLE t(i INT, j INT);
INSERT INTO t VALUES (1,1);
INSERT INTO t VALUES (1,4);
INSERT INTO t VALUES (1,2);
INSERT INTO t VALUES (1,4);

ANALYZE TABLE t;

--echo Single partition, no sorting
SELECT i, j, STD(i) OVER (ROWS UNBOUNDED PRECEDING) std,
      VARIANCE(j) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t;
SELECT i, j, STD(i) OVER (ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) std,
       VARIANCE(j) OVER (ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) variance FROM t;
SELECT i, j, STD(i+j) OVER (ROWS UNBOUNDED PRECEDING) std FROM t ORDER BY std;
SELECT i, j, VAR_POP(i+j) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t ORDER BY variance;
SELECT i, j, STD(i+j) OVER (ROWS UNBOUNDED PRECEDING) std FROM t ORDER BY std DESC;
SELECT i, j, VARIANCE(i+j) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t ORDER BY variance DESC;
--echo Single ordered partition
SELECT i, j, STD(i) OVER (ORDER BY j ROWS UNBOUNDED PRECEDING) std,
      VARIANCE(j) OVER (ORDER BY j ROWS UNBOUNDED PRECEDING) variance FROM t;
SELECT i, j, STDDEV_SAMP(j) OVER (ORDER BY j ROWS UNBOUNDED PRECEDING) std,
       VARIANCE(i) OVER (ORDER BY i ROWS UNBOUNDED PRECEDING) variance FROM t ORDER BY std ;
SELECT i, j, STD(i+j) OVER (ORDER BY j ROWS UNBOUNDED PRECEDING) std FROM t ORDER BY std DESC;
SELECT i, j, VAR_POP(i+j) OVER (ORDER BY j ROWS UNBOUNDED PRECEDING) variance FROM t ORDER BY variance DESC;
SELECT i, j, STDDEV_SAMP(i+j) OVER (ORDER BY j DESC ROWS UNBOUNDED PRECEDING) std FROM t ORDER BY std DESC;
SELECT i, j, VARIANCE(i+j) OVER (ORDER BY j DESC ROWS UNBOUNDED PRECEDING) variance FROM t ORDER BY variance DESC;

--echo Check that we eliminate redundant sorting in ORDER BY even with wfs
SELECT i, j, STD(i+j) OVER (ROWS UNBOUNDED PRECEDING) std FROM t ORDER BY NULL DESC;
--skip_if_hypergraph  # Depends on the query plan.
EXPLAIN FORMAT=JSON SELECT i, j, std(i+j) OVER (ROWS UNBOUNDED PRECEDING) STD FROM t ORDER BY NULL DESC;
--echo With LIMIT
SELECT i, j, STD(i+j) OVER (ROWS UNBOUNDED PRECEDING) STD FROM t ORDER BY std DESC LIMIT 3;

--echo STD/VARIANCE with View
CREATE VIEW v AS
SELECT i, j, STD(i+j) OVER (ORDER BY j DESC ROWS UNBOUNDED PRECEDING) STD,
      VARIANCE(i+j) OVER (ORDER BY j DESC ROWS UNBOUNDED PRECEDING) variance FROM t;
SHOW CREATE VIEW v;
SELECT * FROM v;
DROP VIEW v;

TRUNCATE TABLE t;
INSERT INTO t VALUES (999961560, DEFAULT);
INSERT INTO t VALUES (44721, DEFAULT);
SELECT STD(i) OVER () FROM t;
SELECT VARIANCE(i) OVER () FROM t;
DROP TABLE t;

CREATE TABLE t(i INT, j INT, k INT);
INSERT INTO t VALUES (1,1,1);
INSERT INTO t VALUES (1,4,1);
INSERT INTO t VALUES (1,2,1);
INSERT INTO t VALUES (1,4,1);
INSERT INTO t VALUES (1,4,1);
INSERT INTO t VALUES (1,1,2);
INSERT INTO t VALUES (1,4,2);
INSERT INTO t VALUES (1,2,2);
INSERT INTO t VALUES (1,4,2);
INSERT INTO t VALUES (1,1,3);
INSERT INTO t VALUES (1,4,3);
INSERT INTO t VALUES (1,2,3);
INSERT INTO t VALUES (1,4,3);
INSERT INTO t VALUES (1,1,4);
INSERT INTO t VALUES (1,4,4);
INSERT INTO t VALUES (1,2,4);
INSERT INTO t VALUES (1,4,4);

--echo ----------------------------------------------------------------------
--echo -    Combination with GROUP BY
--echo ----------------------------------------------------------------------

--replace_numeric_round 10
SELECT k, STD(k) OVER (ROWS UNBOUNDED PRECEDING) std,
      VARIANCE(k) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t; # simple
--sorted_result
SELECT k, STD(i), SUM(j), STD(k) OVER (ROWS UNBOUNDED PRECEDING) std_wf FROM t GROUP BY (k);
--sorted_result
SELECT k, STD(i), SUM(j), VAR_POP(k) OVER (ROWS UNBOUNDED PRECEDING) variance_wf FROM t GROUP BY (k);
--sorted_result
SELECT k, STD(i), SUM(j), STD(k) OVER (ROWS UNBOUNDED PRECEDING) std_wf FROM t GROUP BY (k) ORDER BY std_wf DESC;
SELECT k, STD(i), SUM(j), STDDEV(k) OVER (ROWS UNBOUNDED PRECEDING) std_wf FROM t GROUP BY (k) ORDER BY std_wf DESC;
SELECT k, STD(i), SUM(j), STDDEV_POP(k) OVER (ROWS UNBOUNDED PRECEDING) std_wf FROM t GROUP BY (k) ORDER BY std_wf DESC;
SELECT k, STD(i), SUM(j), STDDEV_SAMP(k) OVER (ROWS UNBOUNDED PRECEDING) std_wf FROM t GROUP BY (k) ORDER BY std_wf DESC;
SELECT k, STD(i), SUM(j), VARIANCE(k) OVER (ROWS UNBOUNDED PRECEDING) variance_wf FROM t GROUP BY (k) ORDER BY variance_wf DESC;
SELECT k, STD(i), SUM(j), VAR_POP(k) OVER (ROWS UNBOUNDED PRECEDING) variance_wf FROM t GROUP BY (k) ORDER BY variance_wf DESC;
SELECT k, STD(i), SUM(j), VAR_SAMP(k) OVER (ROWS UNBOUNDED PRECEDING) variance_wf FROM t GROUP BY (k) ORDER BY variance_wf DESC;

--sorted_result
SELECT k, GROUP_CONCAT(j ORDER BY j), STD(k) OVER (ROWS UNBOUNDED PRECEDING) std,
      VAR_SAMP(k) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t GROUP BY (k); # combined with two tmp files GROUP BY
--sorted_result
SELECT k, AVG(DISTINCT j), STD(k) OVER (ROWS UNBOUNDED PRECEDING) std,
      VARIANCE(k) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t GROUP BY (k);
--sorted_result
SELECT k, GROUP_CONCAT(j ORDER BY j), STD(k+1) OVER (ROWS UNBOUNDED PRECEDING) std FROM t GROUP BY (k); # expression argument to std
--sorted_result
SELECT k, GROUP_CONCAT(j ORDER BY j), VARIANCE(k+1) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t GROUP BY (k); # expression argument to VARIANCE
--sorted_result
SELECT k, GROUP_CONCAT(j ORDER BY j), STD(k+1) OVER (ORDER BY k DESC ROWS UNBOUNDED PRECEDING) std FROM t GROUP BY (k);
--sorted_result
SELECT k, GROUP_CONCAT(j ORDER BY j), VAR_POP(k+1) OVER (ORDER BY k DESC ROWS UNBOUNDED PRECEDING) variance FROM t GROUP BY (k);

CREATE TABLE t1 (id INTEGER, sex CHAR(1));
INSERT INTO t1 VALUES (1, 'M');
INSERT INTO t1 VALUES (2, 'F');
INSERT INTO t1 VALUES (3, 'F');
INSERT INTO t1 VALUES (4, 'F');
INSERT INTO t1 VALUES (5, 'M');
INSERT INTO t1 VALUES (10, NULL);
INSERT INTO t1 VALUES (11, NULL);

CREATE TABLE ss(c CHAR(1));
INSERT INTO ss VALUES ('M');

--echo This is grouped aggregate with HAVING in conjunction with a streaming wf
SELECT sex, AVG(id), std(AVG(id)) OVER w std, VARIANCE(AVG(id)) OVER w variance FROM t1
    GROUP BY sex HAVING sex='M' OR sex='F' OR sex IS NULL
    WINDOW w AS (ROWS UNBOUNDED PRECEDING) ORDER BY sex DESC;

--echo Ditto, but HAVING using subquery
SELECT sex, AVG(id), STD(AVG(id)) OVER w std, VARIANCE(AVG(id)) OVER w variance FROM t1
    GROUP BY sex HAVING sex=(SELECT c FROM ss LIMIT 1) OR sex='F' OR sex IS NULL
    WINDOW w AS (ROWS UNBOUNDED PRECEDING) ORDER BY sex DESC;

--echo This is a grouped aggregate in conjunction with a buffered wf
--echo which generates an extra tmp file step
SELECT sex, AVG(id), STDDEV(AVG(id)) OVER w std, VAR_POP(AVG(id)) OVER w variance, NTILE(2) OVER w FROM t1
    GROUP BY sex
    WINDOW w AS (ORDER BY sex ROWS UNBOUNDED PRECEDING) ORDER BY sex DESC;

--echo This is a grouped aggregate with HAVING in conjunction with a buffered wf
--echo which generates an extra tmp file step
SELECT sex, AVG(id), STD(AVG(id)) OVER w std, VARIANCE(AVG(id)) OVER w variance, NTILE(2) OVER w FROM t1
    GROUP BY sex HAVING sex=(SELECT c FROM ss LIMIT 1) OR sex='F' OR sex IS NULL
    WINDOW w AS (ORDER BY sex ROWS UNBOUNDED PRECEDING) ORDER BY sex DESC;

--echo Pure HAVING: In absence of filtering in the grouping sort step,
--echo make sure we filter before windowing steps
SELECT sex, NTILE(2) OVER w , STDDEV_POP(ASCII(sex)) OVER w std,
VARIANCE(ASCII(sex)) OVER w variance FROM t1
HAVING sex=(SELECT c FROM ss LIMIT 1)
  WINDOW w AS (ORDER BY sex ROWS UNBOUNDED PRECEDING);

--echo Bug fix for prepared statements
PREPARE p FROM "SELECT sex, AVG(id), STD(AVG(id)) OVER w std,
        VARIANCE(AVG(id)) OVER w variance, NTILE(2) OVER w FROM t1
    GROUP BY sex HAVING sex=(SELECT c FROM ss LIMIT 1) OR sex='F' OR sex IS NULL
    WINDOW w AS (ORDER BY sex ROWS UNBOUNDED PRECEDING) ORDER BY sex DESC";
EXECUTE p;
EXECUTE p;

DROP PREPARE p;
DROP TABLE t1,ss;

--echo Tests with ROLLUP
SELECT k, STD(i), SUM(j), STDDEV_SAMP(k) OVER (ROWS UNBOUNDED PRECEDING) std_wf FROM t
GROUP BY (k) WITH ROLLUP;
SELECT k, STD(i), SUM(j), VAR_SAMP(k) OVER (ROWS UNBOUNDED PRECEDING) variance_wf FROM t
GROUP BY (k) WITH ROLLUP;
SELECT k, STD(i), SUM(j), STD(k) OVER (ORDER BY k DESC ROWS UNBOUNDED PRECEDING) std_wf FROM t
GROUP BY (k) WITH ROLLUP ORDER BY k DESC;
SELECT k, STD(i), SUM(j), STDDEV(k) OVER (ROWS UNBOUNDED PRECEDING) std_wf FROM t
GROUP BY k,j WITH ROLLUP;

DROP TABLE t;

--echo ----------------------------------------------------------------------
--echo -    STD/VARIANCE with NTH VALUE/LEAD/LAG
--echo ----------------------------------------------------------------------

CREATE TABLE t3(t3_id INT, k INT);
INSERT INTO t3 VALUES (0, 0);
INSERT INTO t3 VALUES (0, 0);
INSERT INTO t3 VALUES (2, 0);
INSERT INTO t3 VALUES (2, 0);
INSERT INTO t3 VALUES (4, 0);
INSERT INTO t3 VALUES (4, 0);
INSERT INTO t3 VALUES (6, 0);
INSERT INTO t3 VALUES (6, 0);
INSERT INTO t3 VALUES (8, 0);
INSERT INTO t3 VALUES (8, 0);
INSERT INTO t3 VALUES (1, 1);
INSERT INTO t3 VALUES (1, 1);
INSERT INTO t3 VALUES (3, 1);
INSERT INTO t3 VALUES (3, 1);
INSERT INTO t3 VALUES (5, 1);
INSERT INTO t3 VALUES (5, 1);
INSERT INTO t3 VALUES (7, 1);
INSERT INTO t3 VALUES (7, 1);
INSERT INTO t3 VALUES (9, 1);
INSERT INTO t3 VALUES (9, 1);

SELECT t3_id, STD(t3_id) OVER w std,
              VARIANCE(t3_id) OVER w variance,
              CUME_DIST() OVER w c_dist,
              LEAD(t3_id, 2) OVER w lead2,
              NTH_VALUE(t3_id, 3) OVER w nth,
              k FROM t3
       WINDOW w AS (PARTITION BY k ORDER BY t3_id);

SELECT t3_id, STDDEV_SAMP(t3_id) OVER w std,
              VARIANCE(t3_id) OVER w variance,
              CUME_DIST() OVER w c_dist,
              LEAD(t3_id, 2) OVER w lead2,
              NTH_VALUE(t3_id, 3) OVER w nth,
              k FROM t3
       WINDOW w AS (PARTITION BY k ORDER BY t3_id RANGE UNBOUNDED PRECEDING);

DROP TABLE t3;

--echo ----------------------------------------------------------------------
--echo -    Window function in parent and subquery
--echo ----------------------------------------------------------------------

CREATE TABLE t(i INT, j INT);
INSERT INTO t VALUES (1,NULL);
INSERT INTO t VALUES (1,NULL);
INSERT INTO t VALUES (1,1);
INSERT INTO t VALUES (1,NULL);
INSERT INTO t VALUES (1,2);
INSERT INTO t VALUES (2,1);
INSERT INTO t VALUES (2,2);
INSERT INTO t VALUES (2,NULL);
INSERT INTO t VALUES (2,NULL);
CREATE TABLE t1 (id INTEGER, sex CHAR(1));
INSERT INTO t1 VALUES (1, 'M');
INSERT INTO t1 VALUES (2, 'F');
INSERT INTO t1 VALUES (3, 'F');
INSERT INTO t1 VALUES (4, 'F');
INSERT INTO t1 VALUES (5, 'M');
CREATE TABLE t2 (user_id INTEGER NOT NULL, date DATE);
INSERT INTO t2 VALUES (1, '2002-06-09');
INSERT INTO t2 VALUES (2, '2002-06-09');
INSERT INTO t2 VALUES (1, '2002-06-09');
INSERT INTO t2 VALUES (3, '2002-06-09');
INSERT INTO t2 VALUES (4, '2002-06-09');
INSERT INTO t2 VALUES (4, '2002-06-09');
INSERT INTO t2 VALUES (5, '2002-06-09');

SELECT t.*, STDDEV_POP(t.rank) OVER (ROWS UNBOUNDED PRECEDING) std,
       VARIANCE(t.rank) OVER (ROWS UNBOUNDED PRECEDING) variance FROM
       (SELECT sex, id, date, ROW_NUMBER() OVER w AS row_no, RANK() OVER w AS `rank` FROM t1,t2
        WHERE t1.id=t2.user_id
        WINDOW w AS (PARTITION BY date ORDER BY id)
       ) AS t;

--echo ----------------------------------------------------------------------
--echo -    Multiple windows
--echo ----------------------------------------------------------------------
SELECT t1.*, RANK() OVER (ORDER BY sex) `rank`, STD(id) OVER (ORDER BY sex,id ROWS UNBOUNDED PRECEDING) std FROM t1;
SELECT t1.*, PERCENT_RANK() OVER (ORDER BY sex) p_rank, VARIANCE(id) OVER (ORDER BY sex,id ROWS UNBOUNDED PRECEDING) variance FROM t1;
SELECT t1.*, CUME_DIST() OVER (ORDER BY sex) c_dist, STDDEV_POP(id) OVER (ORDER BY sex,id ROWS UNBOUNDED PRECEDING) std FROM t1;
SELECT * from (SELECT t1.*, STDDEV(id) OVER (ROWS UNBOUNDED PRECEDING) std, RANK() OVER (ORDER BY sex) `rank` FROM t1) alias ORDER BY id;
SELECT * from (SELECT t1.*, VAR_SAMP(id) OVER (ROWS UNBOUNDED PRECEDING) variance, PERCENT_RANK() OVER (ORDER BY sex) p_rank FROM t1) alias ORDER BY id;
SELECT * from (SELECT t1.*, VARIANCE(id) OVER (ROWS UNBOUNDED PRECEDING) variance, CUME_DIST() OVER (ORDER BY sex) c_dist FROM t1) alias ORDER BY id;

SELECT t1.*, STD(id) OVER (ORDER BY id ROWS UNBOUNDED PRECEDING) std,
       variance(id) OVER (ORDER BY id ROWS UNBOUNDED PRECEDING) VARIANCE,
       RANK() OVER (ORDER BY sex,id) `rank`,
       ROW_NUMBER() OVER (ORDER BY sex,id) row_num
  FROM t1;

--echo a little more windows + subquery
SELECT t.*, STDDEV(id + r00 + r01) OVER (ORDER BY id ROWS UNBOUNDED PRECEDING) AS std,
            variance(id + r00 + r01) OVER (ORDER BY id ROWS UNBOUNDED PRECEDING) AS VARIANCE FROM (
   SELECT t1.*,
       RANK() OVER (ORDER BY sex, id) AS r00,
       RANK() OVER (ORDER BY sex, id DESC) AS r01,
       RANK() OVER (ORDER BY sex, id DESC) AS r02,
       RANK() OVER (PARTITION BY id ORDER BY sex) AS r03,
       RANK() OVER (ORDER BY sex,id) AS r04,
       RANK() OVER (ORDER BY sex,id) AS r05,
       RANK() OVER (ORDER BY sex, id) AS r06,
       RANK() OVER (ORDER BY sex, id) AS r07,
       RANK() OVER (ORDER BY sex, id) AS r08,
       RANK() OVER (ORDER BY sex, id) AS r09,
       RANK() OVER (ORDER BY sex, id) AS r10,
       RANK() OVER (ORDER BY sex, id) AS r11,
       RANK() OVER (ORDER BY sex, id) AS r12,
       RANK() OVER (ORDER BY sex, id) AS r13,
       RANK() OVER (ORDER BY sex, id) AS r14
       FROM t1) t;
DROP TABLE t;

--echo ----------------------------------------------------------------------
--echo -    STD/VARIANCE with frames
--echo ----------------------------------------------------------------------

SELECT AVG(id) OVER w, STD(id) OVER w std, VARIANCE(id) OVER w variance FROM t1
       WINDOW w AS (PARTITION BY sex);

SELECT * FROM (
SELECT id, SUM(id) OVER w, STD(id) OVER w std, VARIANCE(id) OVER w variance, sex FROM t1
       WINDOW w AS (PARTITION BY sex)
       ) alias ORDER BY id;

SELECT STD(id) OVER w std FROM t1 WINDOW w AS (PARTITION BY sex);
SELECT VARIANCE(id) OVER w variance FROM t1 WINDOW w AS (PARTITION BY sex);

SELECT id, STD(id) OVER w std, VARIANCE(id) OVER w variance, sex FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id
                    ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);

--echo try the same as a view
CREATE VIEW v AS
SELECT id, SUM(id) OVER w, STD(id) OVER w std, VARIANCE(id) OVER w variance, sex FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id
                    ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);
SHOW CREATE VIEW v;
SELECT * FROM v;
DROP VIEW v;

SELECT SUM(id) OVER w, STDDEV(id) OVER w std,
       VARIANCE(id) OVER w variance FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id
                    ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);

SELECT id, SUM(id) OVER w, STD(id) OVER w std,
       VARIANCE(id) OVER w variance, sex FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id
                    ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);

SELECT SUM(id) OVER w, COUNT(*) OVER w, STD(id) OVER w std,
      VARIANCE(id) OVER w variance FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id
                    ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);

SELECT id, AVG(id) OVER (ROWS UNBOUNDED PRECEDING) avg,
       STD(id) OVER (ROWS UNBOUNDED PRECEDING) std FROM t1;
SELECT id, AVG(id) OVER (ROWS UNBOUNDED PRECEDING),
       VARIANCE(id) OVER (ROWS UNBOUNDED PRECEDING) variance FROM t1;

SELECT id, AVG(id) OVER w avg, COUNT(id) OVER w count, STD(id) OVER w std,
       VARIANCE(id) OVER w variance FROM t1
       WINDOW w AS (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING);

--echo STD/VARIANCE with double type
CREATE TABLE td(d DOUBLE);
INSERT INTO td VALUES (2);
INSERT INTO td VALUES (2);
INSERT INTO td VALUES (3);
INSERT INTO td VALUES (1);
INSERT INTO td VALUES (1.2);
INSERT INTO td VALUES (NULL);
SELECT d, STD(d) OVER (ORDER BY d) std, VARIANCE(d) OVER (ORDER BY d) variance FROM td;
SELECT d, STD(d) OVER () std, VARIANCE(d) OVER () variance FROM td;
SELECT d, STD(d) OVER (ORDER BY d ROWS BETWEEN 2 PRECEDING AND 1 FOLLOWING) std FROM td;
SELECT d, VARIANCE(d) OVER (ORDER BY d ROWS BETWEEN 2 PRECEDING AND 1 FOLLOWING) variance FROM td;

TRUNCATE td;
INSERT INTO td VALUES (10);
INSERT INTO td VALUES (1);
INSERT INTO td VALUES (2);
INSERT INTO td VALUES (3);
INSERT INTO td VALUES (4);
INSERT INTO td VALUES (5);
INSERT INTO td VALUES (6);
INSERT INTO td VALUES (7);
INSERT INTO td VALUES (8);
INSERT INTO td VALUES (9);
SELECT d, STD(d) OVER w std, VARIANCE(d) OVER w variance FROM td
  WINDOW w AS (ORDER BY d RANGE BETWEEN 2 PRECEDING AND CURRENT ROW);
SELECT d, SUM(d) OVER w sum, STD(d) OVER w std, VARIANCE(d) OVER w variance FROM td
  WINDOW w AS (ORDER BY d RANGE BETWEEN 2 PRECEDING AND 2 FOLLOWING);
SELECT d, SUM(d) OVER w sum, STD(d) OVER w std, VARIANCE(d) OVER w variance FROM td
  WINDOW w AS (ORDER BY d RANGE BETWEEN CURRENT ROW AND 2 FOLLOWING);

INSERT INTO td SELECT * FROM td; # get more duplicates and hence peer sets
SELECT d, SUM(d) OVER w sum, VARIANCE(d) OVER w variance, STD(d) OVER w std FROM td
  WINDOW w AS (ORDER BY d RANGE BETWEEN 2 PRECEDING AND CURRENT ROW);
SELECT d, SUM(d) OVER w sum, VARIANCE(d) OVER w variance, STD(d) OVER w std FROM td
  WINDOW w AS (ORDER BY d RANGE BETWEEN 2 PRECEDING AND 2 FOLLOWING);
SELECT d, SUM(d) OVER w sum, VARIANCE(d) OVER w variance, STD(d) OVER w std FROM td
  WINDOW w AS (ORDER BY d RANGE BETWEEN CURRENT ROW AND 2 FOLLOWING);

DROP TABLE td;

--echo ----------------------------------------------------------------------
--echo -    STD/VARIANCE with frames in combination with non-framing window functions
--echo -    ROW_NUMBER and RANK
--echo ----------------------------------------------------------------------

SELECT ROW_NUMBER() OVER w `row_number`, id, STD(id) OVER w std, VARIANCE(id) OVER w variance, sex FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING);

SELECT ROW_NUMBER() OVER w `row_number`, STD(id) OVER w std,
      VARIANCE(id) OVER w variance FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id
                    ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);

INSERT INTO t1 VALUES (10, NULL);

SELECT RANK() OVER w `rank`, id, STD(id) OVER w std, VARIANCE(id) OVER w variance, sex FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id);

SELECT RANK() OVER w `rank`, STD(id) OVER w std,
       VARIANCE(id) OVER w variance FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id
                    ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);

SELECT id, sex, STD(id) OVER w std,
                VARIANCE(id) OVER w variance,
                ROW_NUMBER() OVER w `row_number`,
                RANK() OVER w  `rank` FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);

SELECT id, sex, STD(id) OVER w std,
                VARIANCE(id) OVER w variance,
                ROW_NUMBER() OVER w `row_number`,
                CUME_DIST() OVER w  `cume_dist` FROM t1
       WINDOW w AS (PARTITION BY sex ORDER BY id ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);


--echo ----------------------------------------------------------------------
--echo -    STD/VARIANCE with RANGE frame specification
--echo ----------------------------------------------------------------------
--echo Make t11 a clone of t1 but with an extra partitioning column, but other values
--echo repeated, so we can test it the same frames work on more than one partition
CREATE TABLE t11 (id INTEGER, sex CHAR(1), p INTEGER DEFAULT 1);
INSERT INTO t11(id, sex) SELECT * FROM t1;
UPDATE t11 SET p=p+1;
INSERT INTO t11(id, sex) SELECT * FROM t1;
--echo Make t22 a clone of t2 but with an extra partitioning column, but other values
--echo repeated, so we can test it the same frames work on more than one partition
CREATE TABLE t22 (user_id INTEGER NOT NULL, date DATE, p INTEGER DEFAULT 1);
INSERT INTO t22(user_id, date) SELECT * FROM t2;
UPDATE t22 SET p=p+1;
INSERT INTO t22(user_id, date) SELECT * FROM t2;

SELECT id, STD(id) OVER (ORDER BY id RANGE 2 PRECEDING) std FROM t1 ORDER BY id;
SELECT id, STD(id) OVER (ORDER BY id RANGE 2 PRECEDING) std FROM t1 ORDER BY id;
SELECT id, STD(id) OVER (ORDER BY id RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) std FROM t1 ORDER BY id;
SELECT id, VARIANCE(id) OVER (ORDER BY id RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) variance FROM t1 ORDER BY id;
SELECT id, STD(id) OVER (ORDER BY id RANGE UNBOUNDED PRECEDING) std FROM t1 ORDER BY id;
SELECT id, VARIANCE(id) OVER (ORDER BY id RANGE UNBOUNDED PRECEDING) variance FROM t1 ORDER BY id;

SELECT p, id, STD(id) OVER (PARTITION BY p ORDER BY id RANGE 2 PRECEDING) std FROM t11 ORDER BY p,id;
SELECT p, id, VARIANCE(id) OVER (PARTITION BY p ORDER BY id RANGE 2 PRECEDING) variance FROM t11 ORDER BY p,id;
SELECT p, id, STD(id) OVER (PARTITION BY p ORDER BY id RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) std FROM t11 ORDER BY p,id;
SELECT p, id, VARIANCE(id) OVER (PARTITION BY p ORDER BY id RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) variance FROM t11 ORDER BY p,id;
SELECT p, id, STD(id) OVER (PARTITION BY p ORDER BY id RANGE UNBOUNDED PRECEDING) std FROM t11 ORDER BY p,id;
SELECT p, id, VARIANCE(id) OVER (PARTITION BY p ORDER BY id RANGE UNBOUNDED PRECEDING) variance FROM t11 ORDER BY p,id;

--echo Implicit frame due to ORDER BY, with last in peer group as upper bound
SELECT user_id, STD(user_id) OVER w, VARIANCE(user_id) OVER w FROM t2 WINDOW w AS (ORDER BY user_id);
--sorted_result
SELECT p, user_id, STD(user_id) OVER w, VARIANCE(user_id) OVER w FROM t22 WINDOW w AS (PARTITION BY p ORDER BY user_id) ORDER BY p;

SELECT user_id, STD(user_id) OVER w, VARIANCE(user_id) OVER w1 FROM t2
    WINDOW w AS (ORDER BY user_id), w1 AS (ORDER BY user_id);

--echo Check descending order by with RANGE: 2 PRECEDING in this case means larger than
--echo current row.
SELECT NTILE(5) OVER w, ROW_NUMBER() OVER w, id, STD(id) OVER w FROM t1
    WINDOW w AS (ORDER BY id DESC RANGE 2 PRECEDING);
SELECT p, NTILE(5) OVER w, ROW_NUMBER() OVER w, id, VARIANCE(id) OVER w FROM t11
    WINDOW w AS (PARTITION BY p ORDER BY id DESC RANGE 2 PRECEDING);
--error ER_WINDOW_RANGE_FRAME_NUMERIC_TYPE
SELECT NTILE(5) OVER w, ROW_NUMBER() OVER w, id, STD(id) OVER w FROM t1
    WINDOW w AS (ORDER BY id DESC RANGE INTERVAL 2 MONTH PRECEDING);

DROP TABLE t11,t22,t1,t2;

CREATE TABLE t3(d DOUBLE);
INSERT INTO t3 VALUES (1.1);
INSERT INTO t3 VALUES (1.9);
INSERT INTO t3 VALUES (4.0);
INSERT INTO t3 VALUES (8.3);
INSERT INTO t3 VALUES (16.0);
INSERT INTO t3 VALUES (24.0);
INSERT INTO t3 VALUES (20.1);
INSERT INTO t3 VALUES (22.0);
INSERT INTO t3 VALUES (23.0);

--replace_numeric_round 10
SELECT d, STD(d) OVER w std, VARIANCE(d) OVER w variance, COUNT(*) OVER w
FROM t3 WINDOW w AS (ORDER BY d RANGE BETWEEN 2.1 PRECEDING AND 1.1 FOLLOWING);
DROP TABLE t3;

--echo ----------------------------------------------------------------------
--echo -    STD/VARIANCE over JSON
--echo ----------------------------------------------------------------------

CREATE TABLE tj(j JSON, i INT DEFAULT 7);
INSERT INTO tj(j) VALUES ('1');
INSERT INTO tj(j) VALUES ('2');
INSERT INTO tj(j) VALUES ('3');
INSERT INTO tj(j) VALUES ('4');
INSERT INTO tj(j) VALUES ('5');
INSERT INTO tj(j) VALUES (NULL);
INSERT INTO tj(j) VALUES ('3.14');
INSERT INTO tj(j) VALUES ('[1,2,3]');
INSERT INTO tj(j) VALUES (NULL);
--replace_numeric_round 10
SELECT STD(j) OVER () AS JSON FROM tj;
# Different line numbers in warnings in hypergraph optimizer.
--disable_warnings
--replace_numeric_round 10
SELECT j, JSON_TYPE(j), STD(j) OVER (ORDER BY j ROWS 3 PRECEDING) FROM tj;
--replace_numeric_round 10
SELECT j, JSON_TYPE(j), VARIANCE(j) OVER (ORDER BY j ROWS 3 PRECEDING) FROM tj;
--enable_warnings
--error ER_WINDOW_RANGE_FRAME_ORDER_TYPE
SELECT j, JSON_TYPE(j), JSON_TYPE(STD(j) OVER (ORDER BY j RANGE 3 PRECEDING)) FROM tj;
INSERT INTO tj(j) VALUES ('3.14');
#SELECT j, JSON_TYPE(j), JSON_TYPE(VARIANCE(j) OVER (ORDER BY j ROWS 3 PRECEDING)) FROM tj;
INSERT INTO tj(j) VALUES ('[1,2,3]');
--replace_numeric_round 10
SELECT j,
       JSON_TYPE(j),
       STD(CASE WHEN JSON_TYPE(j) = 'ARRAY' THEN j->"$[0]" ELSE j END)
         OVER (ORDER BY j ROWS 3 PRECEDING)
  FROM tj;
SELECT DISTINCT i,NTILE(3) OVER (ORDER BY i), VARIANCE(i) OVER (), COUNT(*) OVER () FROM tj ORDER BY NTILE(3) OVER (ORDER BY i);

UPDATE tj SET i=i+CASE WHEN JSON_TYPE(j) = 'ARRAY' THEN 1 ELSE j END;
UPDATE tj SET i=7 where i=8 AND JSON_TYPE(j) != 'ARRAY';

CREATE TABLE tj2 AS SELECT * FROM tj;
UPDATE tj2 SET i=MOD(i,3);
SELECT * FROM tj2;
--echo With GROUP BY
--sorted_result
SELECT          COUNT(*) OVER (), MOD(STD(i),2) FROM tj2 GROUP BY i ;
--sorted_result
SELECT DISTINCT COUNT(*) OVER (), MOD(VARIANCE(i),2) FROM tj2 GROUP BY i ;
SELECT DISTINCT COUNT(*) OVER (), MOD(STD(i),2) as a FROM tj2 GROUP BY i ORDER BY MOD(STD(i),2);
SELECT DISTINCT COUNT(*) OVER (), MOD(STD(i),2) as a FROM tj2 GROUP BY i ORDER BY MOD(STD(i),2) DESC;

--echo Bug fix GROUP BY with window function referring column used in grouping expression
--sorted_result
SELECT i, STD(i) OVER (), MOD(STD(i),2) FROM tj2 GROUP BY i ;
--sorted_result
SELECT i, VARIANCE(VARIANCE(i)) OVER (), VARIANCE(i) OVER (ORDER BY i), MOD(VARIANCE(i),2), VARIANCE(i) FROM tj2 GROUP BY i ;
--sorted_result
SELECT i, STD(i) OVER (ORDER BY i) FROM tj UNION ALL SELECT i, STD(i) OVER (ORDER BY i) FROM tj;
# Different line numbers in warnings, and roundoff (due to different window ordering), in hypergraph optimizer.
--disable_warnings
--replace_numeric_round 14
SELECT j,CAST(STD(j) OVER (PARTITION BY i) AS JSON), CAST(VARIANCE(j) OVER () AS JSON) FROM tj;
SELECT j,CAST(STD(j) OVER (PARTITION BY i ROWS UNBOUNDED PRECEDING) AS JSON), CAST(VARIANCE(j) OVER (PARTITION BY i ROWS UNBOUNDED PRECEDING) AS JSON) FROM tj;
--enable_warnings

DROP TABLE tj,tj2;

CREATE TABLE t1 (i INTEGER, j INTEGER);

INSERT INTO t1 VALUES (NULL,3),(NULL,3),(NULL,30),(NULL,30),(4,3),
       (2,5),(3,7),(1,10),(5,20),(3,30);

SELECT i, j, STD(i) OVER (PARTITION BY j ORDER BY i) FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug#26125954: PROBLEM WITH ITEM_SUM_STD::VAL_REAL
--echo #

CREATE TABLE t1(c1 INT,c2 INT);

INSERT INTO t1 VALUES
(1,5),(2,-1900740608),(3,1938161664),(4,2),(5,6),(6,5),(7,5),
  (8,-921239552),(9,-1347354624),(10,5),(11,-870580224),(12,-30913),
  (13,1267597312),(14,93454336),(15,6),(16,-67174400),(17,10751180),
  (18,659685376),(19,-790691840),(20,882114560),(21,1952907264);

--replace_numeric_round 4
SELECT  STDDEV(c2) OVER ( ROWS 2 PRECEDING ) AS std_deviation FROM t1;

DROP TABLE t1;
