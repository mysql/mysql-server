--disable_warnings
DROP TABLE IF EXISTS test.t1;
--enable_warnings

eval SET NAMES $cset;

let $std_cset= 1;

if ($bin_cset)
{
 eval CREATE TABLE test.t1 (a VARCHAR(3) CHARACTER SET $cset) ENGINE=$engine_type CHARACTER SET $cset;
 let $bin_cset= 0;
 let $std_cset= 0;
}
if ($std_cset)
{
 eval CREATE TABLE test.t1 (a VARCHAR(3) CHARACTER SET $cset COLLATE $coll) ENGINE=$engine_type CHARACTER SET $cset COLLATE $coll;
}

--replace_column 3 # 4 # 5 # 6 # 7 # 8 # 9 # 10 # 11 # 12 # 13 # 14 # 16 # 17 # 18 #
SHOW TABLE STATUS LIKE 't1';

--disable_warnings
--disable_query_log
let $1= 221;
while ($1)
{
 eval INSERT INTO test.t1 VALUES(CHAR(254-$1));
 dec $1;
}
DELETE FROM test.t1 WHERE CHAR_LENGTH(a) <> 1;
--enable_query_log
--enable_warnings

SELECT HEX(ASCII(a)) AS a_ascii, CHAR_LENGTH(a) AS a_len FROM test.t1 ORDER BY a, ORD(a);

--disable_warnings
DROP TABLE IF EXISTS test.t1;
--enable_warnings

