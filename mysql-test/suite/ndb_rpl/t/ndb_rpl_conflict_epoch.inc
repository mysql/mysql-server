#
# Test engine native conflict resolution for ndb
#   NDB$EPOCH() function
#
#
--source include/have_ndb.inc
--source include/have_binlog_format_mixed_or_row.inc
--source suite/ndb_rpl/ndb_master-slave.inc

--echo Setup circular replication
--echo The circle is setup between Primary and Secondary

--disable_query_log
--disable_result_log
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
# Gather port + serverid

select @primary_server_id:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'server_id';
let $PRIMARY_SERVER_ID= query_get_value('select @primary_server_id as v',v,1);
select @primary_server_port:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'port';
let $PRIMARY_SERVER_PORT= query_get_value('select @primary_server_port as v', v,1);

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
# Gather port + serverid
select @secondary_server_id:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'server_id';
let $SECONDARY_SERVER_ID= query_get_value('select @secondary_server_id as v',v,1);
select @secondary_server_port:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'port';
let $SECONDARY_SERVER_PORT= query_get_value('select @secondary_server_port as v', v,1);

# Now set it up
# Stop Slave @ Secondary
STOP REPLICA;
# Clear Secondary Binlog
RESET MASTER;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
# Stop Slave @ Primary 
STOP REPLICA;
# Clear Primary Binlog
RESET MASTER;

--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST="127.0.0.1",SOURCE_PORT=$SECONDARY_SERVER_PORT,SOURCE_USER="root"

# Start Primary replicating from Secondary
START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST="127.0.0.1",SOURCE_PORT=$PRIMARY_SERVER_PORT,SOURCE_USER="root"

# Start Secondary replicating from Primary
START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--enable_query_log
--enable_result_log

--echo Setup ndb_replication and t1$EX exceptions table

--disable_warnings
--disable_query_log
drop table if exists mysql.ndb_replication;
CREATE TABLE mysql.ndb_replication
  (db VARBINARY(63),
   table_name VARBINARY(63),
   server_id INT UNSIGNED,
   binlog_type INT UNSIGNED,
   conflict_fn VARBINARY(128),
   PRIMARY KEY USING HASH (db,table_name,server_id))
  ENGINE=NDB PARTITION BY KEY(db,table_name);
--enable_warnings
--enable_query_log

if (!$CONFLICT_ALG)
{
  let $CONFLICT_ALG=NDB\$EPOCH;
}
if (!$SECONDARY_CONFLICT_ALG)
{
  let $SECONDARY_CONFLICT_ALG=NULL;
}

--echo Populate ndb_replication table as necessary
--echo -- 0 extra gci bits
--disable_query_log
eval replace into mysql.ndb_replication values
  ("test", "t1", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "t1", $PRIMARY_SERVER_ID, 7, "$CONFLICT_ALG(0)");
--enable_query_log

--source suite/ndb_rpl/t/ndb_rpl_conflict_epoch_1.inc

--echo Populate ndb_replication table as necessary
--echo -- 1 extra gci bits
--disable_query_log
eval replace into mysql.ndb_replication values
  ("test", "t1", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "t1", $PRIMARY_SERVER_ID, 7, "$CONFLICT_ALG(1)");
--enable_query_log

--source suite/ndb_rpl/t/ndb_rpl_conflict_epoch_1.inc

--echo Populate ndb_replication table as necessary
--echo -- 31 extra gci bits
--disable_query_log
eval replace into mysql.ndb_replication values
  ("test", "t1", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "t1", $PRIMARY_SERVER_ID, 7, "$CONFLICT_ALG(31)");
--enable_query_log

--source suite/ndb_rpl/t/ndb_rpl_conflict_epoch_1.inc

--echo Populate ndb_replication table as necessary
--echo -- Default extra Gci bits
--disable_query_log
eval replace into mysql.ndb_replication values
  ("test", "t1", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "t1", $PRIMARY_SERVER_ID, 7, "$CONFLICT_ALG()");
--enable_query_log

--source suite/ndb_rpl/t/ndb_rpl_conflict_epoch_1.inc

--source suite/ndb_rpl/t/ndb_conflict_info_init.inc

--echo Now test batched conflict detection/handling
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

create table test.t1 (
  a int primary key,
  b int,
  c varchar(2000)) engine=ndb;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Stop replication from Primary -> Secondary
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Insert a row on Primary
insert into test.t1 values (1,1,repeat('B', 2000));

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Generate a large batch of inserts with early + late conflicts
delimiter %;

