#
# Test engine native conflict resolution for ndb
#   NDB$EPOCH[2]_TRANS() function
#
#
--source include/have_ndb.inc
--source include/have_binlog_format_mixed_or_row.inc
--source suite/ndb_rpl/ndb_rpl_init_source_replica.inc
--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--echo Setup circular replication
--disable_query_log
--disable_result_log
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
RESET BINARY LOGS AND GTIDS;
STOP REPLICA;
select @secondary_server_id:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'server_id';
let $SECONDARY_SERVER_ID= query_get_value('select @secondary_server_id as v',v,1);
select @secondary_port:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'port';
let $SECONDARY_PORT= query_get_value('select @secondary_port as v',v,1);

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
RESET BINARY LOGS AND GTIDS;
STOP REPLICA;
--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST="127.0.0.1",SOURCE_PORT=$SECONDARY_PORT,SOURCE_USER="root"
START REPLICA;
select @primary_server_id:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'server_id';
let $PRIMARY_SERVER_ID= query_get_value('select @primary_server_id as v',v,1);
select @primary_port:=(variable_value+0)
       from performance_schema.global_variables
       where variable_name like 'port';
let $PRIMARY_PORT= query_get_value('select @primary_port as v',v,1);

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST="127.0.0.1",SOURCE_PORT=$PRIMARY_PORT,SOURCE_USER="root"
START REPLICA;

--enable_result_log
--enable_query_log

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Setup ndb_replication and t1$EX exceptions table

--disable_warnings
--disable_query_log
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
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

if (!$PRIMARY_CONFLICT_ALG)
{
  let $PRIMARY_CONFLICT_ALG=\"NDB\$EPOCH_TRANS()\";
}
if (!$SECONDARY_CONFLICT_ALG)
{
  let $SECONDARY_CONFLICT_ALG=NULL;
}

--echo Populate ndb_replication table as necessary
eval replace into mysql.ndb_replication values
  ("test", "t1", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "t1", $PRIMARY_SERVER_ID, 7, $PRIMARY_CONFLICT_ALG);
eval replace into mysql.ndb_replication values
  ("test", "t2", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "t2", $PRIMARY_SERVER_ID, 7, $PRIMARY_CONFLICT_ALG);

create table test.t1 (
  a int primary key,
  b varchar(2000)) engine=ndb;

create table test.t2 (
  a int primary key,
  b varchar(2000)) engine=ndb;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Add some data
insert into test.t1 values
 (1, "Initial data 1"),
 (2, "Initial data 2"),
 (3, "Initial data 3"),
 (4, "Initial data 4"),
 (5, "Initial data 5"),
 (6, "Initial data 6"),
 (7, "Initial data 7"),
 (8, "Initial data 8"),
 (9, "Initial data 9"),
 (10, "Initial data 10");

--echo Show basic row-level conflict detection
--echo ---------------------------------------
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

update t1 set b="Primary first change 2" where a=2;
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

update t1 set b="Secondary first change 2" where a=2;
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Primary should have rejected change from Secondary, keeping its value

select * from t1 order by a;

--source suite/ndb_rpl/t/ndb_trans_conflict_info.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should have been realigned to Primary

select * from t1 order by a;

--echo Show rollback of whole secondary transaction
--echo --------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
update t1 set b="Primary second change 4" where a=4;

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
begin;
update t1 set b="Secondary second change 4" where a=4;
update t1 set b="Secondary second change 5" where a=5;
commit;

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Primary should have rejected secondary changes on both rows
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_trans_conflict_info.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should have been realigned to Primary
select * from test.t1 order by a;

--echo Show rollback of dependent transaction as well
--echo ----------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
update t1 set b="Primary third change 1" where a=1;

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

begin;
update t1 set b="Secondary third change 3" where a=3;
update t1 set b="Secondary third change 1" where a=1; # Conflict here
commit;
begin;
update t1 set b="Secondary fourth change 3" where a=3; # Dependency on conflict here
insert into t1 values (11,"Secondary fourth change 11");
commit;

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Primary should have rejected all secondary changes
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_trans_conflict_info_stable.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should have been realigned to Primary

select * from test.t1 order by a;


--echo Show rollback of dependent transaction across different tables
--echo --------------------------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

