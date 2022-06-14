#
# Bug #28825617 ASSERTION FAILURE: LOB0LOB.CC.*TRX == NULLPTR
# || TRX->IS_READ_UNCOMMITTED()
#
CREATE TABLE t1 (a INT PRIMARY KEY, b TEXT, c TEXT, INDEX (b(1)));
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  `c` text,
  PRIMARY KEY (`a`),
  KEY `b` (`b`(1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Connection con1:
INSERT INTO t1 (a, b, c) VALUES (1, 'hello', repeat('blob ', 12345));
BEGIN;
UPDATE t1 SET b=repeat('blob', 12345), c = 'world' where a = 1;
# Connection default:
set debug = '+d, innodb_cached_old_vers_offsets';
CHECK TABLE t1;
ERROR HY000: Lost connection to MySQL server during query
# restart
DROP TABLE t1;
#
# Repeat the scenario without the debug point.
#
CREATE TABLE t1 (a INT PRIMARY KEY, b TEXT, c TEXT, INDEX (b(1)));
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  `c` text,
  PRIMARY KEY (`a`),
  KEY `b` (`b`(1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Connection con1:
INSERT INTO t1 (a, b, c) VALUES (1, 'hello', repeat('blob ', 12345));
BEGIN;
UPDATE t1 SET b=repeat('blob', 12345), c = 'world' where a = 1;
# Connection default:
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
# Connection con1:
commit;
# Connection default:
DROP TABLE t1;
