#
# Include file for testing ndb$epoch and ndb$epoch2 conflict functions
#
#

if ($extended_exceptions_table)
{
call mtr.add_suppression("NDB Slave: exceptions table .* has suspicious definition .*");
}

if (!$extended_exceptions_table)
{
create table `test`.`t1$EX`
  (server_id int unsigned,
   master_server_id int unsigned,
   master_epoch bigint unsigned,
   count int unsigned,
   a int not null,
   d int,
   primary key(server_id, master_server_id, master_epoch, count)) engine ndb;
}
if ($extended_exceptions_table)
{
create table `test`.`t1$EX`
  (ndb$server_id int unsigned,
   ndb$master_server_id int unsigned,
   ndb$master_epoch bigint unsigned,
   ndb$count int unsigned,
   a int not null,
   b$old varchar(255),
   b$new varchar(255),
   d int,
   ndb$op_type           enum('write_row','update_row', 'delete_row') not null,
   ndb$cft_cause         enum('row_does_not_exist','row_already_exists','data_in_conflict','trans_in_conflict') not null,
   ndb$orig_transid      bigint unsigned not null,
   primary key(ndb$server_id, ndb$master_server_id, ndb$master_epoch, ndb$count)) engine ndb;
}

--echo Create table
create table test.t1(a int primary key, b varchar(255)) engine = ndb;

--echo Create other table
create table test.t2(a int primary key, b int) engine = ndb;

--source suite/ndb_rpl/t/ndb_conflict_info_init.inc

--echo ----------------------------------
--echo Test 1 : Basic two-way replication
--echo ----------------------------------

insert into test.t1 values (1, "Metropole");

# Here we want to verify that updates can originate at the Secondary without
# causing conflicts
# We first insert on the Primary, allow that to propagate, then update on 
# the secondary.
# This is much complicated by the fact that we run with varying number of
# extra GCI bits, from 0 to 31.  With 0 extra GCI bits, the row's GCI appears
# to be GCIhi|0xffffffff, which means that all secondary initiated changes
# will be considered in-conflict until the MaxReplicatedEpoch hits GCIhi+1|0
# To make this test stable, we use a loop to check for this condition!

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
FLUSH LOGS;
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Verify that Max rep epoch has been updated

--disable_query_log
--disable_result_log
SELECT @MaxRepGCIHI:=Variable_Value >> 32
FROM performance_schema.global_status
WHERE Variable_Name like 'Ndb_slave_max_replicated_epoch';

SELECT @MaxRepGCIHI = 0;

--let $first_gcihi = `SELECT @MaxRepGCIHI`
--let $current_gcihi = $first_gcihi
--let $different = `SELECT $current_gcihi - $first_gcihi`

#--echo Difference is $different

while (!$different)
{
  # Do something, sleep a little, and see if we've ticked over GCIhi yet
  replace into test.t2 values(2,1);
  replace into test.t2 values(2,2);
  --sleep 1

  SELECT @MaxRepGCIHI:=Variable_Value >> 32
  FROM performance_schema.global_status
  WHERE Variable_Name like 'Ndb_slave_max_replicated_epoch';

  --let $current_gcihi = `SELECT @MaxRepGCIHI`
  --let $different = `SELECT $current_gcihi - $first_gcihi`
  #--echo Current GCI Hi is $current_gcihi
  #--echo Difference is $different
}
--enable_result_log
--enable_query_log

--echo -- Primary Cluster Max Rep Epoch now beyond the original update epoch

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Now update data on slave
update test.t1 set b="Favorit" where a=1;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Now check data on Primary
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_conflict_info.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Now perform multiple consecutive updates on the Secondary

update test.t1 set b="Elephant house" where a=1;
update test.t1 set b="Beach house" where a=1;

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Now check they've applied on the Primary
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo --------------------------------------------
--echo Test 2 : Normal Insert from Secondary Master
--echo --------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Insert a new row on the Secondary
insert into test.t1 values (2, "Forrest");

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Check it exists on the Primary
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Update from the Secondary
update test.t1 set b="Reds" where a=2;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
delete from test.t1 where a=2;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_conflict_info.inc

# Allow reflected Ndb$Epoch2 ops to propagate back to
# Secondary before starting the next test.
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc


--echo -------------------------------
--echo Test 3 : Insert-Insert conflict
--echo -------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
stop slave;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Insert a row on the Primary Master
insert into test.t1 values (2, "Loopy Lornas");

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Insert a row on the secondary Master
insert into test.t1 values (2, "Cloisters");

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Examine data on Primary Master (should be unaffected)
select * from test.t1 order by a;

--echo -- Examine conflict indicators on Primary Master
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo -- Examine data on isolated secondary Master (should be as-set)
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
select * from test.t1 order by a;

--echo -- Restart secondary Masters slave
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Reexamine secondary Master's data (should be same as Primary Masters)
select * from test.t1 order by a;

--echo -------------------------------
--echo Test 4 : Update-Update conflict
--echo -------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Stop replication to secondary master
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Update row on Primary Master

update test.t1 set b="Peters Yard" where a=2;

--echo -- Show data on Primary Master

select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Update row on Secondary Master
update test.t1 set b="Toast" where a=2;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Examine data on Primary Master - should be unaffected

select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Check data on secondary - should be as set

select * from test.t1 order by a;

--echo -- Now restart slave, will re-align row
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Check that Secondary is re-aligned

select * from test.t1 order by a;


--echo -------------------------------
--echo Test 5 : Update-Delete conflict
--echo -------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Stop Secondary slave
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Update on Primary Master

update test.t1 set b="Pear tree" where a = 2;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Delete on Secondary Master

