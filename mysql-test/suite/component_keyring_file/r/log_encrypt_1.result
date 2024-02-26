# ----------------------------------------------------------------------
# Setup
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
# Re-starting mysql server with manifest file
# ----------------------------------------------------------------------
# Kill the server
CREATE TABLE t1(c1 INT, c2 char(20)) ENGINE = InnoDB;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
SELECT * FROM t1 ORDER BY c1 LIMIT 10;
c1	c2
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
# Kill the server
SELECT * FROM t1 ORDER BY c1 LIMIT 10;
c1	c2
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
0	aaaaa
DROP TABLE t1;
CREATE TABLE t1(c1 INT, c2 char(20)) ENGINE = InnoDB;
INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");
# Kill the server
SELECT * FROM t1 ORDER BY c1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
DELETE FROM t1;
START TRANSACTION;
INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");
# Kill the server
SELECT * FROM t1 ORDER BY c1 LIMIT 10;
c1	c2
INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");
SELECT * FROM t1 ORDER BY c1 LIMIT 10;
c1	c2
0	aaaaa
1	bbbbb
2	ccccc
3	ddddd
4	eeeee
5	fffff
6	ggggg
7	hhhhh
8	iiiii
9	jjjjj
DROP TABLE t1;
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Restarting server without the manifest file
# ----------------------------------------------------------------------
