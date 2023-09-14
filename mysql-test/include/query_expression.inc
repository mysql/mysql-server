--echo #
--echo # WL#15257 Enhanced performance for set operations: INTERSECT,
--echo # EXCEPT i.e. hashed implementation of EXCEPT, INTERSECT with
--echo # spill to disk (chunk files) if hash table can't fit in memory
--echo # and fallback to de-duplication via keyed temporary table as
--echo # last resort. The latter is tested with error injection in
--echo # query_expression_debug.
--echo #
--echo # The size of the VARCHAR column is an argument ($char_type) so
--echo # we can test short varchar fields as well as blobs.
--echo #
eval CREATE TABLE t(i INT, d DATE, c $char_type CHARSET latin1) ENGINE=innodb;

set @@cte_max_recursion_depth = 100000;
INSERT INTO t
   WITH RECURSIVE cte AS (
      SELECT 0 AS i, '2022-04-30' AS d, 'abracadabra' as c
      UNION
      SELECT 1 AS i, '2022-04-30' AS d, 'rabarbra' as c
      UNION
      SELECT i+2, d, c FROM cte
      WHERE i+2 < 65536/2
   )
   SELECT i,d,c FROM cte;
set @@cte_max_recursion_depth = default;

# insert one duplicate of each row
INSERT INTO t select i, d, c FROM  t;
ANALYZE TABLE t;


SELECT COUNT(*) FROM t;
SELECT COUNT(*) FROM (SELECT DISTINCT i,d,c FROM t) derived;

 # so we can get reliable Created_tmp_files counts below
FLUSH STATUS;
SHOW STATUS LIKE 'Created_tmp_files%';

SET SESSION optimizer_switch = 'hash_set_operations=off';
SELECT * FROM (SELECT * FROM t INTERSECT SELECT * FROM t) AS derived ORDER BY i LIMIT 20;
SHOW STATUS LIKE 'Created_tmp_files%';

SET SESSION optimizer_switch = 'hash_set_operations=default';
SELECT * FROM (SELECT * FROM t INTERSECT SELECT * FROM t) AS derived ORDER BY i LIMIT 20;
SHOW STATUS LIKE 'Created_tmp_files%';
FLUSH STATUS;
SET SESSION set_operations_buffer_size = 16384;
--echo # The number of Created_tmp_files will be 386, which is
--echo # 128*2 (build, probe chunks) for left operand + 128 (probe) for right operand
--echo # + 2 (REMAININGINPUT for left and right operand) = 386
--echo # The last 128 (probe chunk files for right operand), could have been avoided
--echo # if we had a way to reset IO_CACHE files; now we do a close and open, cf.
--echo # HashJoinChunk::Init.
SELECT * FROM (SELECT * FROM t INTERSECT SELECT * FROM t) AS derived ORDER BY i LIMIT 20;
SHOW STATUS LIKE 'Created_tmp_files%';

SET SESSION set_operations_buffer_size = default;

--echo # Test spill correctness and secondary overflow, the latter
--echo # using injection. This query with set_operations_buffer_size ==
--echo # 16384 will give 128 chunk files. With default setting it does
--echo # not spill to disk.
let $query = SELECT * FROM t INTERSECT SELECT * FROM t;

SET SESSION optimizer_switch = 'hash_set_operations=off';
eval CREATE TABLE no_hashing AS $query;

SET SESSION optimizer_switch = 'hash_set_operations=default';
eval CREATE TABLE hashing_no_spill AS $query;

--echo # Compare old approach (no hashing) with hashing
SET SESSION optimizer_switch = 'hash_set_operations=off';
SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM hashing_no_spill) derived;
SELECT COUNT(*) FROM (SELECT * FROM hashing_no_spill EXCEPT ALL SELECT * FROM no_hashing) derived;
SET SESSION optimizer_switch = 'hash_set_operations=default';

SET SESSION set_operations_buffer_size = 16384;
eval CREATE TABLE hashing_spill AS $query;

--echo # Compare old approach (no hashing) with hashing w/spill
SET SESSION optimizer_switch = 'hash_set_operations=off';
SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM hashing_spill) derived;
SELECT COUNT(*) FROM (SELECT * FROM hashing_spill EXCEPT ALL SELECT * FROM no_hashing) derived;

SET SESSION optimizer_switch = 'hash_set_operations=default';
SET SESSION set_operations_buffer_size = default;

DROP TABLE no_hashing, hashing_no_spill, hashing_spill;

--echo #
--echo # Test overflow in resulting tmp table
--echo #
SET SESSION optimizer_trace="enabled=on";

let $show_trace=
   SELECT JSON_PRETTY(JSON_EXTRACT(trace,"$.steps[*].join_execution"))
   FROM information_schema.optimizer_trace;

let $pattern=$elide_trace_costs_and_rows;
# elide some sorting statistics:
let $pattern=$pattern /num_initial_chunks_spilled_to_disk\": [0-9.]+/num_initial_chunks_spilled_to_disk\": "elided"/;
let $pattern=$pattern /peak_memory_used\": [0-9.]+/peak_memory_used\": "elided"/;

--echo # a) When we spill
SET SESSION tmp_table_size=100000;
SELECT * FROM (SELECT * FROM t INTERSECT SELECT * FROM t) AS derived ORDER BY i LIMIT 20;
--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

--echo # b) With secondary overflow (part of query_expression_debug
--echo #    since it requires error injection)

--echo # c) When we can hash in memory
SET SESSION set_operations_buffer_size=2621440;
SELECT * FROM (SELECT * FROM t INTERSECT SELECT * FROM t) AS derived ORDER BY i LIMIT 20;
--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

SET SESSION tmp_table_size=default;
SET SESSION set_operations_buffer_size=default;
SET SESSION optimizer_trace="enabled=default";

DROP TABLE t;
