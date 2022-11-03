#############################################
#Purpose:    Generic replication from Innodb to NDB
#            using binlog_format defined by caller
#            and ensuring that the ndb_apply_status
#            table is updated.
#############################################
# Notes: 
# select_ndb_apply_status.inc
# Selects out the log name and start pos
# from the ndb_apply_status table
#
# include/tpcb.inc
# Creates DATABASE tpcb, the tables and 
# stored procedures for loading the DB
# and for running transactions against DB.
##############################################


--echo
--echo *** Test 1 ***
--echo

connection master;
create table t1 (a int key, b int) engine innodb;
create table t2 (a int key, b int) engine innodb;

let $master_server_id = `select @@server_id`;
#echo master_server_id: $master_server_id;

--echo

--sync_slave_with_master
alter table t1 engine ndb;
alter table t2 engine ndb;

--echo

# check binlog position without begin
connection master;
insert into t1 values (1,2);

--echo

--sync_slave_with_master
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

--echo

connection master;
--echo # Now check that that is in the apply_status table is consistant
--echo # with what is in the binlog
--source include/show_binlog_events.inc

--echo

# check binlog position with begin
begin;
insert into t1 values (2,3);
insert into t2 values (3,4);
commit;

--echo

--sync_slave_with_master
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

connection master;
--source include/show_binlog_events.inc

--echo

connection master;
DROP TABLE test.t1, test.t2;
--sync_slave_with_master
SHOW TABLES;

# Run in some transactions using stored procedures
# and ensure that the ndb_apply_status table is 
# updated to show the transactions


--echo
--echo *** Test 2 ***
--echo

# Create database/tables and stored procdures
connection master;
--source include/tpcb.inc

# Switch tables on slave to use NDB 
--sync_slave_with_master
USE tpcb;
ALTER TABLE account ENGINE NDB;
ALTER TABLE branch ENGINE NDB;
ALTER TABLE teller ENGINE NDB;
ALTER TABLE history ENGINE NDB;

--echo

# Load DB tpcb and run some transactions
connection master;
--disable_query_log
CALL tpcb.load();
SET AUTOCOMMIT=0;
let $run= 5;
while ($run)
{
 START TRANSACTION;
 --eval CALL tpcb.trans($rpl_format);
 let $run_bad= $mysql_errno;
 if (!$run_bad)
 {
   COMMIT;
 }
 if ($run_bad)
 {
   ROLLBACK;
 }
 dec $run;
}

SET AUTOCOMMIT=1;
--enable_query_log

--sync_slave_with_master
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

--echo

connection master;
let $binlog_limit = $off_set, 1;
--source include/show_binlog_events.inc

# Flush the logs on the master moving all
# Transaction to a new binlog and ensure
# that the ndb_apply_status table is updated
# to show the use of the new binlog.

--echo
--echo ** Test 3 **
--echo

# Flush logs on master which should force it
# to switch to binlog #2

FLUSH LOGS;

# Run in some transaction to increase end pos in
# binlog

--disable_query_log
SET AUTOCOMMIT=0;
let $run= 5;
while ($run)
{
 START TRANSACTION;
 --eval CALL tpcb.trans($rpl_format);
 let $run_bad= $mysql_errno;
 if (!$run_bad)
 {
   COMMIT;
 }
 if ($run_bad)
 {
   ROLLBACK;
 }
 dec $run;
}
SET AUTOCOMMIT=1;
--enable_query_log

--echo

--sync_slave_with_master
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

--echo

connection master;
let $binlog_limit = $off_set, 1;
--source include/show_binlog_events.inc

# Now we reset both the master and the slave
# Run some more transaction and ensure
# that the ndb_apply_status is updated
# correctly

--echo
--echo ** Test 4 **
--echo

# Reset both slave and master
# This should reset binlog to #1
--source include/rpl_reset.inc

--echo

# Run in some transactions and check
connection master;
--disable_query_log
SET AUTOCOMMIT=0;
let $run= 5;
while ($run)
{
 START TRANSACTION;
 --eval CALL tpcb.trans($rpl_format);
 eval SET @my_errno= $mysql_errno;
 let $run_good= `SELECT @my_errno = 0`;
 let $run_bad= `SELECT @my_errno <> 0`;
 if ($run_good)
 {
   COMMIT;
 }
 if ($run_bad)
 {
   ROLLBACK;
 }
 dec $run;
}
SET AUTOCOMMIT=1;
--enable_query_log

--sync_slave_with_master
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

--echo

connection master;
let $binlog_limit = $off_set, 1;
--source include/show_binlog_events.inc

# Since we are doing replication, it is a good
# idea to check to make sure all data was 
# Replicated correctly

--sync_slave_with_master
--let $diff_tables= tpcb.account, master:tpcb.account
--source include/diff_tables.inc

--let $diff_tables= tpcb.teller, master:tpcb.teller
--source include/diff_tables.inc

--let $diff_tables= tpcb.branch, master:tpcb.branch
--source include/diff_tables.inc

--let $diff_tables= tpcb.history, master:tpcb.history
--source include/diff_tables.inc

connection master;
DROP DATABASE tpcb;

--sync_slave_with_master
