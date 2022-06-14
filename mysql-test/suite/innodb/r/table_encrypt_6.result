call mtr.add_suppression(".*Encryption can't find master key.*");
call mtr.add_suppression(".*Encryption information in datafile.*");
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
ERROR HY000: Can't find master key from keyring, please check in the server log if a keyring is loaded and initialized successfully.
CREATE TABLE t1(c1 INT, c2 char(20)) ENGINE = InnoDB;
ALTER TABLE t1 ENCRYPTION="Y", algorithm=copy;
ERROR HY000: Can't find master key from keyring, please check in the server log if a keyring is loaded and initialized successfully.
DROP TABLE t1;
# restart: --early-plugin-load=keyring_file=keyring_file.so  --loose-keyring_file_data=MYSQL_TMP_DIR/mysecret_keyring  --plugin-dir=KEYRING_PLUGIN_PATH
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL,
  `c2` char(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
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
# restart:
SELECT * FROM t1 ORDER BY c1 LIMIT 10;
ERROR HY000: Can't find master key from keyring, please check in the server log if a keyring is loaded and initialized successfully.
# restart: --early-plugin-load=keyring_file=keyring_file.so  --loose-keyring_file_data=MYSQL_TMP_DIR/mysecret_keyring  --plugin-dir=KEYRING_PLUGIN_PATH
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
# restart:
