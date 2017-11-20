
## Test Statement Digest Query Sample WL#9830
##
## A query sample is collected for each new maximum wait time, and is
## displayed in events_statements_summary_by_digest.
##
## TEST 1 - Query sampled when max age disabled, max wait exceeded
## TEST 2 - Query sampled when age expires, max wait not exceeded
## TEST 3 - Query not sampled before max age expires, max wait not exceeded
## TEST 4 - Query sampled before max age, max wait exceeded
## TEST 5 - Verify QUERY_SAMPLE_TEXT is limited by performance_schema_max_sql_text_length
## TEST 6 - Verify QUERY_SAMPLE_TIMER_WAIT and QUERY_SAMPLE_SEEN

##### TEST 1 - QUERY SAMPLED WITH NEW MAX WAIT TIME

## 1.1 SETUP
USE test;
CREATE TABLE t1 (s1 INT);
INSERT INTO t1 VALUES (100), (200), (300);

## 1.2 DISABLE MAX SAMPLE AGE
SET @@global.performance_schema_max_digest_sample_age = 0;
TRUNCATE performance_schema.events_statements_summary_by_digest;

## 1.3 QUERY_1: UPDATE 3 ROWS
UPDATE t1 SET s1 = 400;

## 1.4 COLLECT DIGEST, MAX WAIT, QUERY SAMPLE
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_1, @max_timer_wait_1, @last_seen_1, @query_sample_text_1, @query_sample_seen_1, @query_sample_timer_wait_1
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 1.5 ADD 48K ROWS

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
FLUSH TABLE test.t1;

## 1.6 QUERY_2: UPDATE 12K ROWS
UPDATE t1 SET s1 = 500;

## 1.7 COLLECT DIGEST, MAX WAIT, QUERY SAMPLE
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_2, @max_timer_wait_2, @last_seen_2, @query_sample_text_2, @query_sample_seen_2, @query_sample_timer_wait_2
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 1.8 TRUNCATE, ADD 24 ROWS

TRUNCATE t1;
INSERT INTO t1 VALUES (100), (200), (300);
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;

## 1.9 QUERY_3: UPDATE 24 ROWS
UPDATE t1 SET s1 = 600;

## 1.10 COLLECT DIGEST, MAX WAIT, QUERY SAMPLE
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_3, @max_timer_wait_3, @last_seen_3, @query_sample_text_3, @query_sample_seen_3, @query_sample_timer_wait_3
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 1.11 VERIFY THAT DIGESTS MATCH

SELECT @digest_1 = @digest_2 AS 'Expect 1';
Expect 1
1

SELECT @digest_2 = @digest_3 AS 'Expect 1';
Expect 1
1

## 1.12 VERIFY MAX_TIMER_WAITS

# Max timer wait 1 < max wait 2
SELECT @max_timer_wait_1 < @max_timer_wait_2 AS 'Expect 1';
Expect 1
1

# Max timer wait 3 = max wait 2, no change
SELECT @max_timer_wait_3 = @max_timer_wait_2 AS 'Expect 1';
Expect 1
1

## 1.13. VERIFY QUERY SAMPLES

# Query_1: Sample taken, first query
SELECT @query_sample_text_1 AS 'Expect UPDATE t1 SET s1 = 400';
Expect UPDATE t1 SET s1 = 400
UPDATE t1 SET s1 = 400

# Query_2: Sample taken, max_timer_wait exceeded
SELECT @query_sample_text_2 AS 'Expect UPDATE t1 SET s1 = 500';
Expect UPDATE t1 SET s1 = 500
UPDATE t1 SET s1 = 500

# Query_3: No sample taken, max_timer_wait not exceeded, no change
SELECT @query_sample_text_3 AS 'Expect UPDATE t1 SET s1 = 500';
Expect UPDATE t1 SET s1 = 500
UPDATE t1 SET s1 = 500

##### TEST 2 - QUERY SAMPLED WHEN MAX SAMPLE AGE EXPIRES, MAX WAIT NOT EXCEEDED

## 2.1 SET MAX SAMPLE AGE TO 3 SECONDS
SET @@global.performance_schema_max_digest_sample_age = 3;
TRUNCATE performance_schema.events_statements_summary_by_digest;

TRUNCATE t1;
INSERT INTO t1 VALUES (100), (200), (300);

## 2.2 ADD 12K ROWS

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
FLUSH TABLE test.t1;

## 2.3 QUERY_1: UPDATE 12K ROWS TO SET MAX_TIMER_WAIT
UPDATE t1 SET s1 = 700;

## 2.4 WAIT 10 SECONDS

