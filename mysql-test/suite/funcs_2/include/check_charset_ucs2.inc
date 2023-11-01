--disable_warnings
DROP TABLE IF EXISTS test.t1;
DROP DATABASE test;
eval CREATE DATABASE test CHARACTER SET utf8mb3;
USE test;
--enable_warnings
SET NAMES utf8mb3;
eval CREATE TABLE test.t1 (a CHAR(4) CHARACTER SET utf8mb3) ENGINE=$engine_type CHARACTER SET utf8mb3;
--replace_column 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 # 13 # 14 # 16 # 17 # 18 #
SHOW TABLE STATUS LIKE 't1';

--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
eval LOAD DATA INFILE 
'$MYSQL_TEST_DIR/suite/funcs_2/data/charset_utf8.txt' INTO TABLE test.t1;
eval ALTER TABLE test.t1 CHANGE a a CHAR(4) CHARACTER SET $cset COLLATE $coll;
DELETE FROM test.t1 WHERE CHAR_LENGTH(a) <> 1;

SELECT HEX(ORD(a)) AS a_ord, CHAR_LENGTH(a) AS a_len FROM test.t1 ORDER BY a, ORD(a);

--disable_warnings
DROP TABLE IF EXISTS test.t1;
--enable_warnings