update t1 set b="Primary fifth change 6" where a=6;

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

begin;
update t1 set b="Secondary fifth change 6" where a=6; # Conflict row
insert into t2 values (1, "Secondary fifth change 1");
insert into t2 values (2, "Secondary fifth change 2");
commit;
begin;
update t2 set b="Secondary sixth change 1" where a=2; # Dependent row
insert into t2 values (3, "Secondary sixth change 2");
commit;

select * from test.t1 order by a;
select * from test.t2 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Primary should have rejected all secondary changes
select * from test.t1 order by a;
select * from test.t2 order by a;

--source suite/ndb_rpl/t/ndb_trans_conflict_info_stable.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should have been realigned to primary
select * from test.t1 order by a;
select * from test.t2 order by a;

--echo Show that whole epoch is not rolled back
--echo ----------------------------------------
# Whole epoch is rolled back when --ndb-serverid-transid-bits is 0!

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
update t1 set b="Primary is king" where a=10;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
begin;
update t1 set b="Secondary is emperor" where a=10;
insert into t1 values (11, "Secondary is pleni-potentiary");
commit;

begin;
insert into t1 values (12, "Secondary ruled once");
insert into t1 values (13, "This history will not be lost");
insert into t1 values (14, "Look on my works ye mighty, and despair");
commit;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Primary should have rejected conflicting trans (emperor, pleni-potentiary)
--echo but accepted unrelated trans (history)

select * from t1 order by a;

--source suite/ndb_rpl/t/ndb_trans_conflict_info.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should be aligned with Primary

select * from t1 order by a;


--echo Show that non-conflicting ancestors are not implicated
--echo ------------------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
update t1 set b="7 : Primary is king" where a=7;

--echo Primary state
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

# 'Innocent' secondary transaction
begin;
update t1 set b="8 : Secondary innocent" where a=8;
update t1 set b="9 : Secondary innocent" where a=9;
commit;

--echo Secondary with innocent
select * from test.t1 order by a;

# 'Guilty secondary transaction, affecting one of the same rows as innocent
begin;
update t1 set b="9 : Secondary guilty" where a=9; # Dependency on innocent
update t1 set b="7 : Secondary guilty" where a=7; # Conflict row
commit;

--echo Secondary with guilty overlaid
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Primary cluster should have rejected 'guilty' secondary transaction, but
--echo accepted 'innocent' secondary transaction.

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_trans_conflict_info.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary cluster should be realigned with Primary

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Classic banking example
--echo -----------------------

eval replace into mysql.ndb_replication values
  ("test", "balances", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "balances", $PRIMARY_SERVER_ID, 7, $PRIMARY_CONFLICT_ALG);

# Transactions table may not need conflict-detection?
eval replace into mysql.ndb_replication values
  ("test", "transactions", $SECONDARY_SERVER_ID, 7, $SECONDARY_CONFLICT_ALG),
  ("test", "transactions", $PRIMARY_SERVER_ID, 7, $PRIMARY_CONFLICT_ALG);

create table test.balances
(name     varchar(100) primary key,
 balance  int) engine=ndb;

if (!$extended_exceptions_table)
{
create table test.transactions$EX
 (server_id             int unsigned,
  master_server_id      int unsigned,
  master_epoch          bigint unsigned,
  count                 int unsigned,
  auto_key              int not null,
  from_name             varchar(100) not null,
  to_name               varchar(100) not null,
  detail                varchar(100) not null,
  primary key(server_id, master_server_id, master_epoch, count))
engine=ndb;
}

if ($extended_exceptions_table)
{
create table test.transactions$EX
 (ndb$server_id         int unsigned,
  ndb$master_server_id  int unsigned,
  ndb$master_epoch      bigint unsigned,
  ndb$count             int unsigned,
  auto_key              int not null,
  from_name             varchar(100) not null,
  to_name               varchar(100) not null,
  detail                varchar(100) not null,
  amount$old            int,
  amount$new            int,
  ndb$op_type           enum('write_row','update_row', 'delete_row') not null,
  ndb$cft_cause         enum('row_does_not_exist','row_already_exists','data_in_conflict','trans_in_conflict') not null,
  ndb$orig_transid      bigint unsigned not null,
  primary key(ndb$server_id, ndb$master_server_id, ndb$master_epoch, ndb$count))
engine=ndb;
}

