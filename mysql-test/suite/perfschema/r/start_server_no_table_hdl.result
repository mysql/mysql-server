show databases;
Database
information_schema
mtr
mysql
performance_schema
sys
test
select count(*) from performance_schema.performance_timers;
count(*)
4
select count(*) from performance_schema.setup_consumers;
count(*)
15
select count(*) > 4 from performance_schema.setup_instruments;
count(*) > 4
1
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
show engine PERFORMANCE_SCHEMA status;
show global status like "performance_schema%";
show global variables like "performance_schema_max_table_instances";
Variable_name	Value
performance_schema_max_table_instances	-1
show global variables like "performance_schema_max_table_handles";
Variable_name	Value
performance_schema_max_table_handles	0
drop table if exists test.instrument_me;
create table test.instrument_me(a int);
insert into test.instrument_me values (1), (2), (3);
select * from test.instrument_me;
a
1
2
3
drop table test.instrument_me;
show global variables like "performance_schema_max_table_instances";
Variable_name	Value
performance_schema_max_table_instances	-1
select variable_value > 0 from performance_schema.global_status
where variable_name like 'PERFORMANCE_SCHEMA_TABLE_HANDLES_LOST';
variable_value > 0
1
