# Wait until INNODB_CACHED_INDEXES agrees with a buffer-pool walk.
#
# Requires:
# - $cached_index_pages_table: INNODB_TABLES.NAME, for example test/sbtest1
# - $cached_index_pages_index: index name, for example k_1
# - $cached_index_pages_checkpoint: human-readable checkpoint name
# - $expected_non_leaf_pages: exact number of non-leaf index pages expected to
#                            be visible in the buffer-pool walk
#
--let $wait_condition= SELECT count_index_pages_in_lru('$cached_index_pages_table', '$cached_index_pages_index') - get_n_cached_pages('$cached_index_pages_table', '$cached_index_pages_index') = $expected_non_leaf_pages
--source include/wait_condition.inc

if (!$success)
{
  --echo # Cached-index accounting mismatch after $cached_index_pages_checkpoint:
  --eval SELECT get_n_cached_pages('$cached_index_pages_table', '$cached_index_pages_index') AS cached_pages, count_index_pages_in_lru('$cached_index_pages_table', '$cached_index_pages_index') AS buffer_pages, $expected_non_leaf_pages AS expected_non_leaf_pages
}
