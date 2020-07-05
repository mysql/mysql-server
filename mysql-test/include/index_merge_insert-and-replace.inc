#
# INSERT and REPLACE using index_merge optimization
#

# The include file works as intended only when index_merge_single_col_setup.inc
# or index_merge_multi_col_setup.inc is used. The table t1 set-up is done in
# these two include files.

--disable_warnings
DROP TABLE IF EXISTS t2;
--enable_warnings

CREATE TABLE t2 LIKE t1;

let $i=10;
while ($i)
{
  INSERT INTO t2 VALUES (rand()*6, rand()*6, rand()*6, rand()*6,
                         rand()*6, rand()*6, rand()*6, rand()*6);
  dec $i;
}

ANALYZE TABLE t2;

# intersect optimization technique test cases for non-selects 
# are in a separate file(index_merge_intersect_dml.inc) 
# due to different table requirements.

#
# REPLACE
#

# union

--let $query = INTO t2 SELECT * FROM t1 WHERE key3=1025 OR key5 IS NULL
--replace_column 10 #
--eval EXPLAIN REPLACE $query
--eval SELECT COUNT(*) FROM t2
--eval REPLACE $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

--let $query = INTO t2 SELECT * FROM t1 WHERE key1=48 OR key4=2 OR key6=3
--replace_column 10 #
--eval EXPLAIN REPLACE $query
--eval SELECT COUNT(*) FROM t2
--eval REPLACE $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

# sort_union

--let $query = INTO t2 SELECT * FROM t1 WHERE key1<2 OR key2<3
--replace_column 10 #
--eval EXPLAIN REPLACE $query
--eval SELECT COUNT(*) FROM t2
--eval REPLACE $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

--let $query = INTO t2 SELECT * FROM t1 WHERE (key1>30 AND key1<35) OR (key2>32 AND key2<40)
--replace_column 10 #
--eval EXPLAIN REPLACE $query
--eval SELECT COUNT(*) FROM t2
--eval REPLACE $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

# statements with use/force/ignore index

--let $query = INTO t2 SELECT * FROM t1 IGNORE INDEX(i2) WHERE key1<2 OR key2<3
--replace_column 10 #
--eval EXPLAIN REPLACE $query
--eval SELECT COUNT(*) FROM t2
--eval REPLACE $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

INSERT INTO t1 VALUES (42,42,42,42,3,3,3,3),(4,4,6,6,6,6,4,4);
ANALYZE TABLE t1;

--let $query = INTO t2 SELECT * FROM t1 USE INDEX(i6,i3) WHERE (key4=42 AND key6=3) OR (key1=4 AND key3=6)
--replace_column 10 #
--eval EXPLAIN  REPLACE $query
--eval SELECT COUNT(*) FROM t2
--eval REPLACE $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

--let $query = INTO t2 SELECT * FROM t1 FORCE INDEX(i1,i2) WHERE (key1>1 OR key2>2)
--replace_column 10 #
--eval EXPLAIN REPLACE $query
--eval SELECT COUNT(*) FROM t2
--eval REPLACE $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

#
# INSERT 
#

# union

INSERT INTO t1 VALUES (42,42,42,42,3,3,3,3),(4,4,6,6,6,6,4,4);
ANALYZE TABLE t1;
--let $query = INTO t2 SELECT * FROM t1 WHERE (key4=42 AND key6=3) OR (key1=4 AND key3=6) AND key5<>50
--replace_column 10 #
--eval EXPLAIN INSERT $query
--eval SELECT COUNT(*) FROM t2
--eval INSERT $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

# sort_union

--let $query = INTO t2 SELECT * FROM t1 WHERE key2=10 OR key3=3 OR key4 <=> null
--replace_column 10 #
--eval EXPLAIN INSERT $query
--eval SELECT COUNT(*) FROM t2
--eval INSERT $query                   
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

# statements with use/force/ignore index

--let $query = INTO t2 SELECT * FROM t1 IGNORE INDEX(i1) WHERE key1<2 OR key2<3
--replace_column 10 #
--eval EXPLAIN INSERT $query
--eval SELECT COUNT(*) FROM t2
--eval INSERT $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

--let $query = INTO t2 SELECT * FROM t1 USE INDEX(i5,i6) WHERE (key1<2 OR key2<2) AND (key3<3 OR key4<3) AND (key5<5 OR key6<5);
--replace_column 10 #
--eval EXPLAIN INSERT $query
--eval SELECT COUNT(*) FROM t2
--eval INSERT $query
--eval ANALYZE TABLE t2;
--eval SELECT COUNT(*) FROM t2

--let $query = INTO t2 SELECT * FROM t1 FORCE INDEX(i4,i5) WHERE (key4>3 OR key5>5)
--replace_column 10 #
--eval EXPLAIN INSERT $query
--eval SELECT COUNT(*) FROM t2
--eval INSERT $query
--eval SELECT COUNT(*) FROM t2

#
# end 
#
