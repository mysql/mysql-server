--source include/elide_costs.inc

--echo # Pre WL#15257 test using tmp table de-duplication
SET optimizer_switch='hash_set_operations=off';
--source include/query_expression_common.inc

--echo # Pre WL#15257 tests using hash table of WL#15257
SET optimizer_switch='hash_set_operations=default';
--source include/query_expression_common.inc

--echo # Test hash table set operations against "old" approach
--echo # and overflow of in-memory hash table
let $char_type=VARCHAR(60);
--source include/query_expression.inc

--echo # Ditto, now with rows containing a blob
let $char_type=TEXT;
--source include/query_expression.inc

--echo # query_expression.inc tests spilling to disk with two table shapes:
--echo #
--echo #     CREATE TABLE t(i INT, d DATE, c VARCHAR(6) CHARSET latin1
--echo #     CREATE TABLE t(i INT, d DATE, c TEXT CHARSET latin1
--echo #
--echo # At least on MacOS debug builds, this triggers an overflow when we
--echo # attempt store the row payload in a mem_root.  But there is another
--echo # overflow situation: when extending the Robin Hood hash map (it grows
--echo # by doubling as needed on the heap).  We track this memory usage
--echo # against set_operations_buffer_size. The next test hopefully triggers this
--echo # logic path instead.  The shorter record size (a 32 bits integer)
--echo # makes this situation more likely.  Verified with on MacOS build.
CREATE TABLE t(i INT) ENGINE=innodb;

INSERT INTO t
   WITH RECURSIVE cte AS (
      SELECT 0 AS i
      UNION
      SELECT 1 AS i
      UNION
      SELECT i+2 FROM cte
      WHERE i+2 < 1024
   )
   SELECT i FROM cte;

# insert one duplicate of each row
INSERT INTO t select i FROM  t;
ANALYZE TABLE t;
SET SESSION set_operations_buffer_size = 16384;
SELECT * FROM (SELECT * FROM t INTERSECT SELECT * FROM t) AS derived ORDER BY i LIMIT 2;
SET SESSION set_operations_buffer_size = default;

DROP TABLE t;
