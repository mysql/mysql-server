# Check that (after lots of action) the following is still zero. If it is not,
# then the file that is allocating dynamic memory should be added to the list
# in ut_new_boot().
SELECT count_alloc FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name like 'memory/innodb/other';
