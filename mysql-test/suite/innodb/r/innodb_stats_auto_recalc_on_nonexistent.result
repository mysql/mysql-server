Test with default setting
CREATE TABLE t (a INT, PRIMARY KEY (a)) ENGINE=INNODB;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	1
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	3
FLUSH TABLE t;
DELETE FROM mysql.innodb_index_stats WHERE table_name = 't';
DELETE FROM mysql.innodb_table_stats WHERE table_name = 't';
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	0
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	0
SELECT * FROM t;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	1
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	3
DROP TABLE t;
Test with explicit enable
CREATE TABLE t (a INT, PRIMARY KEY (a)) ENGINE=INNODB STATS_AUTO_RECALC=1;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	1
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	3
FLUSH TABLE t;
DELETE FROM mysql.innodb_index_stats WHERE table_name = 't';
DELETE FROM mysql.innodb_table_stats WHERE table_name = 't';
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	0
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	0
SELECT * FROM t;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	1
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	3
DROP TABLE t;
Test with explicit disable
CREATE TABLE t (a INT, PRIMARY KEY (a)) ENGINE=INNODB STATS_AUTO_RECALC=0;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	1
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	3
FLUSH TABLE t;
DELETE FROM mysql.innodb_index_stats WHERE table_name = 't';
DELETE FROM mysql.innodb_table_stats WHERE table_name = 't';
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	0
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	0
SELECT * FROM t;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE table_name = 't';
COUNT(*)	0
SELECT COUNT(*) FROM mysql.innodb_index_stats WHERE table_name = 't';
COUNT(*)	0
DROP TABLE t;
