#############################################
#Purpose:    Generic replication from Innodb to NDB
#            using binlog_format defined by caller
#            and ensuring that the ndb_apply_status
#            table is updated.
#############################################
#
# The ndb_apply_status table contains information about the binlog on
# source, the log_name is the file name, start_pos and end_pos relates
# to positions of the current "group" where end_pos = start_pos + length(group).
#
# A group in the binlog is normally a query/transaction with BEGIN/COMMIT as
# well as any preceding events containing state.
#
# For example queries like this:
# mysql> BEGIN
# mysql> UPDATE tpcb.account SET b=1 WHERE pk=4
# mysql> UPDATE tpcb.branch SET b=2 WHERE pk=3
# mysql> COMMIT
#
# would create this group:
# binlog.001	419917	Anonymous_Gtid	1	419996	SET @@SESSION.GTID_NEXT= 'ANONYMOUS'
# binlog.001	419996	Query	1	420080	BEGIN
# binlog.001	420080	Table_map	1	420145	table_id: 271 (tpcb.account)
# binlog.001	420145	Update_rows	1	420241	table_id: 271 flags: STMT_END_F
# binlog.001	420241	Table_map	1	420304	table_id: 272 (tpcb.branch)
# binlog.001	420304	Update_rows	1	420392	table_id: 272 flags: STMT_END_F
# binlog.001	420753	Xid	1	420784	COMMIT /* xid=5090 */
#
# Then log_name = binlog.001, start_pos = 419917 and end_pos = 420784 should
# end up in ndb_apply_status on replica when this group has been replicated.
#
# ###############################################
# Notes:
#
# Check contents of ndb_aply_status on replica by:
#
# 1) Save binlog file and position before query (on source), this should then
# show up as "log_name" and "start_pos" in ndb_apply_status (on replica)
# when change has been replicated.
#
# 2) Save binlog position after query (on source), this should then show
# up as "end_pos" in ndb_apply_status (on replica) when change has
# been applied replicated.
#
# select_ndb_apply_status.inc
# Selects out the log name and start pos
# from the ndb_apply_status table on replica and compare with the expected
# values from 1) and 2) saved when executing the query/transaction.
# Print relevant debugging information if the positions does not align,
# then fail the test.
#
# include/tpcb.inc
# Creates DATABASE tpcb, the tables and 
# stored procedures for loading the DB
# and for running transactions against DB.
##############################################

--source include/rpl_connection_master.inc

create table t1 (a int key, b int) engine innodb;
create table t2 (a int key, b int) engine innodb;

let $master_server_id = `select @@server_id`;
#echo master_server_id: $master_server_id;

--echo

--source include/sync_slave_sql_with_master.inc
alter table t1 engine ndb;
alter table t2 engine ndb;

--echo
--echo *** Test 1 ***
--echo

# check binlog position without begin
--source include/rpl_connection_master.inc

let $source_file= query_get_value(SHOW MASTER STATUS, File, 1);
let $source_start_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

insert into t1 values (1,2);

let $source_end_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

# Show the binlog events recorded for above change
let $binlog_file = $source_file;
let $binlog_start = $source_start_pos;
--source include/show_binlog_events.inc

--source include/sync_slave_sql_with_master.inc
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

--echo
--echo *** Test 1B ***
--echo
--source include/rpl_connection_master.inc

let $source_file= query_get_value(SHOW MASTER STATUS, File, 1);
let $source_start_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

# check binlog position with begin
begin;
insert into t1 values (2,3);
insert into t2 values (3,4);
commit;

let $source_end_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

# Show the binlog events recorded for above change
let $binlog_file = $source_file;
let $binlog_start = $source_start_pos;
--source include/show_binlog_events.inc

--source include/sync_slave_sql_with_master.inc
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

# Cleanup after test 1 and test 2
--source include/rpl_connection_master.inc
DROP TABLE test.t1, test.t2;
--source include/sync_slave_sql_with_master.inc
SHOW TABLES;

