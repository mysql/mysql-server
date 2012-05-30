CREATE TABLE arddl (a INT, b INT, PRIMARY KEY (a)) ENGINE=INNODB;
INSERT INTO arddl VALUES (1, 10);
INSERT INTO arddl VALUES (2, 10);
ALTER TABLE arddl ADD INDEX (b);
SELECT n_rows FROM mysql.innodb_table_stats WHERE table_name = 'arddl' ORDER BY 1;
n_rows	2
SELECT index_name, stat_name, stat_value FROM mysql.innodb_index_stats WHERE table_name = 'arddl' AND index_name = 'PRIMARY' ORDER BY 1, 2, 3;
index_name	PRIMARY
stat_name	n_diff_pfx01
stat_value	2
index_name	PRIMARY
stat_name	n_leaf_pages
stat_value	1
index_name	PRIMARY
stat_name	size
stat_value	1
DROP TABLE arddl;
CREATE TABLE arddl (a INT, b INT, PRIMARY KEY (a), KEY (b)) ENGINE=INNODB;
INSERT INTO arddl VALUES (3, 10);
INSERT INTO arddl VALUES (4, 10);
ALTER TABLE arddl DROP INDEX b;
SELECT n_rows FROM mysql.innodb_table_stats WHERE table_name = 'arddl' ORDER BY 1;
n_rows	2
SELECT index_name, stat_name, stat_value FROM mysql.innodb_index_stats WHERE table_name = 'arddl' AND index_name = 'PRIMARY' ORDER BY 1, 2, 3;
index_name	PRIMARY
stat_name	n_diff_pfx01
stat_value	2
index_name	PRIMARY
stat_name	n_leaf_pages
stat_value	1
index_name	PRIMARY
stat_name	size
stat_value	1
DROP TABLE arddl;