create table test.transactions
(auto_key      int auto_increment,
 from_name     varchar(100),
 to_name       varchar(100),
 detail        varchar(100),
 amount        int,
 primary key(auto_key, from_name, to_name, detail)) engine=ndb;

--echo Initialise balances across both bank sites
insert into test.balances values
 ("Larry", 100),
 ("Employee-1", 0),
 ("Employee-2", 0),
 ("Yacht dealer", 0),
 ("Newsagent", 0);

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
# Sync back to master, to ensure that what follows on slave,
# is in a separate epoch transaction.
# This is needed to get stable counts, not for correctness
#
FLUSH LOGS; # To give a position to sync
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc


--echo Bank sites are disconnected
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Larry buys a yacht using Primary bank site

begin;
insert into test.transactions (from_name, to_name, detail, amount)
  values ("Larry", "Yacht dealer", "Yacht purchase", 50);
update test.balances set balance = balance - 50 where name = "Larry";
update test.balances set balance = balance + 50 where name = "Yacht dealer";
commit;

--echo Show yacht transaction records

select * from test.transactions order by auto_key;
select * from test.balances order by name;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo Larry pays employees using Secondary bank site

begin;
insert into test.transactions (from_name, to_name, detail, amount)
  values ("Larry", "Employee-1", "Payment to Employee-1", 1);
update test.balances set balance = balance - 1 where name = "Larry";
update test.balances set balance = balance + 1 where name = "Employee-1";
commit;
begin;
insert into test.transactions (from_name, to_name, detail, amount)
  values ("Larry", "Employee-2", "Payment to Employee-2", 1);
update test.balances set balance = balance - 1 where name = "Larry";
update test.balances set balance = balance + 1 where name = "Employee-2";
commit;

--echo Employee-2 buys yacht magazine using Secondary bank site
begin;
insert into test.transactions (from_name, to_name, detail, amount)
  values ("Employee-2", "Newsagent", "Buy yacht magazine", 1);
update test.balances set balance = balance - 1 where name = "Employee-2";
update test.balances set balance = balance + 1 where name = "Newsagent";
commit;

--echo Show employee transactions

select * from test.transactions order by auto_key;
select * from test.balances order by name;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Bank sites re-connected
START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Records at Primary bank site

select * from test.transactions order by auto_key;
select * from test.balances order by name;

--echo Exceptions at Primary bank site
# Note count not included here as it's non-deterministic
# (CompletedOperations list order related to actual completion order related to
#  universal randomness)

if (!$extended_exceptions_table)
{
select server_id, master_server_id, auto_key, from_name, to_name, detail
  from test.transactions$EX order by auto_key, from_name, to_name, detail;
}

if ($extended_exceptions_table)
{
select ndb$server_id, ndb$master_server_id, auto_key, from_name, to_name, detail
   amount$old, amount$new, ndb$op_type, ndb$cft_cause from test.transactions$EX order by ndb$orig_transid, ndb$count;
}

--echo Conflict handling activity at Primary bank site
--echo Expect :
--echo   1 conflict from slave T1 on Larry's balance
--echo   1 conflict from slave T2 on Larry's balance
--echo  =2 row conflicts
--echo
--echo 3 (user) transactions rejected
--echo 9 rows rejected (3 per transaction)
--echo Variability : # epoch transactions, # row conflicts detected
--echo               1-3                   2-3
--echo               # detect_iter_count
--echo               1-3
--echo We only check stable values

--source suite/ndb_rpl/t/ndb_trans_conflict_info_stable.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Records at Secondary bank site

select * from test.transactions order by auto_key;
select * from test.balances order by name;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
drop table test.balances;
drop table test.transactions;
drop table test.transactions$EX;

--echo Test mixing transactional and non transactional
--echo -----------------------------------------------
--echo Remove old data from t1
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
delete from test.t1;
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Define table with row-based epoch detection
eval replace into mysql.ndb_replication values
           ("test", "t3", $SECONDARY_SERVER_ID, 7, NULL),
           ("test", "t3", $PRIMARY_SERVER_ID, 7, 'NDB\$EPOCH()');

