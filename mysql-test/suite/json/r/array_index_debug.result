#
# Bug#30292605: ASSERTION WHEN READING A HIDDEN ARRAY COLUMN
# Bug#30302208: ASSERTION FAILURE WITH CREATE TABLE AS SELECT AND
#               SHOW_HIDDEN_COLUMNS
#
CREATE TABLE t(j JSON, KEY k ((CAST(j->'$[*]' AS SIGNED ARRAY))));
INSERT INTO t VALUES ('[1,2,3]');
SET debug = '+d,show_hidden_columns';
SELECT * FROM t;
j	28d61f7b1f07d88c18f1068410d140b5
[1, 2, 3]	[1, 2, 3]
CREATE TABLE t_copy AS SELECT * FROM t;
ERROR 42000: This version of MySQL doesn't yet support 'Creating tables with array columns.'
SET debug = '-d,show_hidden_columns';
DROP TABLE t;
#
# Bug#30263373: INCORRECT OUTPUT FROM TABLE_FUNCTION_JSON::PRINT()
#
CREATE TABLE t(j JSON,
KEY k1 ((CAST(j->'$.a[*]' AS DATETIME(3) ARRAY))),
KEY k2 ((CAST(j->'$.b[*]' AS UNSIGNED INT ARRAY))));
SET debug = '+d,show_hidden_columns';
SHOW CREATE TABLE t;
Table	Create Table
t	CREATE TABLE `t` (
  `j` json DEFAULT NULL,
  `3caa85db42b2089e2bb7c45a02715df8` bigint unsigned array GENERATED ALWAYS AS (cast(json_extract(`j`,_utf8mb4'$.b[*]') as unsigned array)) VIRTUAL,
  `e2586f3a475714f9959f74986e64ee13` datetime(3) array GENERATED ALWAYS AS (cast(json_extract(`j`,_utf8mb4'$.a[*]') as datetime(3) array)) VIRTUAL,
  KEY `k1` ((cast(json_extract(`j`,_utf8mb4'$.a[*]') as datetime(3) array))),
  KEY `k2` ((cast(json_extract(`j`,_utf8mb4'$.b[*]') as unsigned array)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET debug = '-d,show_hidden_columns';
DROP TABLE t;
