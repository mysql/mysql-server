
#### Test 1 - VERIFY QUERY_SAMPLE_TEXT LENGTH IS LIMITED BY PERFORMANCE_SCHEMA_MAX_SQL_TEXT_LENGTH


# 1.1 Verify sql text length = 10
SELECT @@global.performance_schema_max_sql_text_length;
@@global.performance_schema_max_sql_text_length
10

USE test;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (s1 int);
INSERT INTO t1 values (100), (200), (300);
TRUNCATE TABLE performance_schema.events_statements_summary_by_digest;
UPDATE t1 SET s1 = 500;

## 1.2 Verify that QUERY_SAMPLE_TEXT is truncated to 10 bytes

SELECT length(query_sample_text) = 10 AS 'Expect 1 as QUERY_SAMPLE_TEXT is 10'
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";
Expect 1 as QUERY_SAMPLE_TEXT is 10
1

## CLEANUP

DROP TABLE t1;
