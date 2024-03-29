# Tests for PERFORMANCE_SCHEMA

show databases;

select count(*) from performance_schema.performance_timers;
select count(*) from performance_schema.setup_consumers;
# wait/io/table/sql/handler is a native instrument
# wait/lock/table/sql/handler is a native instrument
# idle/io/socket is a native instrument
# wait/lock/metadata is a native instrument
select count(*) > 4 from performance_schema.setup_instruments;

# Make sure we don't crash, no matter what the starting parameters are

--disable_result_log
select * from performance_schema.accounts;
select * from performance_schema.cond_instances;
select * from performance_schema.events_stages_current;
select * from performance_schema.events_stages_history;
select * from performance_schema.events_stages_history_long;
select * from performance_schema.events_stages_summary_by_account_by_event_name;
select * from performance_schema.events_stages_summary_by_host_by_event_name;
select * from performance_schema.events_stages_summary_by_thread_by_event_name;
select * from performance_schema.events_stages_summary_by_user_by_event_name;
select * from performance_schema.events_stages_summary_global_by_event_name;
select * from performance_schema.events_statements_current;
select * from performance_schema.events_statements_histogram_by_digest;
select * from performance_schema.events_statements_histogram_global;
select * from performance_schema.events_statements_history;
select * from performance_schema.events_statements_history_long;
select * from performance_schema.events_statements_summary_by_account_by_event_name;
select * from performance_schema.events_statements_summary_by_digest;
select * from performance_schema.events_statements_summary_by_host_by_event_name;
select * from performance_schema.events_statements_summary_by_thread_by_event_name;
select * from performance_schema.events_statements_summary_by_user_by_event_name;
select * from performance_schema.events_statements_summary_global_by_event_name;
select * from performance_schema.events_transactions_current;
select * from performance_schema.events_transactions_history;
select * from performance_schema.events_transactions_history_long;
select * from performance_schema.events_transactions_summary_by_account_by_event_name;
select * from performance_schema.events_transactions_summary_by_host_by_event_name;
select * from performance_schema.events_transactions_summary_by_thread_by_event_name;
select * from performance_schema.events_transactions_summary_by_user_by_event_name;
select * from performance_schema.events_transactions_summary_global_by_event_name;
select * from performance_schema.events_waits_current;
select * from performance_schema.events_waits_history;
select * from performance_schema.events_waits_history_long;
select * from performance_schema.events_waits_summary_by_account_by_event_name;
select * from performance_schema.events_waits_summary_by_host_by_event_name;
select * from performance_schema.events_waits_summary_by_instance;
select * from performance_schema.events_waits_summary_by_thread_by_event_name;
select * from performance_schema.events_waits_summary_by_user_by_event_name;
select * from performance_schema.events_waits_summary_global_by_event_name;
select * from performance_schema.memory_summary_by_account_by_event_name;
select * from performance_schema.memory_summary_by_host_by_event_name;
select * from performance_schema.memory_summary_by_thread_by_event_name;
select * from performance_schema.memory_summary_by_user_by_event_name;
select * from performance_schema.memory_summary_global_by_event_name;
select * from performance_schema.events_errors_summary_by_account_by_error;
select * from performance_schema.events_errors_summary_by_host_by_error;
select * from performance_schema.events_errors_summary_by_thread_by_error;
select * from performance_schema.events_errors_summary_by_user_by_error;
select * from performance_schema.events_errors_summary_global_by_error;
select * from performance_schema.file_instances;
select * from performance_schema.file_summary_by_event_name;
select * from performance_schema.file_summary_by_instance;
select * from performance_schema.host_cache;
select * from performance_schema.hosts;
select * from performance_schema.memory_summary_by_account_by_event_name;
select * from performance_schema.memory_summary_by_host_by_event_name;
select * from performance_schema.memory_summary_by_thread_by_event_name;
select * from performance_schema.memory_summary_by_user_by_event_name;
select * from performance_schema.memory_summary_global_by_event_name;
select * from performance_schema.metadata_locks;
select * from performance_schema.mutex_instances;
select * from performance_schema.objects_summary_global_by_type;
select * from performance_schema.performance_timers;
select * from performance_schema.rwlock_instances;
select * from performance_schema.data_locks;
select * from performance_schema.data_lock_waits;
select * from performance_schema.session_account_connect_attrs;
select * from performance_schema.session_connect_attrs;
select * from performance_schema.setup_actors;
select * from performance_schema.setup_consumers;
select * from performance_schema.setup_instruments;
select * from performance_schema.setup_objects;
select * from performance_schema.setup_threads;
select * from performance_schema.socket_instances;
select * from performance_schema.socket_summary_by_instance;
select * from performance_schema.socket_summary_by_event_name;
select * from performance_schema.table_handles;
select * from performance_schema.table_io_waits_summary_by_index_usage;
select * from performance_schema.table_io_waits_summary_by_table;
select * from performance_schema.table_lock_waits_summary_by_table;
select * from performance_schema.threads;
select * from performance_schema.users;
select * from performance_schema.replication_connection_configuration;
select * from performance_schema.replication_connection_status;
select * from performance_schema.replication_applier_configuration;
select * from performance_schema.replication_applier_status;
select * from performance_schema.replication_applier_status_by_coordinator;
select * from performance_schema.replication_applier_status_by_worker;
select * from performance_schema.global_status;
select * from performance_schema.status_by_thread;
select * from performance_schema.status_by_user;
select * from performance_schema.status_by_host;
select * from performance_schema.status_by_account;
select * from performance_schema.session_status;
select * from performance_schema.global_variables;
select * from performance_schema.variables_by_thread;
select * from performance_schema.session_variables;
--enable_result_log

# This has an unrepeatable output, it does depends too much on
# - the platform hardware (sizeof structures, padding)
# - the compiler used (sizeof(enum))
# - the platform header files (sizeof(size_t))
# - the code path in the server (what gets executed and instrumented
#   at runtime)

--disable_result_log
show engine PERFORMANCE_SCHEMA status;
show global status like "performance_schema%";
--enable_result_log

