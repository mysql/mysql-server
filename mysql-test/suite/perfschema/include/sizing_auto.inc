
show variables like "table_definition_cache";
show variables like "table_open_cache";
show variables like "max_connections";
# open_files_limit depends on OS configuration (ulimit -n)
#show variables like "open_files_limit";
show variables like "%performance_schema%";
show status like "%performance_schema%";

# Each test script should provide a different test.cnf file,
# with different settings.
# This output will show the sizes computed automatically.
# Note that this output is very dependent on the platform.

show engine performance_schema status;

--vertical_results
select * from performance_schema.memory_summary_global_by_event_name
  where event_name='memory/performance_schema/internal_buffers';
