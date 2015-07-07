#
# DELETE Statements
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

#1. SQL statments with various combinations of comparison operators

# union and sort_union 

--let $query = FROM t1 WHERE key1=25 OR key4=10
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE key1=48 OR  key4=2 OR key6=3
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE key3=1025 OR key5 IS NULL
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE key3=45 OR key1=6 OR key6 IS NULL
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

# The following query could produce different query plans due to variations
# in statistics
-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

--let $query = FROM t1 WHERE (key4=42 AND key6 IS NOT NULL) OR (key1=4 AND key3=6)
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE key1<2 OR key2<3
--replace_result "i1,i2" "i2,i1"
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE key1<5 OR key3<7
--replace_result "i1,i3" "i3,i1"
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

INSERT INTO t1 VALUES (10,10,10,10,10,10,10,10),(3,3,3,3,3,3,3,3);
--let $query = FROM t1 WHERE key2=10 OR key3=3 OR key4 IS NULL
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

INSERT INTO t1 VALUES (10,10,10,10,10,10,10,10),(3,3,3,3,3,3,3,3);
--let $query = FROM t1 WHERE key2=10 OR key3=3 OR key4 <=> null
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE (key1>30 AND key1<35) OR (key2>32 AND key2<40)
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE key1 BETWEEN 30 AND 35 OR key2 BETWEEN 32 AND 40
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE (key1>30 AND key1<35) OR (key2>32 AND key2<40) OR (key3>20 AND key3<45)
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE key1 BETWEEN 30 AND 35 OR key2 BETWEEN 32 AND 40 OR key3 BETWEEN 20 AND 45
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

INSERT INTO t1 VALUES (1,1,1,1,1,1,1,1),(2,2,2,2,2,2,2,2),(3,3,3,3,3,3,3,3),(4,4,4,4,4,4,4,4);
--let $query = FROM t1 WHERE ((key3<4 OR key5<3) AND (key1<3 OR key2<3)) OR (((key3<5 AND key6<5) OR key5<2) AND (key5<4 OR key6<4))
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE (key4<42 AND key6 IS NOT NULL) OR (key1>4 AND key5 IS NULL) AND key2<>50
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

--let $query = FROM t1 WHERE (key5 IN (15,70) OR key6 IN (15,70)) 
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

#2. Statements with ORDER BY

INSERT INTO t1 VALUES (1,1,1,1,1,1,1,1),(2,2,2,2,2,2,2,2),(3,3,3,3,3,3,3,3);
--let $query = FROM t1 WHERE key1<2 OR key2<3 ORDER BY key1
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

#3. Multi table SQL statements 

--let $query = FROM t1 USING t1 INNER JOIN t2 ON (t2.key1=t2.key1) WHERE t1.key1=3 OR t1.key2=4
--replace_column 10 #
--eval EXPLAIN DELETE $query
SELECT COUNT(*) FROM t1;
--eval DELETE $query
SELECT COUNT(*) FROM t1;

# Statements with subqueries

--let $query1 = FROM t2 WHERE key6 = (SELECT key6 FROM t1 WHERE key1=10 OR key5=3 OR key2 IS NULL)
--let $query2 = key3=28 OR key4=10
--replace_column 10 #
--eval EXPLAIN DELETE $query1 OR $query2
--eval SELECT COUNT(*) $query1 OR $query2
--eval DELETE $query1 OR $query2
--eval SELECT COUNT(*) $query1 OR $query2

--let $query3 = key1 < ANY (SELECT key1 FROM t1 WHERE key1<5 OR key4<7) 
--let $query2 = key4 < SOME (SELECT key4 FROM t1 WHERE key1<5 OR key4<8)
--let $query1 = FROM t2 WHERE key1=25 OR key4=40
--replace_column 10 #
--eval EXPLAIN DELETE $query1 AND ($query2 OR $query3)
--eval SELECT COUNT(*) $query1 AND ($query2 OR $query3)
--eval DELETE $query1 AND ($query2 OR $query3)
--eval SELECT COUNT(*) $query1 AND ($query2 OR $query3)

INSERT INTO t1 VALUES (2,2,2,2,2,2,2,2),(5,5,5,5,5,5,5,5);
--let $query = FROM t2 WHERE EXISTS (SELECT * FROM t1 WHERE key1<5 OR key3<7)
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

# The following statement uses index_merge optimization only when the table is 
# created with multi column setup. 

INSERT INTO t1 VALUES ( 3,3,3,3,3,3,3,3 ),(4,4,4,4,4,4,4,4);
--let $query = FROM t1 WHERE key7 = 3 OR key8 = 4
--replace_column 10 #
--eval EXPLAIN DELETE $query
--eval SELECT COUNT(*) $query
--eval DELETE $query
--eval SELECT COUNT(*) $query

#
# end of DELETE statements 
#


