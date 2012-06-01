SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE
database_name = 'test' AND
table_name = 'test_ps_create_table';
COUNT(*)	0
CREATE TABLE test_ps_create_table (a INT, PRIMARY KEY (a))
ENGINE=INNODB STATS_PERSISTENT=0;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE
database_name = 'test' AND
table_name = 'test_ps_create_table';
COUNT(*)	0
DROP TABLE test_ps_create_table;
CREATE TABLE test_ps_create_table (a INT, PRIMARY KEY (a))
ENGINE=INNODB;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE
database_name = 'test' AND
table_name = 'test_ps_create_table';
COUNT(*)	1
DROP TABLE test_ps_create_table;
CREATE TABLE test_ps_create_table (a INT, PRIMARY KEY (a))
ENGINE=INNODB STATS_PERSISTENT=default;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE
database_name = 'test' AND
table_name = 'test_ps_create_table';
COUNT(*)	1
DROP TABLE test_ps_create_table;
CREATE TABLE test_ps_create_table (a INT, PRIMARY KEY (a))
ENGINE=INNODB STATS_PERSISTENT=1;
SELECT COUNT(*) FROM mysql.innodb_table_stats WHERE
database_name = 'test' AND
table_name = 'test_ps_create_table';
COUNT(*)	1
DROP TABLE test_ps_create_table;
