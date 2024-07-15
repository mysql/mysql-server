--source include/no_ps_protocol.inc

--echo #
--echo # Bug#35616015: SUM_ROWS_EXAMINED does not count rows correctly
--echo #               for Index Merge (PS.events_statements_summary_by_digest)
--echo #

CREATE TABLE t1 (f1 INT, f2 INT, f3 INT, key(f1), key(f2, f3), key(f2));

INSERT INTO t1 VALUES (0,1,2),(1,2,3),(2,3,4);
INSERT INTO t1 SELECT f1,f2+1,f3+2 FROM t1;
INSERT INTO t1 SELECT f1,f2+1,f3+2 FROM t1;
INSERT INTO t1 SELECT f1,f2+1,f3+2 FROM t1;

ANALYZE TABLE t1;

SET explain_json_format_version = 2;

# Extract the range scans from a plan, excluding filters. (The old
# optimizer might have a redundant filter on top of the range scan,
# whereas the hypergraph optimizer tries to remove redundant filters.)
CREATE PROCEDURE extract_range_scans(plan TEXT)
SELECT description FROM
JSON_TABLE(plan, '$**.operation' COLUMNS (o FOR ORDINALITY,
                                          description TEXT PATH '$')) AS jt
WHERE description NOT LIKE 'Filter:%'
ORDER BY o;

let $show_row_count =
SELECT SUM_ROWS_EXAMINED, SUM_ROWS_SENT
FROM performance_schema.events_statements_summary_by_digest
WHERE schema_name = 'test' AND NOT DIGEST_TEXT LIKE '%TRUNCATE%';

let $query = SELECT /*+ INDEX_MERGE(t1) */ * FROM t1 WHERE f1 = 0 OR f2 = 2;
--eval EXPLAIN FORMAT=JSON INTO @plan $query
CALL extract_range_scans(@plan);
TRUNCATE TABLE performance_schema.events_statements_summary_by_digest;
--eval $query
--eval $show_row_count

let $query =
  SELECT /*+ INDEX_MERGE(t1 f1, f2_2) */ * FROM t1 WHERE f1 > 1 OR f2 < 0;
--eval EXPLAIN FORMAT=JSON INTO @plan $query
CALL extract_range_scans(@plan);
TRUNCATE TABLE performance_schema.events_statements_summary_by_digest;
--eval $query
--eval $show_row_count

let $query = SELECT /*+ INDEX_MERGE(t1) */ * FROM t1 WHERE f1 = 0 AND f2 = 2;
--eval EXPLAIN FORMAT=JSON INTO @plan $query
CALL extract_range_scans(@plan);
TRUNCATE TABLE performance_schema.events_statements_summary_by_digest;
--eval $query
--eval $show_row_count

SET explain_json_format_version = DEFAULT;

DROP PROCEDURE extract_range_scans;
DROP TABLE t1;
