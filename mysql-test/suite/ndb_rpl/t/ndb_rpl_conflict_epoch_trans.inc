#
# Test engine native conflict resolution for ndb
#   NDB$EPOCH[2]_TRANS() function
#
#
--source include/have_ndb.inc
--source include/have_binlog_format_mixed_or_row.inc
--source suite/ndb_rpl/ndb_master-slave.inc
--source suite/ndb_rpl/t/ndb_trans_conflict_info_init.inc

--echo Setup circular replication
--disable_query_log
--disable_result_log
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
RESET MASTER;
STOP SLAVE;
select @secondary_server_id:=(variable_value+0)
       from information_schema.global_variables
       where variable_name like 'server_id';
let $SECONDARY_SERVER_ID= query_get_value('select @secondary_server_id as v',v,1);
select @secondary_port:=(variable_value+0)
       from information_schema.global_variables
       where variable_name like 'port';
let $SECONDARY_PORT= query_get_value('select @secondary_port as v',v,1);

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
RESET MASTER;
STOP SLAVE;
--eval CHANGE MASTER TO master_host="127.0.0.1",master_port=$SECONDARY_PORT,master_user="root"
START SLAVE;
select @primary_server_id:=(variable_value+0)
       from information_schema.global_variables
       where variable_name like 'server_id';
let $PRIMARY_SERVER_ID= query_get_value('select @primary_server_id as v',v,1);
select @primary_port:=(variable_value+0)
       from information_schema.global_variables
       where variable_name like 'port';
let $PRIMARY_PORT= query_get_value('select @primary_port as v',v,1);

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--eval CHANGE MASTER TO master_host="127.0.0.1",master_port=$PRIMARY_PORT,master_user="root"
START SLAVE;

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

stop slave;

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

start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should have been realigned to Primary

select * from t1 order by a;

--echo Show rollback of whole secondary transaction
--echo --------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

stop slave;

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
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should have been realigned to Primary
select * from test.t1 order by a;

--echo Show rollback of dependent transaction as well
--echo ----------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
stop slave;

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
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should have been realigned to Primary

select * from test.t1 order by a;


--echo Show rollback of dependent transaction across different tables
--echo --------------------------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
stop slave;

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
start slave;
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
stop slave;

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
start slave;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Secondary should be aligned with Primary

select * from t1 order by a;


--echo Show that non-conflicting ancestors are not implicated
--echo ------------------------------------------------------

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
stop slave;

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
start slave;

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
stop slave;
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
start slave;

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
stop slave;
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
start slave;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

select * from test.t1 order by a;
select * from test.t3 order by a;
select * from test.t4 order by a;

--echo Case 2 : Row based detection does not affect other transaction entries
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
stop slave;
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
start slave;
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
STOP SLAVE;

--source suite/ndb_rpl/t/ndb_connect_to_primary.inc

--echo Setup warning suppression
--disable_query_log
call mtr.add_suppression("Transaction conflict handling on table t5 failed as table has Blobs which cannot be refreshed");
call mtr.add_suppression("NDBCLUSTER Error_code: 1296");
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
--source include/wait_for_slave_sql_error.inc
#SHOW SLAVE STATUS;

--echo Restart Primary Slave
set global sql_slave_skip_counter=1;

START SLAVE;

--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc

--echo Restart Secondary Slave
START SLAVE;

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
stop slave;

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

--echo Show Secondary contents
select * from test.t1 order by a;

--echo Allow propagation to Primary
--source suite/ndb_rpl/t/ndb_sync_secondary_to_primary.inc
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--echo PRIMARY:
--echo Expect:
--echo - Delete-Delete conflict did not result in rejection of Secondary transaction
--echo   Therefore related row from Secondary Deleting transaction (73) is present
--echo - Unrelated transaction from Secondary (84) is present
--echo - Following insert on delete-delete conflict row (77) is not in conflict,
--echo   so both it and the related row from the same transaction (80) are present
--echo

select * from test.t1 order by a;

--source suite/ndb_rpl/t/ndb_trans_conflict_info_stable.inc

--echo Now sync with Secondary to show outcomes
--echo   NDB\$EPOCH_TRANS will have divergence as the Primary delete of
--echo   row 77 took out the newly inserted row 77.
--echo   NDB\$EPOCH2_TRANS will have no divergence as the Secondary insert
--echo   is reflected after the Primary delete, and re-inserts the value.
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
start slave;
--source suite/ndb_rpl/t/ndb_connect_to_primary.inc
--source suite/ndb_rpl/t/ndb_sync_primary_to_secondary.inc
--source suite/ndb_rpl/t/ndb_connect_to_secondary.inc
--echo SECONDARY:
--echo 
select * from test.t1 order by a;


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
stop slave;
reset slave;

#change master to master_host='';
--source include/rpl_end.inc

# TODO
# More complex dependencies
