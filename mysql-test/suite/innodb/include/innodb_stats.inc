TRUNCATE TABLE test_innodb_stats;

-- eval $insert

ANALYZE TABLE test_innodb_stats;

query_vertical SELECT
stat_name,
stat_value,
sample_size,
stat_description
FROM mysql.innodb_index_stats
WHERE
database_name = DATABASE() AND
table_name = 'test_innodb_stats' AND
index_name = 'a_key' AND
stat_name IN ('n_diff_pfx01', 'n_diff_pfx02', 'n_leaf_pages', 'size')
ORDER BY stat_name;

# provoke reading from persistent storage upon next table access (the SELECT
# from information_schema.statistics below)
FLUSH TABLE test_innodb_stats;

# see what we are going to read
query_vertical SELECT * FROM information_schema.statistics
WHERE table_name = 'test_innodb_stats';
