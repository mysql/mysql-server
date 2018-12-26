--echo #
--echo # Bug #22811659 "ACCESSES TO NEW DATA-DICTIONARY ADD CONFUSING STAGES
--echo #                TO P_S.EVENTS_STAGES_*.".
--echo #
--enable_connect_log

connect (con1, localhost, root, , );

--echo # Obtain @con1_thread_id
let $con1_thread_id=`select thread_id from performance_schema.threads where processlist_id = connection_id()`;
connection default;
--disable_query_log
eval select $con1_thread_id into @con1_thread_id;
--enable_query_log

--echo # Reduce noise from the default connection.
update performance_schema.threads set instrumented = 'NO' where processlist_id = connection_id();

connection con1;
--echo # Create and drop a basic table to populate the DD cache with
--echo # the DD table objects. This also populates global Table
--echo # Definition Cache with TABLE_SHARE objects and con1-specific
--echo # Table Cache with TABLE objects for system tables.
create table t1(a int);
drop table t1;

connection default;
--echo # Reset history table.
truncate table performance_schema.events_stages_history_long;

connection con1;
--echo # Run some statements and see that we have no additional stage
--echo # events from opening and locking system tables.
--echo # Disable prepared statements since they cause extra attempts to open
--echo # tables.
--disable_ps_protocol
create table t1(a int);
insert into t1 values (1), (2), (3), (4), (5);
drop table t1;
--enable_ps_protocol

connection default;
--echo # Checking the expected number of stages history entries:
select event_name from performance_schema.events_stages_history_long
  where thread_id = @con1_thread_id and
        event_name like '%Opening %tables' or
        event_name like '%Locking system tables' or
        event_name like '%System lock';

connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc

connection default;
--echo # Clean-up.
update performance_schema.threads set instrumented = 'YES' where processlist_id = connection_id();
--disable_connect_log
