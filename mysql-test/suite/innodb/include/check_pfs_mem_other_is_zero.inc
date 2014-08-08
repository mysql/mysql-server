# Check that (after lots of action) the following is still zero. If it is not,
# then the file that is allocating dynamic memory should be added to the list
# in ut_new_boot().

-- echo count_alloc WHERE event_name like 'memory/innodb/other'

-- let pfs_enabled = `SELECT COUNT(*) FROM information_schema.engines WHERE support IN ('YES', 'DEFAULT') AND engine = 'PERFORMANCE_SCHEMA'`

-- disable_query_log

if ($pfs_enabled) {
	SELECT count_alloc FROM performance_schema.memory_summary_global_by_event_name
	WHERE event_name = 'memory/innodb/other';
}

if (!$pfs_enabled) {
	SELECT 0 AS count_alloc;
}

-- enable_query_log
