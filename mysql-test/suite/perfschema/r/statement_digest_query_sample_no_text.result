
##### TEST 1 - VERIFY QUERY_SAMPLE_TEXT LENGTH RETURNS NULL WHEN SQL TEXT LENGTH IS SET TO 0 


# Verify sql text length = 0 
SELECT @@global.performance_schema_max_sql_text_length;
@@global.performance_schema_max_sql_text_length
0

USE test;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (s1 int);
INSERT INTO t1 values (100), (200), (300);
TRUNCATE TABLE performance_schema.events_statements_summary_by_digest;
UPDATE t1 SET s1 = 400;

## 1.3 Verify that QUERY_SAMPLE_TEXT has zero length

SELECT query_sample_text
FROM performance_schema.events_statements_summary_by_digest
WHERE digest_text = "UPDATE `t1` SET `s1` = ?";
query_sample_text
NULL

#### CLEANUP

DROP TABLE t1;