create table t3 (a int primary key, b int) engine=ndb;
create table t4 (a int primary key, b int) engine=ndb;
create table t5 (a int primary key, b longtext) engine=ndb;

--echo Insert some data

insert into test.t1 values
  (1,1),
  (2,2),
  (3,3),
  (4,4),
  (5,5),
  (6,6);

insert into test.t3 values
  (11,11),
  (12,12),
  (13,13),
  (14,14),
  (15,15),
  (16,16);

insert into test.t4 values
  (21,21),
  (22,22),
  (23,23),
  (24,24),
  (25,25),
  (26,26);

insert into test.t5 values
  (1, REPEAT("B", 10000)),
  (2, REPEAT("E", 10000)),
  (3, REPEAT("A", 10000));

--echo Allow to propagate
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
FLUSH LOGS;  # Ensure Inserts are in previous epoch trans to what follows

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc

--echo Case 1 : Transactional detection affects row - based entries in same trans
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
update test.t1 set b=100 where a=1;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
# t3 is in a table without trans conflict detection (but with row based)
# t4 is in a table without any detection
# t1 is in a table with trans conflict detection
begin;
update test.t3 set b=1100 where a=11;
update test.t4 set b=2100 where a=21;
update test.t1 set b=1000 where a=1;
commit;

--echo Show slave transaction effect
select * from test.t1 order by a;
select * from test.t3 order by a;
select * from test.t4 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Expect Primary to have rejected whole trans across 3 tables

select * from test.t1 order by a;
select * from test.t3 order by a;
select * from test.t4 order by a;

--echo Expect 1 transaction rejected, 3 rows rejected
--echo        1 conflict row, 1 epoch, 1 iteration

--source suite/ndb_rpl/t/ndb_trans_conflict_info.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--echo Now restart rep to Secondary, and check realignment
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

select * from test.t1 order by a;
select * from test.t3 order by a;
select * from test.t4 order by a;

--echo Case 2 : Row based detection does not affect other transaction entries
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
update test.t3 set b=1200 where a=12;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
# Transaction conflicts with master, on table without transactional
# conflict detection
# Conflict will be detected on row, but no other transaction state
# will be reverted
#
begin;
update test.t3 set b=1201 where a=12;
update test.t4 set b=2200 where a=22;
update test.t1 set b=2000 where a=2;
commit;

--echo Show effect of transaction on Secondary
select * from test.t1 order by a;
select * from test.t3 order by a;
select * from test.t4 order by a;

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Show effect of transaction on Primary
--echo Only t3 should have been reverted

select * from test.t1 order by a;
select * from test.t3 order by a;
select * from test.t4 order by a;

--echo Expect all counters to be zero

--source suite/ndb_rpl/t/ndb_trans_conflict_info.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--echo Show effect of transaction on Secondary
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

select * from test.t1 order by a;
select * from test.t3 order by a;
select * from test.t4 order by a;

flush logs;
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Case 3 : Check behaviour where table with Blob is implicated
--echo          in transactional conflict.  Should result in Slave
--echo          stopping with an error.

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Setup warning suppression
--disable_query_log
call mtr.add_suppression("Transaction conflict handling on table t5 failed as table has Blobs which cannot be refreshed");
call mtr.add_suppression("NDBCLUSTER Error_code: MY-001296");
--enable_query_log


begin;
update t1 set b= 11 where a=1;
commit;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
begin;
update t1 set b= 111 where a=1;                 # Conflict
update t1 set b= 222 where a=2;                 # Implicated row
update t5 set b= REPEAT("T", 10000) where a=3;  # ImplicatedBlob update
commit;

--echo Show effect of transaction on Secondary
select * from test.t1 order by a;
select left(b,1), length(b) from test.t5 order by a;

--echo Check that Primary Slave has stopped
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--let $slave_sql_errno=1296
--source include/rpl/wait_for_applier_error.inc
#SHOW REPLICA STATUS;

--echo Restart Primary Slave
set global sql_replica_skip_counter=1;

START REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Restart Secondary Slave
START REPLICA;

flush logs;
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
drop table test.t3;
drop table test.t4;
drop table test.t5;

--echo Fix misalignment caused above
--echo Makes following result sets look nicer

update test.t1 set b=2 where a=2;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Show a simple row-level delete-delete conflict
--echo PRIMARY:

