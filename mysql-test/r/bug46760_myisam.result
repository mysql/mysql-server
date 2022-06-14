#
# MySQL Bug#39200: optimize table does not recognize
# ROW_FORMAT=COMPRESSED
#
CREATE TABLE t1 (a INT) ROW_FORMAT=compressed, ENGINE=MyISAM;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
OPTIMIZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	status	Table is already up to date
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=COMPRESSED
DROP TABLE t1;
