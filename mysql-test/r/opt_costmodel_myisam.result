#
# Bug#23508576 FILL_INDEXES_FROM_DD() DOES NOT INITIALIZE
#              ST_KEY::M_IN_MEMORY_ESTIMATE
#
UPDATE mysql.engine_cost
SET cost_value=0.5
WHERE cost_name="memory_block_read_cost";
UPDATE mysql.engine_cost
SET cost_value=2
WHERE cost_name="io_block_read_cost";
FLUSH OPTIMIZER_COSTS;
SELECT engine_name, device_type, cost_name, cost_value
FROM mysql.engine_cost
WHERE cost_name="memory_block_read_cost"
   OR cost_name="io_block_read_cost";
engine_name	device_type	cost_name	cost_value
default	0	io_block_read_cost	2
default	0	memory_block_read_cost	0.5
CREATE TABLE t1 (
i1 INTEGER,
c1 CHAR(200),
INDEX idx (i1)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1, "Ullensvang"), (2, "Odda"), (3, "Jondal");
# Pass criteria for the query plan:
# 1. Should be executed as an index-only range scan
# 2. Cost estimate: "read_cost" - "eval_cost" should be approximately
#    the cost of reading one page from memory, ie. about 0.5.
EXPLAIN FORMAT=JSON
SELECT i1 FROM t1 WHERE i1 > 1;
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "0.92"
    },
    "table": {
      "table_name": "t1",
      "access_type": "range",
      "possible_keys": [
        "idx"
      ],
      "key": "idx",
      "used_key_parts": [
        "i1"
      ],
      "key_length": "5",
      "rows_examined_per_scan": 2,
      "rows_produced_per_join": 2,
      "filtered": "100.00",
      "using_index": true,
      "cost_info": {
        "read_cost": "0.72",
        "eval_cost": "0.20",
        "prefix_cost": "0.92",
        "data_read_per_join": "1K"
      },
      "used_columns": [
        "i1"
      ],
      "attached_condition": "(`test`.`t1`.`i1` > 1)"
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`i1` AS `i1` from `test`.`t1` where (`test`.`t1`.`i1` > 1)
DROP TABLE t1;
UPDATE mysql.engine_cost
SET cost_value=DEFAULT;
FLUSH OPTIMIZER_COSTS;