insert into test.t1 values (77, "A good year");

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo SECONDARY:
STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo PRIMARY:
--echo Primary Delete
delete from test.t1 where a=77;

--echo Show Primary contents
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo SECONDARY:
select * from test.t1 order by a;

--echo Secondary Delete->Insert in separate transactions
--echo and potentially different epochs
--echo Delete part with related transaction op
begin;
delete from test.t1 where a=77;
insert into test.t1 values (73, "A superb year");
commit;

# Grab commit epoch
--disable_query_log
--disable_result_log
SELECT @s1_epoch:=(Variable_value + 0) 
   FROM performance_schema.global_status
  WHERE variable_name like 'Ndb_last_commit_epoch_session';
let $S1_EPOCH= query_get_value('select @s1_epoch as v',v,1);
--enable_result_log
--enable_query_log


--echo Unrelated transaction from Secondary
begin;
insert into test.t1 values (84, "A good vintage");
commit;

# Optionally flush logs here to mix separate-epoch and
# same-epoch
--echo Optionally flush logs
let $do_flush=query_get_value(SELECT (RAND() * 10) > 5 AS y, y, 1);

if ($do_flush)
{
  --disable_query_log
  --disable_result_log
  FLUSH LOGS;
  --enable_query_log
  --enable_result_log
}

--echo Following Insert from Secondary - potentially in different epoch
begin;
insert into test.t1 values (77, "Exceptional");
insert into test.t1 values (80, "Poor");
commit;

# Grab commit epoch
--disable_query_log
--disable_result_log
SELECT @s2_epoch:=(Variable_value + 0) 
   FROM performance_schema.global_status
  WHERE variable_name like 'Ndb_last_commit_epoch_session';
let $S2_EPOCH= query_get_value('select @s2_epoch as v',v,1);
--enable_result_log
--enable_query_log

--echo Show Secondary contents
select * from test.t1 order by a;

--echo Allow propagation to Primary
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo PRIMARY:
--echo Expect:
--echo - NDB\$EPOCH_TRANS :
--echo   - Will not detect delete-delete conflict, so first
--echo     Secondary transaction will apply row 73.
--echo   - Unrelated transaction from Secondary (84) is present
--echo   - Following insert on delete-delete conflict row (77) is not in conflict,
--echo     so both it and the related row from the same transaction (80) are present
--echo     This applies whether the following insert is in the same or a different
--echo     epoch
--echo   Divergence:
--echo     - Primary will have 73,84,77,80
--echo     - Secondary will have 73,84,80, no 77
--echo
--echo - NDB\$EPOCH_TRANS2 :
--echo   - Will detect delete-delete conflict, so first 
--echo     Secondary transaction will not apply (no row 73)
--echo     77 and 73 will be refreshed (DEL)
--echo   - Unrelated transaction from Secondary (84) is present and applied
--echo     84 will be reflected (INS)
--echo   - Following insert on delete-delete conflict row (77) will be found in 
--echo     conflict iff this transaction is in the same epoch as the first 
--echo     conflicting transaction.  If it is in a different epoch then it will
--echo     not be found in conflict, as there will be no state available to 
--echo     use.
--echo   - If it is in-conflict, then 77 and 80 will be refreshed (DEL)
--echo     Result on both should be :
--echo       84
--echo   - If it is not in-conflict, then 77 and 80 will be reflected (INS)
--echo     Result on both should be :
--echo       84, 77, 80
--echo   No divergence

--disable_query_log
--disable_result_log
eval select @epochs_equal:=$S1_EPOCH=$S2_EPOCH as epochs_equal;
let $EPOCHS_EQUAL= query_get_value('select @epochs_equal as v',v,1);

eval select @Epoch2:=$PRIMARY_CONFLICT_ALG LIKE "%EPOCH2%";
let $HAVE_EPOCH2= query_get_value('select @Epoch2 as v',v,1); 

select @PrimaryRow77Present:=count(1) from test.t1 where a=77;
select @PrimaryRow73Present:=count(1) from test.t1 where a=73;
select @PrimaryRow80Present:=count(1) from test.t1 where a=80;
select @PrimaryRow84Present:=count(1) from test.t1 where a=84;

