--echo #
--echo # Test optimizer trace for secondary overflow
--echo #

eval CREATE TABLE t(i INT, d DATE, c $char_type CHARSET latin1) ENGINE=innodb;

SET @@cte_max_recursion_depth = 100000;
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

SET @@cte_max_recursion_depth = default;

# insert one duplicate of each row
INSERT INTO t select i, d, c FROM  t;

ANALYZE TABLE t;

let $show_trace=
   SELECT JSON_PRETTY(JSON_EXTRACT(trace,"$.steps[*].join_execution"))
   FROM information_schema.optimizer_trace;

SET SESSION optimizer_trace='enabled=on';

SET SESSION set_operations_buffer_size = 16 * 1024;
SET SESSION debug_set_operations_secondary_overflow_at= '1 50 20';
SELECT COUNT(*) FROM (SELECT * FROM t INTERSECT SELECT * FROM t) derived;
--skip_if_hypergraph
eval $show_trace;

SET SESSION debug_set_operations_secondary_overflow_at= default;

SET SESSION optimizer_switch = 'hash_set_operations=default';
SET SESSION optimizer_trace = 'enabled=default';
SET SESSION set_operations_buffer_size = default;

DROP TABLE t;

