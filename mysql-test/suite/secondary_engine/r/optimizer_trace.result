#
# Bug#30625270: ADD SECONDARY_ENGINE_COST_THRESHOLD NOT MET IN TRACE
#               RESPONSE WHEN OFFLOAD FAILS
#
CREATE TABLE t(x INT) SECONDARY_ENGINE = MOCK;
INSERT INTO t VALUES (1), (2), (3);
ALTER TABLE t SECONDARY_LOAD;
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SET optimizer_trace = 'enabled=on';
SELECT /*+ SET_VAR(secondary_engine_cost_threshold=100000) */
* FROM t ORDER BY x;
x
1
2
3
SET optimizer_trace = 'enabled=off';
SELECT JSON_PRETTY(JSON_REMOVE(TRACE->'$**.secondary_engine_not_used',
'$[0].cost')) AS trace
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
trace
[
  {
    "reason": "The estimated query cost does not exceed secondary_engine_cost_threshold.",
    "threshold": 100000
  }
]
DROP TABLE t;
