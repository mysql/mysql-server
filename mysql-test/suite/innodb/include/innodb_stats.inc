-- disable_query_log
DELETE FROM test_innodb_stats;

-- enable_query_log
-- eval $insert

-- disable_query_log
-- disable_result_log
ANALYZE TABLE test_innodb_stats;

-- enable_result_log
SELECT
stat_name,
stat_value,
sample_size,
stat_description
FROM innodb.index_stats
WHERE
database_name = DATABASE() AND
table_name = 'test_innodb_stats' AND
index_name = 'a_key' AND
stat_name IN ('n_diff_pfx01', 'n_diff_pfx02', 'n_leaf_pages', 'size')
ORDER BY stat_name;