delete from test.t1 where a = 2;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Examine data on Primary Master, should be unaffected

select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo -- Examine data on Secondary Master before slave restart, still missing
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

select * from test.t1 order by a;

--echo -- Restart Secondary Master slave
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Examine data on Secondary Master after slave restart, aligned with Master
select * from test.t1 order by a;

--echo -------------------------------
--echo Test 6 : Delete-Update conflict
--echo -------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Stop Secondary slave
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Delete on Primary Master

delete from test.t1 where a=2;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Update on Secondary Master

update test.t1 set b="Black pig" where a=2;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Examine data on Primary Master, should be unaffected (no row)

select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo -- Examine data on Secondary Master before slave restart, should be as inserted
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
select * from test.t1 order by a;

--echo -- Restart Secondary Master slave
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Examine data on Secondary Master after slave restart, aligned with Master (deleted)
select * from test.t1 order by a;

--echo -------------------------------
--echo Test 7 : Delete-Delete conflict
--echo -------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Stop Secondary slave
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Delete on Primary Master

delete from test.t1 where a=1;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Delete on Secondary Master

delete from test.t1 where a=1;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Examine data on Primary Master, no row

select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo -- Examine data on Secondary Master before slave restart, no row
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
select * from test.t1 order by a;

--echo -- Restart Secondary Master slave
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Examine data on Secondary Master after slave restart, no row
select * from test.t1 order by a;


--echo ------------------------------------------------
--echo Test 8 : Delete-Delete, Insert conflict exposure
--echo ------------------------------------------------
# NDB$EPOCH
#   Data will diverge :
#   This occurs as the Primary Master's Delete is still
#   in-flight when the Secondary Master's Insert arrives,
#   but as there is no knowledge of this at the Primary
#   Master, it accepts the Insert.
#
# NDB$EPOCH2
#   Data will not diverge :
#   As above, except the Secondary Master's Insert will be
#   reflected back to it, and applied.
#   So the (eventual) result will be equal data.
#

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Insert a row on Secondary Master
insert into test.t1 values (3, "Espy");
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Check it's present on Primary Master
select * from test.t1 order by a;

--echo -- Stop replication in both directions
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
stop slave;

--echo -- Delete row from both clusters
delete from test.t1 where a=3;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
delete from test.t1 where a=3;

--echo -- Follow up with Insert from Secondary master
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

insert into test.t1 values (3, "Dalriada");

--echo -- Restart replication in both directions
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
start slave;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
#
# For Ndb$EPOCH2, Primary will reflect Secondary Insert
# back to it - we must wait for that to occur before
# expecting consistency
#
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Check data on both sites
--echo --   diverged with NDB\$EPOCH
--echo --   consistent with NDB\$EPOCH2 
--echo -- Secondary master :
select * from test.t1 order by a;
--echo -- Primary master :
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
select * from test.t1 order by a;

--echo --Remove extra row
delete from test.t1 where a=3;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Note that Delete-Delete conflict detected below
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo ------------------------------------------------
--echo Test 9 : Insert, Insert-Update-Delete conflict
--echo ------------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Stop replication on Secondary Master
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Insert row on Primary Master
insert into test.t1 values (4, "Haymarket");

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Insert row on Secondary Master
insert into test.t1 values (4, "Outhouse");
--echo -- Update row on Secondary Master
update test.t1 set b="Mathers" where a = 4;
--echo -- Delete row on Secondary Master
delete from test.t1 where a=4;

--echo -- Examine data (none) on Secondary Master
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Examine data on Primary Master, should be unaffected

select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Examine data on Secondary Master (none)
select * from test.t1 order by a;

--echo -- Restart Secondary Master's slave
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Check data on Secondary Master, should be same as Primary Master
select * from test.t1;

--echo ------------------------------------------------
--echo Test 10 : Update, Delete-Insert-Update conflict
--echo ------------------------------------------------
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Stop replication on Secondary Master
stop slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo -- Update row on Primary Master
update test.t1 set b="Blind poet" where a=4;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Delete row on Secondary Master
delete from test.t1 where a=4;

--echo -- Insert row on Secondary Master
insert into test.t1 values (4, "Drouthy Neebors");

--echo -- Update row on Secondary Master
update test.t1 set b="The Tankard" where a=4;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo -- Check data on Primary Master, should be unaffected
select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo -- Check data on Secondary Master, as set

select * from test.t1 order by a;

--echo -- Restart Secondary Master slave
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo -- Check data on Secondary Master - should be as Primary
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo ------------------------------------------------------------------------
--echo Test 11 : Test Secondary insert-update-delete accepted
--echo ------------------------------------------------------------------------
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo Insert row on Secondary
insert into test.t1 values (5, "Minders");
--echo Update row on Secondary
update test.t1 set b="Southsider" where a=5;
--echo Delete row on Secondary
delete from test.t1 where a=5;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Check data on Primary, should be none.  No new conflicts
select * from test.t1 order by a;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

# Test runs with --log-slave-updates=1
# This means that the Secondary logs reflected ops if they
# take some effect in its binlog
# (e.g. they go S->P --reflect--> S --logged--> P)
# On the final return to P they are logged with the P server
# id, so won't be applied.
# BUT the table_map events are logged with the logging server
# id (e.g. S in this case), and will be processed by the
# slave SQL thread.
# If e.g. the table has been dropped on the PRIMARY then 
# this can cause a slave failure...
# In real life I guess that table drop is rare.
# For this testcase, we'll do some more serialisation between
# the last writes to a table, and the drop of the table
#
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc

# Show nothing changed in the meantime
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_conflict_info.inc

# Now drop
drop table test.t1;
drop table test.t2;
drop table test.t1$EX;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
