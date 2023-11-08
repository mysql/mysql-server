--source include/have_debug.inc
--source include/not_hypergraph.inc  # Chooses different query plans that do not exercise the path with the bug.

--echo #
--echo # Bug #30769515 INCORRECT ERROR HANDLING IN GROUP-BY PLANS.
--echo #

let $engine=innodb;
--source include/group_skip_scan_ext_data.inc

SET debug= '+d,bug30769515_QUERY_INTERRUPTED';
--error ER_QUERY_INTERRUPTED
SELECT a, b, MAX(d), MIN(d) FROM t
WHERE (a > 6) AND (c = 3 OR c = 40) AND
      (d = 11 OR d = 12)
GROUP BY a, b;
SET debug= '-d,bug30769515_QUERY_INTERRUPTED';
#SHOW STATUS LIKE 'handler_read%';


ALTER TABLE t DROP KEY k1;
ALTER TABLE t ADD KEY k1(a, b DESC, c, d DESC);
ANALYZE TABLE t;
SET debug= '+d,bug30769515_QUERY_INTERRUPTED';
--error ER_QUERY_INTERRUPTED
SELECT a, b, MAX(d), MIN(d) FROM t
WHERE (a > 6) AND (c = 3 OR c = 40) AND
      (d = 11 OR d = 12)
GROUP BY a, b;
SET debug= '-d,bug30769515_QUERY_INTERRUPTED';

DROP TABLE t;
