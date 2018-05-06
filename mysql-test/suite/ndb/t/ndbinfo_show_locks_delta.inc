# See ndbinfo_locks_per_fragment.test
--disable_query_log
--disable_result_log
# Empty 'current' snapshot
delete from current;
# Reload 'current' snapshot
insert into current select * from frag_locks; 
--enable_result_log

# Show diff from baseline
select * from baseline_diff;

--disable_result_log
# Reset baseline
delete from baseline; 
insert into baseline select * from current;
--enable_result_log
--enable_query_log
