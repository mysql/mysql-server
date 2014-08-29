SET GLOBAL innodb_stats_persistent_sample_pages=17;
CREATE TABLE test_ps_sample_pages_used (
a VARCHAR(512), PRIMARY KEY (a)
) ENGINE=INNODB STATS_SAMPLE_PAGES=default;
BEGIN;
COMMIT;
ANALYZE TABLE test_ps_sample_pages_used;
Table	Op	Msg_type	Msg_text
test.test_ps_sample_pages_used	analyze	status	OK
SELECT stat_name, stat_value FROM mysql.innodb_index_stats
WHERE table_name='test_ps_sample_pages_used' AND stat_name='n_leaf_pages';
stat_name	stat_value
n_leaf_pages	37
SELECT sample_size FROM mysql.innodb_index_stats
WHERE table_name='test_ps_sample_pages_used' AND stat_name='n_diff_pfx01';
sample_size
17
ALTER TABLE test_ps_sample_pages_used STATS_SAMPLE_PAGES=14;
ANALYZE TABLE test_ps_sample_pages_used;
Table	Op	Msg_type	Msg_text
test.test_ps_sample_pages_used	analyze	status	OK
SELECT sample_size FROM mysql.innodb_index_stats
WHERE table_name='test_ps_sample_pages_used' AND stat_name='n_diff_pfx01';
sample_size
14
DROP TABLE test_ps_sample_pages_used;
SET GLOBAL innodb_stats_persistent_sample_pages=default;
