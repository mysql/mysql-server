# Include file to extend partition_crash with a second table.
# To be used with WL#4445: EXCHANGE PARTITION WITH TABLE.
--eval $create_statement2
--eval $insert_statement2
SHOW CREATE TABLE t2;
--sorted_result
SELECT * FROM t2;
--source suite/parts/inc/partition_crash.inc
SHOW CREATE TABLE t2;
--sorted_result
SELECT * FROM t2;
DROP TABLE t2;