--echo
--echo *** Test 2 ***
--echo

# Run in some transactions using stored procedures
# and ensure that the ndb_apply_status table is
# updated to show the transactions
--source include/rpl_connection_master.inc

# Create database/tables and stored procdures
--source include/tpcb.inc

# Switch tables on slave to use NDB 
--source include/sync_slave_sql_with_master.inc
USE tpcb;
ALTER TABLE account ENGINE NDB;
ALTER TABLE branch ENGINE NDB;
ALTER TABLE teller ENGINE NDB;
ALTER TABLE history ENGINE NDB;

# Load DB tpcb and run some transactions
--source include/rpl_connection_master.inc

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
   let $source_file= query_get_value(SHOW MASTER STATUS, File, 1);
   let $source_start_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

   COMMIT;
   echo COMMIT;
 }
 if ($run_bad)
 {
   ROLLBACK;
   echo ROLLBACK;
 }
 dec $run;
}

SET AUTOCOMMIT=1;
--enable_query_log

let $source_end_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

# Show the binlog events recorded for last part of above change
let $binlog_file = $source_file;
let $binlog_start = $source_start_pos;
let $binlog_limit = $off_set, 1; # Show only the last COMMIT event in group
--source include/show_binlog_events.inc

--source include/sync_slave_sql_with_master.inc
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

--echo
--echo ** Test 3 **
--echo

# Flush the logs on the master moving all
# Transaction to a new binlog and ensure
# that the ndb_apply_status table is updated
# to show the use of the new binlog.
--source include/rpl_connection_master.inc

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
   let $source_file= query_get_value(SHOW MASTER STATUS, File, 1);
   let $source_start_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

   COMMIT;
   echo COMMIT;
 }
 if ($run_bad)
 {
   ROLLBACK;
   echo ROLLBACK;
 }
 dec $run;
}
SET AUTOCOMMIT=1;
--enable_query_log

let $source_end_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

# Show the binlog events recorded for last part of above change
let $binlog_file = $source_file;
let $binlog_start = $source_start_pos;
let $binlog_limit = $off_set, 1; # Show only the last COMMIT event in group
--source include/show_binlog_events.inc

--source include/sync_slave_sql_with_master.inc
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

--echo
--echo ** Test 4 **
--echo

# Now we reset both the master and the slave
# Run some more transaction and ensure
# that the ndb_apply_status is updated
# correctly

--source include/rpl_connection_master.inc

# Reset both slave and master
# This should reset binlog to #1
--source include/rpl_reset.inc

--echo

# Run in some transactions and check
--source include/rpl_connection_master.inc
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
   let $source_file= query_get_value(SHOW MASTER STATUS, File, 1);
   let $source_start_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

   COMMIT;
   echo COMMIT;
 }
 if ($run_bad)
 {
   ROLLBACK;
   echo ROLLBACK;
 }
 dec $run;
}
SET AUTOCOMMIT=1;
--enable_query_log

let $source_end_pos= query_get_value(SHOW MASTER STATUS, Position, 1);

# Show the binlog events recorded for last part of above change
let $binlog_file = $source_file;
let $binlog_start = $source_start_pos;
let $binlog_limit = $off_set, 1; # Show only the last COMMIT event  in group
--source include/show_binlog_events.inc

--source include/sync_slave_sql_with_master.inc
--source suite/ndb_rpl/t/select_ndb_apply_status.inc

# Since we are doing replication, it is a good
# idea to check to make sure all data was 
# Replicated correctly

--let $diff_tables= tpcb.account, master:tpcb.account
--source include/diff_tables.inc

--let $diff_tables= tpcb.teller, master:tpcb.teller
--source include/diff_tables.inc

--let $diff_tables= tpcb.branch, master:tpcb.branch
--source include/diff_tables.inc

--let $diff_tables= tpcb.history, master:tpcb.history
--source include/diff_tables.inc

--source include/rpl_connection_master.inc
DROP DATABASE tpcb;

--source include/sync_slave_sql_with_master.inc
