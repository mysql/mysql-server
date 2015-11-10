CREATE TABLE t (c1 INT, c2 INT, KEY i2 (c2)) ENGINE=INNODB;
SET DEBUG='+d,ib_rename_index_fail1';
ALTER TABLE t RENAME INDEX i2 to x, ALGORITHM=INPLACE;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
SET DEBUG='-d,ib_rename_index_fail1';
SET DEBUG='+d,ib_rename_index_fail2';
ALTER TABLE t RENAME INDEX i2 to x, ROW_FORMAT=REDUNDANT, ALGORITHM=INPLACE;
Warnings:
Warning	1088	Error updating stats for table 't' after table rebuild: Tablespace deleted or being deleted
SET DEBUG='-d,ib_rename_index_fail2';
SELECT index_name, stat_description
FROM mysql.innodb_index_stats
WHERE table_name = 't' AND stat_name LIKE 'n_diff%'
ORDER BY 1, 2;
index_name	stat_description
GEN_CLUST_INDEX	DB_ROW_ID
x	c2
x	c2,DB_ROW_ID
SET DEBUG='+d,ib_rename_index_fail3';
ALTER TABLE t RENAME INDEX x to y, ROW_FORMAT=REDUNDANT, ALGORITHM=INPLACE;
Warnings:
Warning	1088	Deleting persistent statistics for rebuilt table 't' in InnoDB failed: Unable to delete statistics for table test.t: Deadlock. They can be deleted later using DELETE FROM mysql.innodb_index_stats WHERE database_name = 'test' AND table_name = 't'; DELETE FROM mysql.innodb_table_stats WHERE database_name = 'test' AND table_name = 't';
SET DEBUG='-d,ib_rename_index_fail3';
SELECT index_name, stat_description
FROM mysql.innodb_index_stats
WHERE table_name = 't' AND stat_name LIKE 'n_diff%'
ORDER BY 1, 2;
index_name	stat_description
GEN_CLUST_INDEX	DB_ROW_ID
x	c2
x	c2,DB_ROW_ID
y	c2
y	c2,DB_ROW_ID
DROP TABLE t;
