drop table if exists t1;
drop table if exists t2;
create table t1 (test_name text);
create table t2 (test_name text);
load data infile "<MYSQLTEST_VARDIR>/tmp/perfschema-all_tests.txt" into table t1;
insert into t2 select concat('ddl_', table_name, '.test') from information_schema.tables
where table_schema='performance_schema';
insert into t2 select concat('dml_', table_name, '.test') from information_schema.tables
where table_schema='performance_schema';
insert into t2 select concat('idx_', table_name, '.test') from information_schema.tables
where table_schema='performance_schema';
update t2 set test_name= replace(test_name, "events_waits_summary_", "ews_");
update t2 set test_name= replace(test_name, "events_stages_summary_", "esgs_");
update t2 set test_name= replace(test_name, "events_statements_histogram_", "esmh_");
update t2 set test_name= replace(test_name, "events_statements_summary_", "esms_");
update t2 set test_name= replace(test_name, "events_transactions_summary_", "ets_");
update t2 set test_name= replace(test_name, "events_errors_summary_", "ees_");
update t2 set test_name= replace(test_name, "file_summary_", "fs_");
update t2 set test_name= replace(test_name, "objects_summary_", "os_");
update t2 set test_name= replace(test_name, "table_io_waits_summary_", "tiws_");
update t2 set test_name= replace(test_name, "table_lock_waits_summary_", "tlws_");
update t2 set test_name= replace(test_name, "memory_summary_", "mems_");
update t2 set test_name= replace(test_name, "user_variables_", "uvar_");
delete from t2 where t2.test_name in (select t1.test_name from t1);
select test_name as `MISSING DDL/DML TESTS` from t2;
MISSING DDL/DML TESTS
ddl_replication_asynchronous_connection_failover.test
ddl_replication_asynchronous_connection_failover_managed.test
dml_replication_asynchronous_connection_failover.test
dml_replication_asynchronous_connection_failover_managed.test
idx_binary_log_transaction_compression_stats.test
idx_log_status.test
idx_replication_applier_filters.test
idx_replication_applier_global_filters.test
idx_replication_asynchronous_connection_failover.test
idx_replication_asynchronous_connection_failover_managed.test
idx_user_defined_functions.test
drop table t1;
drop table t2;