eval select @epoch_trans:=
  !$HAVE_EPOCH2 and
  @PrimaryRow77Present and
  @PrimaryRow73Present and
  @PrimaryRow80Present and
  @PrimaryRow84Present;

eval select @epoch_trans2_same_epoch:=
  $HAVE_EPOCH2 and
  $EPOCHS_EQUAL and
  !@PrimaryRow77Present and
  !@PrimaryRow73Present and
  !@PrimaryRow80Present and
  @PrimaryRow84Present;

eval select @epoch_trans2_diff_epoch:=
  $HAVE_EPOCH2 and
  !$EPOCHS_EQUAL and
  @PrimaryRow77Present and
  !@PrimaryRow73Present and
  @PrimaryRow80Present and
  @PrimaryRow84Present;


--enable_result_log
--enable_query_log

set @primary_ok:=
  ((@epoch_trans XOR
    @epoch_trans2_same_epoch) XOR
   @epoch_trans2_diff_epoch);

select @primary_ok;
let $primary_ok= query_get_value('select @primary_ok as v',v,1);

if (!$primary_ok)
{
  echo Problem, dumping data;
  select * from test.t1;
  eval select $HAVE_EPOCH2 as have_epoch2;
  eval select $EPOCHS_EQUAL as epochs_equal;
  die  Testcase problem;
}

if (!$HAVE_EPOCH2)
{
  echo EPOCH_TRANS has stable results independent of;
  echo epoch boundaries: ;

  select * from test.t1 order by a;

  source suite/ndb_rpl/t/ndb_trans_conflict_info_stable.inc;
}
if ($HAVE_EPOCH2)
{
  echo EPOCH2_TRANS results are dependent on epoch boundaries;
}

--echo Now sync with Secondary to show outcomes
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo SECONDARY:
--echo

--disable_query_log
--disable_result_log

select @SecondaryRow77Present:=count(1) from test.t1 where a=77;
select @SecondaryRow73Present:=count(1) from test.t1 where a=73;
select @SecondaryRow80Present:=count(1) from test.t1 where a=80;
select @SecondaryRow84Present:=count(1) from test.t1 where a=84;


eval select @epoch_trans:=
  !$HAVE_EPOCH2 and
  !@SecondaryRow77Present and
  @SecondaryRow73Present and
  @SecondaryRow80Present and
  @SecondaryRow84Present;

eval select @epoch_trans2_same_epoch:=
  $HAVE_EPOCH2 and
  $EPOCHS_EQUAL and
  !@SecondaryRow77Present and
  !@SecondaryRow73Present and
  !@SecondaryRow80Present and
  @SecondaryRow84Present;

eval select @epoch_trans2_diff_epoch:=
  $HAVE_EPOCH2 and
  !$EPOCHS_EQUAL and
  @SecondaryRow77Present and
  !@SecondaryRow73Present and
  @SecondaryRow80Present and
  @SecondaryRow84Present;

--enable_result_log
--enable_query_log

set @secondary_ok:=
  ((@epoch_trans XOR
    @epoch_trans2_same_epoch) XOR
   @epoch_trans2_diff_epoch);
select @secondary_ok;

let $secondary_ok= query_get_value('select @secondary_ok as v',v,1);

if (!$secondary_ok)
{
  echo Problem, dumping data;
  select * from test.t1;
  eval select $HAVE_EPOCH2 as have_epoch2;
  eval select $EPOCHS_EQUAL as epochs_equal;
  die  Testcase problem;
}

if (!$HAVE_EPOCH2)
{
  echo EPOCH_TRANS has stable results independent of;
  echo epoch boundaries: ;
  select * from test.t1 order by a;
}
if ($HAVE_EPOCH2)
{
  echo EPOCH2_TRANS results are dependent on epoch boundaries;
}

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo PRIMARY
--echo Cleanup
delete from test.t1 where a != 6;
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc







--echo PRIMARY
--echo Transactional Delete testcase

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo # STOP REPLICA on SECONDARY
STOP REPLICA;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo PRIMARY
--echo Primary updates row with pk 6

update test.t1 set b="Pentland" where a=6;

--echo PRIMARY content
--echo Row 6 updated
select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo SECONDARY
--echo Transaction deleting pk 6, inserting pk 7
begin;
  delete from test.t1 where a = 6;
  insert into test.t1 values (7, "Mendip");
