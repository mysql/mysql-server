# 
# UPDATE statements using index_merge optimization
# 

# The include file works as intended only when index_merge_single_col_setup.inc
# or index_merge_multi_col_setup.inc is used. The table t1 set-up is done in
# these two include files.

--disable_warnings
DROP TABLE IF EXISTS t2;
--enable_warnings

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 SELECT * FROM t1;
ANALYZE TABLE t2;

#1. UPDATE statements with various combinations of comparison operators

# intersect optimization technique test cases for non-selects 
# are in a separate file(index_merge_intersect_dml.inc) 
# due to different table requirements. 

# union and sort_union

--let $query = WHERE key1=80 OR key4=49
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key6=7 $query
--eval SELECT COUNT(*), SUM(key6) FROM t1 $query
--eval UPDATE t1 SET key6=7 $query
--eval SELECT COUNT(*), SUM(key6) FROM t1 $query

--let $query = WHERE (key3=80 AND key5 IS NULL) OR (key2=49 AND key6 IS NULL)
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key1=18 $query
--eval SELECT COUNT(*), SUM(key1) FROM t1 $query
--eval UPDATE t1 SET key1=18 $query
--eval SELECT COUNT(*), SUM(key1) FROM t1 $query

--let $query = WHERE (key4=42 AND key6 IS NOT NULL) OR (key1>4 AND key5 IS NULL) AND key2<>50
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key3=18 $query
--eval SELECT COUNT(*), SUM(key3) FROM t1 $query
--eval UPDATE t1 SET key3=18 $query
--eval SELECT COUNT(*), SUM(key3) FROM t1 $query

--let $query = WHERE key2=80 OR key3=1025 OR key4 <=> NULL
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key8=64 $query
--eval SELECT COUNT(*), SUM(key8) FROM t1 $query
--eval UPDATE t1 SET key8=64 $query
--eval SELECT COUNT(*), SUM(key8) FROM t1 $query

--let $query = WHERE (key1 < 3 OR key2 < 2) AND (key3 < 3 OR key4 < 3) AND (key5 < 2 OR key6 < 2)
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key8=18 $query
--eval SELECT COUNT(*), SUM(key8) FROM t1 $query
--eval UPDATE t1 SET key8=18 $query
--eval SELECT COUNT(*), SUM(key8) FROM t1 $query

--let $query = WHERE key1 between 25 AND 40 OR key2 between 15 AND 49
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key3=32 $query
--eval SELECT COUNT(*), SUM(key3) FROM t1 $query
--eval UPDATE t1 SET key3=32 $query
--eval SELECT COUNT(*), SUM(key3) FROM t1 $query

--let $query = WHERE key1 IN (15,70) OR key2 IN (15,70)
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key7=25 $query
--eval SELECT COUNT(*), SUM(key7) FROM t1 $query
--eval UPDATE t1 SET key7=25 $query
--eval SELECT COUNT(*), SUM(key7) FROM t1 $query

#2. Statement with ORDER BY

--let $query = WHERE key5<5 OR key8<10 ORDER BY key1
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key1=10 $query
--eval SELECT COUNT(*), SUM(key1) FROM t1 $query
--eval UPDATE t1 SET key1=10 $query
--eval SELECT COUNT(*), SUM(key1) FROM t1 $query

#3. Multi table SQL statements

--let $query = t1 left join t2 ON (t2.key1=t2.key1) SET t1.key4=15 WHERE t1.key5=40 OR t1.key4=15
--replace_column 9 #
--eval EXPLAIN UPDATE $query
SELECT COUNT(*), SUM(key4) FROM t1; 
--eval UPDATE $query
SELECT COUNT(*), SUM(key4) FROM t1;

# Statements with subqueries

--let $query2 = key6 < ANY (SELECT key6 FROM t1 WHERE key1=10 OR key5=3)
--let $query1 = WHERE (key3=28 OR key4=10) 
--replace_column 9 #
--eval EXPLAIN UPDATE t2 SET key7=7 $query1 AND $query2
--eval SELECT COUNT(*), SUM(key7) FROM t2 $query1 AND $query2
--eval UPDATE t2 SET key7=7 $query1 AND $query2
--eval SELECT COUNT(*), SUM(key7) FROM t2 $query1 AND $query2

--let $query3 = key1 < ANY (SELECT key1 FROM t1 WHERE key1<5 OR key4<7)
--let $query2 = key4 < SOME (SELECT key4 FROM t1 WHERE key1<5 OR key4<8)
--let $query1 = WHERE key1=25 OR key4=40
--replace_column 9 #
--eval EXPLAIN UPDATE t2 SET key2=2 $query1 AND ($query2 OR $query3)
--eval SELECT COUNT(*), SUM(key2) FROM t2 $query1 AND ($query2 OR $query3)
--eval UPDATE t2 SET key2=2 $query1 AND ($query2 OR $query3)
--eval SELECT COUNT(*), SUM(key2) FROM t2 $query1 AND ($query2 OR $query3)

--let $query = WHERE EXISTS (SELECT * FROM t1 WHERE key6<3 OR key4<10)
--replace_column 9 #
--eval EXPLAIN UPDATE t2 SET key3=20 $query
--eval SELECT COUNT(*), SUM(key3) FROM t2 $query
--eval UPDATE t2 SET key3=20 $query
--eval SELECT COUNT(*), SUM(key3) FROM t2 $query

# The following statement doesn't use index_merge optimization technique when 
# the table is created with single column index setup.

--let $query = WHERE key5=80 OR key6=49
--replace_column 9 #
--eval EXPLAIN UPDATE t1 SET key8=50 $query
--eval SELECT COUNT(*), SUM(key8) FROM t1 $query
--eval UPDATE t1 SET key8=50 $query
--eval SELECT COUNT(*), SUM(key8) FROM t1 $query

#
# end of UPDATE statements
#
