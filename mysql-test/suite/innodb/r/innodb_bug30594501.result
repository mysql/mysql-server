#
# Bug#30594501 QUERY STALLS IN "QUERY END" STATUS FOR LONG TIME
#
SELECT @@table_open_cache_instances;
@@table_open_cache_instances
1
CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(1);
# Connection con1
SELECT * FROM t1;
a
1
# Connection con2
PREPARE s FROM 'SELECT * FROM `t1`';
# Connection default
DROP TABLE t1;