commit;

# Grab commit epoch
--disable_query_log
--disable_result_log
SELECT @s1_epoch:=(Variable_value + 0) 
   FROM performance_schema.global_status
  WHERE variable_name like 'Ndb_last_commit_epoch_session';
let $S1_EPOCH= query_get_value('select @s1_epoch as v',v,1);
--enable_result_log
--enable_query_log


# Optionally flush logs here to mix separate-epoch and
# same-epoch
--echo Optionally flush logs
let $do_flush=query_get_value(SELECT (RAND() * 10) > 5 AS y, y, 1);

if ($do_flush)
{
  --disable_query_log
  --disable_result_log
  FLUSH LOGS;
  --enable_query_log
  --enable_result_log
}

#--echo Flushed? $do_flush

--echo Transaction deleting pk 7, inserting pk 8
begin;
delete from test.t1 where a = 7;
insert into test.t1 values (8, "Ochil");
commit;

# Grab commit epoch
--disable_query_log
--disable_result_log
SELECT @s2_epoch:=(Variable_value + 0) 
   FROM performance_schema.global_status
  WHERE variable_name like 'Ndb_last_commit_epoch_session';
let $S2_EPOCH= query_get_value('select @s2_epoch as v',v,1);
eval select @epochs_equal:=$S1_EPOCH=$S2_EPOCH as epochs_equal;
let $EPOCHS_EQUAL= query_get_value('select @epochs_equal as v',v,1);

eval select @Epoch2:=$PRIMARY_CONFLICT_ALG LIKE "%EPOCH2%";
let $HAVE_EPOCH2= query_get_value('select @Epoch2 as v',v,1); 
--enable_result_log
--enable_query_log

--echo SECONDARY content
--echo Rows 6 + 7 deleted.  Row 8 present.
select * from test.t1 order by a;

--echo Sync SECONDARY -> PRIMARY
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Expected Behaviour
--echo EPOCH_TRANS
--echo
--echo   1.  Conflict between Primary update to 6 + Secondary
--echo       [Delete of 6 + Insert of 7] is detected by Primary
--echo       and transaction is not applied at the Primary.
--echo       Rows 6 (INS) and 7 (DEL) are refreshed.  Row 6 has its 
--echo       epoch updated.
--echo
--echo   2.  Following Secondary transaction, [delete of 7
--echo       + insert of 8] behaviour depends on whether it
--echo       arrives in the same or a different epoch.
--echo       Same epoch : 
--echo         Will conflict with refreshed row 7, and so will
--echo         not be applied.  Row 8 will be refreshed (DEL)
--echo       Different epoch :
--echo         Will not conflict with Not-present row 7, and so
--echo         will be applied.
--echo
--echo   3.  On the Secondary, the observed sequence will be :
--echo         - Update 6 from PRIMARY : (Idempotent insert)
--echo         - Refresh (INS) 6 from PRIMARY
--echo         - Refresh (DEL) 7 from PRIMARY
--echo         Same epoch :
--echo         - Refresh (DEL) 8 from PRIMARY
--echo         Different epoch :
--echo         - Nothing
--echo 
--echo       Results (no divergence) :
--echo         Same epoch :
--echo           PRIMARY : 6
--echo           SECONDARY : 6
--echo         Diff epoch :
--echo           PRIMARY : 6,8
--echo           SECONDARY : 6,8
--echo
--echo
--echo EPOCH_TRANS2
--echo
--echo   1.  Conflict between Primary update to 6 + Secondary
--echo       [Delete of 6 + Insert of 7] is detected by Primary
--echo       and transaction is not applied at the Primary.
--echo       Rows 6 (INS) and 7 (DEL) are refreshed.  Row 6 has its 
--echo       epoch updated.
--echo
--echo   2.  Following Secondary transaction [delete of 7 + 
--echo       insert of 8] will conflict with the first 
--echo       transaction as EPOCH_TRANS2 does not allow
--echo       a delete of an already deleted row.
--echo       Row 7 will be refreshed (DEL) and row 8 will be 
--echo       refreshed (DEL)
--echo 
--echo   3.  On the Secondary, the observed sequence will be :
--echo       - Update 6 from PRIMARY : (Idempotent insert)
--echo       - Refresh (INS) 6 from PRIMARY
--echo       - Refresh (DEL) 7 from PRIMARY
--echo       - [Refresh (DEL) 7 from PRIMARY] - diff epoch
--echo       - Refresh (DEL) 8 from PRIMARY
--echo 
--echo       Results (no divergence) :
--echo           PRIMARY : 6
--echo           SECONDARY : 6
--echo

