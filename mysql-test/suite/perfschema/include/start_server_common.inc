# Tests for PERFORMANCE_SCHEMA

show databases;

select count(*) from performance_schema.performance_timers;
select count(*) from performance_schema.setup_consumers;
select count(*) > 0 from performance_schema.setup_instruments;
select count(*) from performance_schema.setup_timers;

# Make sure we don't crash, no matter what the starting parameters are

--disable_result_log
select * from performance_schema.cond_instances;
select * from performance_schema.events_waits_current;
select * from performance_schema.events_waits_history;
select * from performance_schema.events_waits_history_long;
select * from performance_schema.events_waits_summary_by_instance;
select * from performance_schema.events_waits_summary_by_thread_by_event_name;
select * from performance_schema.events_waits_summary_global_by_event_name;
select * from performance_schema.file_instances;
select * from performance_schema.file_summary_by_event_name;
select * from performance_schema.file_summary_by_instance;
select * from performance_schema.mutex_instances;
select * from performance_schema.performance_timers;
select * from performance_schema.rwlock_instances;
select * from performance_schema.setup_consumers;
select * from performance_schema.setup_instruments;
select * from performance_schema.setup_timers;
select * from performance_schema.threads;
--enable_result_log

# This has a stable output, printing the result:
show variables like "performance_schema%";

# This has an unrepeatable output, it does depends too much on
# - the platform hardware (sizeof structures, padding)
# - the compiler used (sizeof(enum))
# - the platform header files (sizeof(size_t))
# - the code path in the server (what gets executed and instrumented
#   at runtime)

--disable_result_log
show engine PERFORMANCE_SCHEMA status;
show status like "performance_schema%";
--enable_result_log

