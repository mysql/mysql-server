CREATE TABLE t (
a INT,
b INT,
c INT GENERATED ALWAYS AS(a+b),
d INT GENERATED ALWAYS AS(a+b+b),
KEY idxa (a),
KEY vidxcd (c, d)
) ENGINE=INNODB;
INSERT INTO t (a,b) VALUES (1, 2);
SELECT index_name, stat_name, stat_description
FROM mysql.innodb_index_stats
WHERE database_name = 'test' AND table_name = 't';
index_name	stat_name	stat_description
GEN_CLUST_INDEX	n_diff_pfx01	DB_ROW_ID
GEN_CLUST_INDEX	n_leaf_pages	Number of leaf pages in the index
GEN_CLUST_INDEX	size	Number of pages in the index
idxa	n_diff_pfx01	a
idxa	n_diff_pfx02	a,DB_ROW_ID
idxa	n_leaf_pages	Number of leaf pages in the index
idxa	size	Number of pages in the index
vidxcd	n_diff_pfx01	c
vidxcd	n_diff_pfx02	c,d
vidxcd	n_diff_pfx03	c,d,DB_ROW_ID
vidxcd	n_leaf_pages	Number of leaf pages in the index
vidxcd	size	Number of pages in the index
ALTER TABLE t ADD COLUMN e INT GENERATED ALWAYS AS(a+a+b), ADD INDEX idxb (b), ALGORITHM=INPLACE;
SELECT index_name, stat_name, stat_description
FROM mysql.innodb_index_stats
WHERE database_name = 'test' AND table_name = 't';
index_name	stat_name	stat_description
GEN_CLUST_INDEX	n_diff_pfx01	DB_ROW_ID
GEN_CLUST_INDEX	n_leaf_pages	Number of leaf pages in the index
GEN_CLUST_INDEX	size	Number of pages in the index
idxa	n_diff_pfx01	a
idxa	n_diff_pfx02	a,DB_ROW_ID
idxa	n_leaf_pages	Number of leaf pages in the index
idxa	size	Number of pages in the index
idxb	n_diff_pfx01	b
idxb	n_diff_pfx02	b,DB_ROW_ID
idxb	n_leaf_pages	Number of leaf pages in the index
idxb	size	Number of pages in the index
vidxcd	n_diff_pfx01	c
vidxcd	n_diff_pfx02	c,d
vidxcd	n_diff_pfx03	c,d,DB_ROW_ID
vidxcd	n_leaf_pages	Number of leaf pages in the index
vidxcd	size	Number of pages in the index
ALTER TABLE t DROP COLUMN c, DROP INDEX idxa, ALGORITHM=INPLACE;
SELECT index_name, stat_name, stat_description
FROM mysql.innodb_index_stats
WHERE database_name = 'test' AND table_name = 't';
index_name	stat_name	stat_description
GEN_CLUST_INDEX	n_diff_pfx01	DB_ROW_ID
GEN_CLUST_INDEX	n_leaf_pages	Number of leaf pages in the index
GEN_CLUST_INDEX	size	Number of pages in the index
idxb	n_diff_pfx01	b
idxb	n_diff_pfx02	b,DB_ROW_ID
idxb	n_leaf_pages	Number of leaf pages in the index
idxb	size	Number of pages in the index
vidxcd	n_diff_pfx01	d
vidxcd	n_diff_pfx02	d,DB_ROW_ID
vidxcd	n_leaf_pages	Number of leaf pages in the index
vidxcd	size	Number of pages in the index
ALTER TABLE t ADD INDEX vidxe (e), ALGORITHM=INPLACE;
SELECT index_name, stat_name, stat_description
FROM mysql.innodb_index_stats
WHERE database_name = 'test' AND table_name = 't';
index_name	stat_name	stat_description
GEN_CLUST_INDEX	n_diff_pfx01	DB_ROW_ID
GEN_CLUST_INDEX	n_leaf_pages	Number of leaf pages in the index
GEN_CLUST_INDEX	size	Number of pages in the index
idxb	n_diff_pfx01	b
idxb	n_diff_pfx02	b,DB_ROW_ID
idxb	n_leaf_pages	Number of leaf pages in the index
idxb	size	Number of pages in the index
vidxcd	n_diff_pfx01	d
vidxcd	n_diff_pfx02	d,DB_ROW_ID
vidxcd	n_leaf_pages	Number of leaf pages in the index
vidxcd	size	Number of pages in the index
vidxe	n_diff_pfx01	e
vidxe	n_diff_pfx02	e,DB_ROW_ID
vidxe	n_leaf_pages	Number of leaf pages in the index
vidxe	size	Number of pages in the index
ALTER TABLE t ADD COLUMN f INT GENERATED ALWAYS AS(a + a), ADD INDEX vidxf (f), ALGORITHM=INPLACE;
SELECT index_name, stat_name, stat_description
FROM mysql.innodb_index_stats
WHERE database_name = 'test' AND table_name = 't';
index_name	stat_name	stat_description
GEN_CLUST_INDEX	n_diff_pfx01	DB_ROW_ID
GEN_CLUST_INDEX	n_leaf_pages	Number of leaf pages in the index
GEN_CLUST_INDEX	size	Number of pages in the index
idxb	n_diff_pfx01	b
idxb	n_diff_pfx02	b,DB_ROW_ID
idxb	n_leaf_pages	Number of leaf pages in the index
idxb	size	Number of pages in the index
vidxcd	n_diff_pfx01	d
vidxcd	n_diff_pfx02	d,DB_ROW_ID
vidxcd	n_leaf_pages	Number of leaf pages in the index
vidxcd	size	Number of pages in the index
vidxe	n_diff_pfx01	e
vidxe	n_diff_pfx02	e,DB_ROW_ID
vidxe	n_leaf_pages	Number of leaf pages in the index
vidxe	size	Number of pages in the index
vidxf	n_diff_pfx01	f
vidxf	n_diff_pfx02	f,DB_ROW_ID
vidxf	n_leaf_pages	Number of leaf pages in the index
vidxf	size	Number of pages in the index
ALTER TABLE t DROP INDEX vidxcd;
SELECT index_name, stat_name, stat_description
FROM mysql.innodb_index_stats
WHERE database_name = 'test' AND table_name = 't';
index_name	stat_name	stat_description
GEN_CLUST_INDEX	n_diff_pfx01	DB_ROW_ID
GEN_CLUST_INDEX	n_leaf_pages	Number of leaf pages in the index
GEN_CLUST_INDEX	size	Number of pages in the index
idxb	n_diff_pfx01	b
idxb	n_diff_pfx02	b,DB_ROW_ID
idxb	n_leaf_pages	Number of leaf pages in the index
idxb	size	Number of pages in the index
vidxe	n_diff_pfx01	e
vidxe	n_diff_pfx02	e,DB_ROW_ID
vidxe	n_leaf_pages	Number of leaf pages in the index
vidxe	size	Number of pages in the index
vidxf	n_diff_pfx01	f
vidxf	n_diff_pfx02	f,DB_ROW_ID
vidxf	n_leaf_pages	Number of leaf pages in the index
vidxf	size	Number of pages in the index
DROP TABLE t;
