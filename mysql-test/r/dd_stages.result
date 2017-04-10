#
# Bug #22811659 "ACCESSES TO NEW DATA-DICTIONARY ADD CONFUSING STAGES
#                TO P_S.EVENTS_STAGES_*.".
#
connect  con1, localhost, root, , ;
# Obtain @con1_thread_id
connection default;
# Reduce noise from the default connection.
update performance_schema.threads set instrumented = 'NO' where processlist_id = connection_id();
connection con1;
# Create and drop a basic table to populate the DD cache with
# the DD table objects. This also populates global Table
# Definition Cache with TABLE_SHARE objects and con1-specific
# Table Cache with TABLE objects for system tables.
create table t1(a int);
drop table t1;
connection default;
# Reset history table.
truncate table performance_schema.events_stages_history_long;
connection con1;
# Run some statements and see that we have no additional stage
# events from opening and locking system tables.
# Disable prepared statements since they cause extra attempts to open
# tables.
create table t1(a int);
insert into t1 values (1), (2), (3), (4), (5);
drop table t1;
connection default;
# Checking the expected number of stages history entries:
select event_name from performance_schema.events_stages_history_long
where thread_id = @con1_thread_id and
event_name like '%Opening %tables' or
event_name like '%Locking system tables' or
event_name like '%System lock';
event_name
stage/sql/Opening tables
stage/sql/Opening tables
stage/sql/System lock
connection con1;
disconnect con1;
connection default;
# Clean-up.
update performance_schema.threads set instrumented = 'YES' where processlist_id = connection_id();
