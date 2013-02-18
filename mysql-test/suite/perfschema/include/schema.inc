# Tests for PERFORMANCE_SCHEMA
#    Show existing objects and information about their structure

show databases like 'performance_schema';
show create database performance_schema;

use performance_schema;

show tables;

show create table accounts;
show create table cond_instances;
show create table events_stages_current;
show create table events_stages_history;
show create table events_stages_history_long;
show create table events_stages_summary_by_host_by_event_name;
show create table events_stages_summary_by_thread_by_event_name;
show create table events_stages_summary_by_user_by_event_name;
show create table events_stages_summary_by_account_by_event_name;
show create table events_stages_summary_global_by_event_name;
show create table events_statements_current;
show create table events_statements_history;
show create table events_statements_history_long;
show create table events_statements_summary_by_digest;
show create table events_statements_summary_by_host_by_event_name;
show create table events_statements_summary_by_thread_by_event_name;
show create table events_statements_summary_by_user_by_event_name;
show create table events_statements_summary_by_account_by_event_name;
show create table events_statements_summary_global_by_event_name;
show create table events_waits_current;
show create table events_waits_history;
show create table events_waits_history_long;
show create table events_waits_summary_by_host_by_event_name;
show create table events_waits_summary_by_instance;
show create table events_waits_summary_by_thread_by_event_name;
show create table events_waits_summary_by_user_by_event_name;
show create table events_waits_summary_by_account_by_event_name;
show create table events_waits_summary_global_by_event_name;
show create table file_instances;
show create table file_summary_by_event_name;
show create table file_summary_by_instance;
show create table host_cache;
show create table hosts;
show create table mutex_instances;
show create table objects_summary_global_by_type;
show create table performance_timers;
show create table rwlock_instances;
show create table setup_actors;
show create table setup_consumers;
show create table setup_instruments;
show create table setup_objects;
show create table setup_timers;
show create table socket_instances;
show create table socket_summary_by_instance;
show create table socket_summary_by_event_name;
show create table table_io_waits_summary_by_index_usage;
show create table table_io_waits_summary_by_table;
show create table table_lock_waits_summary_by_table;
show create table threads;
show create table users;