#eval select $EPOCHS_EQUAL;
#eval select $HAVE_EPOCH2;

--echo PRIMARY content 

--disable_query_log
--disable_result_log
select @PrimaryRow6Present:=count(1) from test.t1 where a=6;
select @PrimaryRow7Present:=count(1) from test.t1 where a=7;
select @PrimaryRow8Present:=count(1) from test.t1 where a=8;

eval select @epoch_trans_same_epoch:=
  !$HAVE_EPOCH2 and
  $EPOCHS_EQUAL and
  @PrimaryRow6Present and
  !@PrimaryRow7Present and
  !@PrimaryRow8Present;

eval select @epoch_trans_diff_epoch:=
  !$HAVE_EPOCH2 and
  !$EPOCHS_EQUAL and
  @PrimaryRow6Present and
  !@PrimaryRow7Present and
  @PrimaryRow8Present;

eval select @epoch2_trans:=
  $HAVE_EPOCH2 and
  @PrimaryRow6Present and
  !@PrimaryRow7Present and
  !@PrimaryRow8Present;
--enable_result_log
--enable_query_log


set @primary_ok:=
  ((@epoch_trans_same_epoch XOR
    @epoch_trans_diff_epoch) XOR
   @epoch2_trans);

select @primary_ok;
let $primary_ok= query_get_value('select @primary_ok as v',v,1);

if (!$primary_ok)
{
  echo Problem, dumping data;
  select * from test.t1;
  eval select $HAVE_EPOCH2 as have_epoch2;
  eval select $EPOCHS_EQUAL as epochs_equal;
  die  Testcase problem;
}

if ($HAVE_EPOCH2)
{
 select * from test.t1 order by a;
}

--echo Restart SECONDARY Slave
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
START REPLICA;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc


--echo Sync PRIMARY -> SECONDARY

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo SECONDARY content

--disable_query_log
--disable_result_log

select @SecondaryRow6Present:=count(1) from test.t1 where a=6;
select @SecondaryRow7Present:=count(1) from test.t1 where a=7;
select @SecondaryRow8Present:=count(1) from test.t1 where a=8;

eval select @epoch_trans_same_epoch:=
  !$HAVE_EPOCH2 and
  $EPOCHS_EQUAL and
  @SecondaryRow6Present and
  !@SecondaryRow7Present and
  !@SecondaryRow8Present;

eval select @epoch_trans_diff_epoch:=
  !$HAVE_EPOCH2 and
  !$EPOCHS_EQUAL and
  @SecondaryRow6Present and
  !@SecondaryRow7Present and
  @SecondaryRow8Present;

eval select @epoch2_trans:=
  $HAVE_EPOCH2 and
  @SecondaryRow6Present and
  !@SecondaryRow7Present and
  !@SecondaryRow8Present;

--enable_result_log
--enable_query_log

set @secondary_ok:=
  ((@epoch_trans_same_epoch XOR
    @epoch_trans_diff_epoch) XOR
   @epoch2_trans);
select @secondary_ok;

let $secondary_ok= query_get_value('select @secondary_ok as v',v,1);

if (!$secondary_ok)
{
  echo Problem, dumping data;
  select * from test.t1;
  eval select $HAVE_EPOCH2 as have_epoch2;
  eval select $EPOCHS_EQUAL as epochs_equal;
  die  Testcase problem;
}

if ($HAVE_EPOCH2)
{
 select * from test.t1 order by a;
}

--echo STABILISE

--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc


# Cleanup
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
drop table mysql.ndb_replication;
drop table test.t1;
drop table test.t2;

--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
flush logs;
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc

--connection master
STOP REPLICA;
RESET REPLICA;

#change replication source to source_host='';
--source include/rpl/deinit.inc

# TODO
# More complex dependencies
