drop table if exists t1;
set @@sql_mode="ANSI";
select @@sql_mode;
@@sql_mode
REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI
SELECT 'A' || 'B';
'A' || 'B'
AB
CREATE TABLE t1 (id INT, id2 int);
SELECT id,NULL,1,1.1,'a' FROM t1 GROUP BY id;
id	NULL	1	1.1	a
SELECT id FROM t1 GROUP BY id2;
ERROR 42000: Expression #1 of SELECT list is not in GROUP BY clause and contains nonaggregated column 'test.t1.id' which is not functionally dependent on columns in GROUP BY clause; this is incompatible with sql_mode=only_full_group_by
drop table t1;
SET @@SQL_MODE="";