## 2.5 COLLECT DIGEST, MAX WAIT, QUERY SAMPLE, QUERY SEEN

SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_1, @max_timer_wait_1, @last_seen_1, @query_sample_text_1, @query_sample_seen_1, @query_sample_timer_wait_1
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 2.6 QUERY_2: UPDATE 3 ROWS AGAIN, QUERY SHOULD BE SAMPLED
TRUNCATE t1;
INSERT INTO t1 VALUES (100), (200), (300);
UPDATE t1 SET s1 = 800;

## 2.7 COLLECT DIGEST, MAX WAIT, QUERY SAMPLE, QUERY SEEN
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_2, @max_timer_wait_2, @last_seen_2, @query_sample_text_2, @query_sample_seen_2, @query_sample_timer_wait_2
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 2.8 VERIFY THAT DIGESTS MATCH

SELECT @digest_1 = @digest_2 AS 'Expect 1';
Expect 1
1

## 2.9 VERIFY MAX_TIMER_WAITS, QUERY_SAMPLE_TIMER_WAIT

# Max timer wait 1 = max wait 2
SELECT @max_timer_wait_1 = @max_timer_wait_2 AS 'Expect 1';
Expect 1
1
SELECT @query_sample_timer_wait_1 > @query_sample_timer_wait_2 AS 'Expect 1';
Expect 1
1

## 2.10 VERIFY QUERY SAMPLES

# Query_1: Sample taken, first query
SELECT @query_sample_text_1 AS 'Expect UPDATE t1 SET s1 = 700';
Expect UPDATE t1 SET s1 = 700
UPDATE t1 SET s1 = 700

# Query_2: Sample taken, age expired
SELECT @query_sample_text_2 AS 'Expect UPDATE t1 SET s1 = 800';
Expect UPDATE t1 SET s1 = 800
UPDATE t1 SET s1 = 800


##### TEST 3 - QUERY NOT SAMPLED BEFORE AGE EXPIRES, MAX WAIT NOT EXCEEDED

## 3.1 SET MAX SAMPLE AGE TO 1000 SECONDS
SET @@global.performance_schema_max_digest_sample_age = 1000;

TRUNCATE t1;
INSERT INTO t1 VALUES (100), (200), (300);

## 3.2 ADD 12K ROWS

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
FLUSH TABLE test.t1;
TRUNCATE performance_schema.events_statements_summary_by_digest;

## 3.2 QUERY_1: UPDATE 12K ROWS TO SET MAX_TIMER_WAIT
UPDATE t1 SET s1 = 700;

## 3.3 COLLECT DIGEST, MAX WAIT, query_sample_timer_wait, QUERY SAMPLE, QUERY SEEN, LAST SEEN
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_1, @max_timer_wait_1, @last_seen_1, @query_sample_text_1, @query_sample_seen_1, @query_sample_timer_wait_1
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 3.4 WAIT 10 SECONDS

## 3.5 QUERY_2: UPDATE 3 ROWS, QUERY SHOULD *NOT* BE SAMPLED
TRUNCATE t1;
INSERT INTO t1 VALUES (100), (200), (300);

UPDATE t1 SET s1 = 800;

## 3.6 COLLECT DIGEST, MAX WAIT, query_sample_timer_wait, QUERY SAMPLE, QUERY SEEN, LAST SEEN
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_2, @max_timer_wait_2, @last_seen_2, @query_sample_text_2, @query_sample_seen_2, @query_sample_timer_wait_2
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 3.7 VERIFY THAT DIGESTS MATCH

SELECT @digest_1 = @digest_2 AS 'Expect 1';
Expect 1
1

## 3.8 VERIFY MAX_TIMER_WAITS, QUERY_SAMPLE_TIMER_WAIT

# Max timer wait 1 = max wait 2
SELECT @max_timer_wait_1 = @max_timer_wait_2 AS 'Expect 1 because query is not sampled';
Expect 1 because query is not sampled
1

SELECT @query_sample_timer_wait_1 = @query_sample_timer_wait_2 AS 'Expect 1 because query is not sampled';
Expect 1 because query is not sampled
1

SELECT @last_seen_1 < @last_seen_2 AS 'Expect 1 because a more recent query with same digest was queried';
Expect 1 because a more recent query with same digest was queried
1


## 3.9 VERIFY QUERY SAMPLES - Query 1 == Query 2

# Query_1: Sampled, first query
SELECT @query_sample_text_1 AS 'Expect UPDATE t1 SET s1 = 700';
Expect UPDATE t1 SET s1 = 700
UPDATE t1 SET s1 = 700

