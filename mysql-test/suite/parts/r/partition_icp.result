#
# test using Index Condition Pushdown for partitioned tables
#
# Test behavior of ICP calls -> use ICP for InnoDB even for BLOB indexes.
CREATE TABLE t1 (a int PRIMARY KEY, b BLOB, c varchar(16) DEFAULT 'Filler...', INDEX (b(4), a))
PARTITION BY HASH (a) PARTITIONS 3;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` blob,
  `c` varchar(16) DEFAULT 'Filler...',
  PRIMARY KEY (`a`),
  KEY `b` (`b`(4),`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`a`)
PARTITIONS 3 */
INSERT INTO t1 (a, b) VALUES (1, 0xdeadbeef), (2, "text filler"),
(3, 'filler...'), (4, " more filler "), (5, "test text"), (6, "testing...");
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
EXPLAIN SELECT a, HEX(b) FROM t1 WHERE b >= 'te' and (a % 2);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	p0,p1,p2	range	b	b	7	NULL	4	100.00	Using index condition; Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,hex(`test`.`t1`.`b`) AS `HEX(b)` from `test`.`t1` where ((`test`.`t1`.`b` >= 'te') and (0 <> (`test`.`t1`.`a` % 2)))
EXPLAIN FORMAT=JSON SELECT a, HEX(b) FROM t1 WHERE b >= 'te' and (a % 2);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "2.06"
    },
    "table": {
      "table_name": "t1",
      "partitions": [
        "p0",
        "p1",
        "p2"
      ],
      "access_type": "range",
      "possible_keys": [
        "b"
      ],
      "key": "b",
      "used_key_parts": [
        "b"
      ],
      "key_length": "7",
      "rows_examined_per_scan": 4,
      "rows_produced_per_join": 4,
      "filtered": "100.00",
      "index_condition": "(0 <> (`test`.`t1`.`a` % 2))",
      "cost_info": {
        "read_cost": "1.66",
        "eval_cost": "0.40",
        "prefix_cost": "2.06",
        "data_read_per_join": "352"
      },
      "used_columns": [
        "a",
        "b"
      ],
      "attached_condition": "(`test`.`t1`.`b` >= 'te')"
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,hex(`test`.`t1`.`b`) AS `HEX(b)` from `test`.`t1` where ((`test`.`t1`.`b` >= 'te') and (0 <> (`test`.`t1`.`a` % 2)))
SELECT a, HEX(b) FROM t1 WHERE b >= 'te' and (a % 2);
a	HEX(b)
1	DEADBEEF
5	746573742074657874
DROP TABLE t1;
