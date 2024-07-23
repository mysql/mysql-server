--echo #
--echo # Debug build companion of query_expression.inc
--echo # See also suite opt_trace.query_expression{_debug).test
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


--echo # Test spill correctness with secondary overflow using
--echo # injection. This query with set_operations_buffer_size == 16384
--echo # will give 128 chunk files. With default setting it does not
--echo # spill to disk.
let $query = SELECT * FROM t INTERSECT SELECT * FROM t;

SET SESSION optimizer_switch = 'hash_set_operations=off';
eval CREATE TABLE no_hashing AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=default';

# force overflow so we can inject secondary overflow
SET SESSION set_operations_buffer_size = 16384;

SET SESSION debug_set_operations_secondary_overflow_at='0 128 1';
--error ER_SIMULATED_INJECTION_ERROR
eval $query;

SET SESSION debug_set_operations_secondary_overflow_at='1 128 1';
--error ER_SIMULATED_INJECTION_ERROR
eval $query;

SET SESSION debug_set_operations_secondary_overflow_at='2 0 1';
--error ER_SIMULATED_INJECTION_ERROR
eval $query;

--echo # Compare old approach (no hashing) with hashing w/spill and secondary overflow
let $fail_at=0 0 1;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
eval CREATE TABLE t_$sans AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=off';
eval SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM t_$sans) derived;
eval SELECT COUNT(*) FROM (SELECT * FROM t_$sans EXCEPT ALL SELECT * FROM no_hashing) derived;
SET SESSION optimizer_switch = 'hash_set_operations=default';
eval DROP TABLE t_$sans;

--echo # Compare old approach (no hashing) with hashing w/spill and secondary overflow
let $fail_at=0 0 200;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
eval CREATE TABLE t_$sans AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=off';
eval SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM t_$sans) derived;
eval SELECT COUNT(*) FROM (SELECT * FROM t_$sans EXCEPT ALL SELECT * FROM no_hashing) derived;
SET SESSION optimizer_switch = 'hash_set_operations=default';
eval DROP TABLE t_$sans;

--echo # Compare old approach (no hashing) with hashing w/spill and secondary overflow
let $fail_at=0 50 20;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
eval CREATE TABLE t_$sans AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=off';
eval SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM t_$sans) derived;
eval SELECT COUNT(*) FROM (SELECT * FROM t_$sans EXCEPT ALL SELECT * FROM no_hashing) derived;
SET SESSION optimizer_switch = 'hash_set_operations=default';
eval DROP TABLE t_$sans;

--echo # Too high row
let $fail_at=1 127 279;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
--replace_regex /[0-9]+//
--error ER_SIMULATED_INJECTION_ERROR
eval CREATE TABLE t_$sans AS $query;

--echo # Compare old approach (no hashing) with hashing w/spill and secondary overflow
let $fail_at=0 127 200;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
eval CREATE TABLE t_$sans AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=off';
eval SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM t_$sans) derived;
eval SELECT COUNT(*) FROM (SELECT * FROM t_$sans EXCEPT ALL SELECT * FROM no_hashing) derived;
SET SESSION optimizer_switch = 'hash_set_operations=default';
eval DROP TABLE t_$sans;

--echo # Compare old approach (no hashing) with hashing w/spill and secondary overflow
let $fail_at=1 0 1;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
eval CREATE TABLE t_$sans AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=off';
eval SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM t_$sans) derived;
eval SELECT COUNT(*) FROM (SELECT * FROM t_$sans EXCEPT ALL SELECT * FROM no_hashing) derived;
SET SESSION optimizer_switch = 'hash_set_operations=default';
eval DROP TABLE t_$sans;

--echo # Compare old approach (no hashing) with hashing w/spill and secondary overflow
let $fail_at=1 50 20;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
eval CREATE TABLE t_$sans AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=off';
eval SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM t_$sans) derived;
eval SELECT COUNT(*) FROM (SELECT * FROM t_$sans EXCEPT ALL SELECT * FROM no_hashing) derived;
SET SESSION optimizer_switch = 'hash_set_operations=default';
eval DROP TABLE t_$sans;

--echo # Row too high
let $fail_at=1 127 258;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
--replace_regex /[0-9]+//
--error ER_SIMULATED_INJECTION_ERROR
eval CREATE TABLE t_$sans AS $query;

--echo # Compare old approach (no hashing) with hashing w/spill and secondary overflow
--echo # Row in last chunk
let $fail_at=1 127 200;
let $sans = `SELECT REPLACE("$fail_at", ' ', '_')`;
eval SET SESSION debug_set_operations_secondary_overflow_at='$fail_at';
eval CREATE TABLE t_$sans AS $query;
SET SESSION optimizer_switch = 'hash_set_operations=off';
eval SELECT COUNT(*) FROM (SELECT * FROM no_hashing EXCEPT ALL SELECT * FROM t_$sans) derived;
eval SELECT COUNT(*) FROM (SELECT * FROM t_$sans EXCEPT ALL SELECT * FROM no_hashing) derived;
eval DROP TABLE t_$sans;

SET SESSION debug_set_operations_secondary_overflow_at= default;
SET SESSION optimizer_switch = 'hash_set_operations=default';
SET SESSION set_operations_buffer_size = default;

DROP TABLE no_hashing;

--echo #
--echo # Test overflow in resulting tmp table at secondary overflow
--echo #
SET SESSION optimizer_trace="enabled=on";
SET SESSION tmp_table_size=100000;

let $show_trace=
  SELECT JSON_PRETTY(JSON_EXTRACT(trace,"$.steps[*].join_execution"))
  FROM information_schema.optimizer_trace;

let $pattern=$elide_trace_costs_and_rows;
# elide some sorting statistics:
let $pattern=$pattern /num_initial_chunks_spilled_to_disk\": [0-9.]+/num_initial_chunks_spilled_to_disk\": "elided"/;
let $pattern=$pattern /peak_memory_used\": [0-9.]+/peak_memory_used\": "elided"/;

--echo # b) With secondary overflow
SET SESSION debug_set_operations_secondary_overflow_at='0 10 20';
SELECT * FROM (SELECT * FROM t INTERSECT SELECT * FROM t) AS derived ORDER BY i LIMIT 20;
--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;
SET SESSION debug_set_operations_secondary_overflow_at= default;

SET SESSION tmp_table_size=default;
SET SESSION set_operations_buffer_size=default;
SET SESSION optimizer_trace="enabled=default";

DROP TABLE t;