# Query_2: Not sampled, age not expired, max_timer_wait not exceeded, no change
SELECT @query_sample_text_2 AS 'Expect UPDATE t1 SET s1 = 700';
Expect UPDATE t1 SET s1 = 700
UPDATE t1 SET s1 = 700

##### TEST 4 - QUERY SAMPLED WHEN MAX SAMPLE AGE DOES NOT EXPIRE BUT MAX WAIT IS EXCEEDED

SET @@global.performance_schema_max_digest_sample_age = 10000;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (s1 int);
INSERT INTO t1 values (100),(200),(300);
TRUNCATE performance_schema.events_statements_summary_by_digest;

## 4.1 Query 1 to test

UPDATE t1 SET s1 = 400;

SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_1, @max_timer_wait_1, @last_seen_1, @query_sample_text_1, @query_sample_seen_1, @query_sample_timer_wait_1
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;

## 4.2 Query 2 to test

UPDATE t1 SET s1 = 500;

SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_2, @max_timer_wait_2, @last_seen_2, @query_sample_text_2, @query_sample_seen_2, @query_sample_timer_wait_2
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## Verify: Digests should be the same
SELECT @digest_1 = @digest_2 AS "check_digest";
check_digest
1

## The second query is expected to be slower
SELECT @max_timer_wait_1 < @max_timer_wait_2 AS "check_wait";
check_wait
1

## The sampled query should be SET s1 = 400, then SET s1 = 500
SELECT @query_sample_text_1 AS "Expect UPDATE t1 SET s1 = 400";
Expect UPDATE t1 SET s1 = 400
UPDATE t1 SET s1 = 400

SELECT @query_sample_text_2 AS "Expect UPDATE t1 SET s1 = 500";
Expect UPDATE t1 SET s1 = 500
UPDATE t1 SET s1 = 500


##### TEST 5 - VERIFY QUERY_SAMPLE_SEEN AND QUERY_SAMPLE_TIMER_WAIT

DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (s1 int);
## 5.1 SET MAX SAMPLE AGE TO 2 SECONDS
SET @@global.performance_schema_max_digest_sample_age = 2;
TRUNCATE performance_schema.events_statements_summary_by_digest;
TRUNCATE performance_schema.events_statements_history;

INSERT INTO t1 VALUES (100), (200), (300);

## 5.2 ADD 12K ROWS

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
FLUSH TABLE test.t1;

## 5.3 QUERY_1: UPDATE 12K ROWS
UPDATE t1 SET s1 = 1000;

## 5.3a COLLECT DIGEST, MAX WAIT, QUERY SAMPLE, QUERY SEEN
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_1, @max_timer_wait_1, @last_seen_1, @query_sample_text_1, @query_sample_seen_1, @query_sample_timer_wait_1
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 5.4. WAIT 10 SECONDS, FORCE AGE

## 5.5. QUERY_2: UPDATE 3 ROWS
TRUNCATE t1;
INSERT INTO t1 VALUES (100), (200), (300);
UPDATE t1 SET s1 = 1100;

## 5.5a COLLECT DIGEST, MAX WAIT, QUERY SAMPLE, QUERY SEEN
SELECT digest, max_timer_wait, last_seen, query_sample_text, query_sample_seen, query_sample_timer_wait
INTO @digest_2, @max_timer_wait_2, @last_seen_2, @query_sample_text_2, @query_sample_seen_2, @query_sample_timer_wait_2
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";

## 5.6 VERIFY MAX_TIMER_WAIT VS QUERY_SAMPLE_TIMER_WAIT

# Verify max_timer_wait has not changed
SELECT @max_timer_wait_1 = @max_timer_wait_2 AS 'Expect 1';
Expect 1
1

# Verify max_timer_wait > query_sample_timer_wait
SELECT @max_timer_wait_2 > @query_sample_timer_wait_2 AS 'Expect 1';
Expect 1
1

## 5.7 VERIFY QUERY_SAMPLE_TIMER_WAIT VS. STATEMENTS HISTORY

SELECT COUNT(*) FROM performance_schema.events_statements_history
WHERE digest = @digest_2 AND timer_wait = @query_sample_timer_wait_2
AND sql_text = @query_sample_text_2;
COUNT(*)
1

## 5.8 VERIFY QUERY_SAMPLE_SEEN == LAST_SEEN

SELECT @query_sample_seen_2 = @last_seen_2 AS 'Expect 1';
Expect 1
1

## CLEAN UP

DROP TABLE t1;

# Restore max sample age
SET @@global.performance_schema_max_digest_sample_age = 60;