create procedure test.doit (`rows` int)
begin
  set @x = 0;
  START TRANSACTION;
    repeat
      insert into test.t1 values (@x, @x, repeat('B', 2000));
      set @x = @x + 1;
    until @x = `rows`
    end repeat;
  COMMIT;
  START TRANSACTION;
    update test.t1 set b=999, c=repeat('E',2000) where a=1;
  COMMIT;
  START TRANSACTION;
    delete from test.t1 where a=1;
  COMMIT;
  START TRANSACTION;
    insert into test.t1 values (1,1,'A');
  COMMIT;
end%

delimiter ;%

call test.doit(100);

drop procedure test.doit;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Look at Primary status, expect 4 conflicts
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Look at Seconday row
select a,b,sha1(c) from test.t1 where a=1;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Check it's the same on the Secondary
select a,b,sha1(c) from test.t1 where a=1;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo Test batching of DELETE vs DELETE with following INSERT
delete from test.t1;
insert into test.t1 values (1, 1, "Ma Brows");
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Stop Slave in both directions
stop slave;
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
stop slave;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Delete row on Primary Cluster
delete from test.t1 where a=1;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Delete row on Secondary Cluster, followed by Insert in later 'batch'
delimiter %;

create procedure test.doit (`rows` int)
begin
  set @x = 2;
  START TRANSACTION;
    delete from test.t1 where a=1;
    repeat
      insert into test.t1 values (@x, @x, repeat('B', 2000));
      set @x = @x + 1;
    until @x = (`rows` + 2)
    end repeat;
  COMMIT;
  START TRANSACTION;
    insert into test.t1 values (1, 1, 'Malleny arms');
  COMMIT;
end%

delimiter ;%

call test.doit(200);

--echo -- Restart slave on Primary Cluster
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
# TODO : Reenable with new slave counts
#--source suite/ndb_rpl/include/ndb_init_replica_counts.inc
start replica;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

# TODO : Reenable with new slave counts
#--disable_result_log
# Don't include results as they vary depending on epoch boundaries
#--source suite/ndb_rpl/include/ndb_slave_counts.inc
#--enable_result_log
#--echo -- Verify that batching occurred when replicating the above
#select (@ndb_slave_execute_count - @ndb_slave_commit_count) > 1;
--echo -- Show data on Primary Cluster (should have row inserted on Secondary)
select * from test.t1 where a=1;

--echo -- Show data on Secondary Cluster (should have row inserted on Secondary)
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
select * from test.t1 where a=1;

--echo --Restart slave on Secondary Cluster
start slave;
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Show data on Clusters after slaves restarted+synced
--echo      For NDB\$EPOCH, the data will be missing
--echo      as expected with delete vs delete conflict
--echo      followed closely by Insert
--echo      For NDB\$EPOCH2, the data will be present on both
--echo      clusters.
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo PRIMARY
select * from test.t1 where a=1;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo SECONDARY
select * from test.t1 where a=1;

--echo -- Force wait for master to be in-sync with slave
--echo    To avoid race between DML and table drop
flush logs;
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

drop procedure test.doit;
drop table test.t1;

delete from mysql.ndb_replication;
eval insert into mysql.ndb_replication values
  ("test", "t3", 0, 7, "$CONFLICT_ALG(32)"),
  ("test", "t4", 0, 7, "$CONFLICT_ALG(-1)");

--disable_query_log
# Only need suppress here, as table creation fails due to this.
call mtr.add_suppression("NDB Replica: .* Too many extra Gci bits at .*");
call mtr.add_suppression(".*engine for 't3' doesn't have this option");
call mtr.add_suppression(".*engine for 't4' doesn't have this option");
call mtr.add_suppression(".*coordinator and worker threads are stopped.*");
--enable_query_log

--error ER_CONFLICT_FN_PARSE_ERROR
create table test.t3 (a int primary key) engine=ndb;
show warnings;

--error ER_CONFLICT_FN_PARSE_ERROR
create table test.t4 (a int primary key) engine=ndb;
show warnings;

--echo -- Force sync before dropping table to avoid race
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
flush logs;
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
flush logs;
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

drop table mysql.ndb_replication;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc

--echo -- Attempt to get system back in pre-test state
# Note use of fixed connection names here.
--connection master
stop replica;
reset replica;

--source include/rpl_end.inc
