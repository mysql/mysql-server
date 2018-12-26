# Test replication, when using special non-replicated tables.
#
# This test involve special statements that use non-replicated tables.
# Changes affecting non replicated tables are never written to the binlog.
# Executing these statements should work nicely with replication in all cases:
# - STATEMENT binlog format
# - MIXED binlog format
# - ROW binlog format.

RESET MASTER;

--disable_warnings
drop database if exists my_local_db;
--enable_warnings

create database my_local_db;

create table my_local_db.my_tx_table(a bigint) engine = innodb;
create table my_local_db.my_non_tx_table(a bigint) engine = myisam;

use test;
drop table if exists marker_start;

# --binlog-ignore-db only works with the current database.
use my_local_db;

insert into my_tx_table(a)
  values (1000), (2000), (3000);

insert into my_non_tx_table(a)
  values (1000), (2000), (3000);

use test;
drop table if exists marker_truncate;

use performance_schema;
truncate table events_waits_history;
truncate table events_waits_history_long;

use test;
truncate table performance_schema.events_statements_history_long;
truncate table performance_schema.host_cache;

use test;
drop table if exists marker_update;

use performance_schema;
update setup_instruments set enabled='NO';
update setup_instruments set timed='NO';
use test;
update performance_schema.setup_instruments set enabled='YES', timed='YES';
update performance_schema.threads set instrumented='YES';

use test;
drop table if exists marker_insert;

insert into performance_schema.setup_actors(`user`, `host`, `role`)
  values ('XXX', 'XXX', 'XXX'),
         ('YYY', 'YYY', 'YYY'),
         ('ZZZ', 'ZZZ', 'ZZZ');

select * from performance_schema.setup_actors
  where user in ('XXX', 'YYY', 'ZZZ') order by user;

insert into performance_schema.setup_objects
  (object_type, object_schema, object_name, enabled, timed)
  values ('TABLE', 'DB1', 'AAA', 'YES', 'YES'),
         ('TABLE', 'DB1', 'BBB', 'NO', 'NO'),
         ('TABLE', 'DB2', 'CCC', 'YES', 'NO'),
         ('TABLE', 'DB2', 'DDD', 'NO', 'YES');

select * from performance_schema.setup_objects
  where object_schema like 'DB%' order by object_name;

use test;
drop table if exists marker_insert_select;

# Note:
# The queries used here do not make any sense (no semantic).
# What this test is interrested in, is check the behavior
# when replicating queries that mix both:
# - non replicated tables
# - replicated tables

use my_local_db;
insert into my_tx_table(a)
  select thread_id from performance_schema.threads;

insert into my_non_tx_table(a)
  select thread_id from performance_schema.threads;

insert into my_tx_table(a)
  select id from information_schema.processlist;

insert into my_non_tx_table(a)
  select id from information_schema.processlist;

insert into my_tx_table(a)
  select thread_id from mysql.general_log;

insert into my_non_tx_table(a)
  select thread_id from mysql.general_log;

insert into my_tx_table(a)
  select thread_id from mysql.slow_log;

insert into my_non_tx_table(a)
  select thread_id from mysql.slow_log;

insert into my_tx_table(a)
  select Relay_log_pos from mysql.slave_relay_log_info;

insert into my_non_tx_table(a)
  select Relay_log_pos from mysql.slave_relay_log_info;

insert into my_tx_table(a)
  select Master_log_pos from mysql.slave_master_info;

insert into my_non_tx_table(a)
  select Master_log_pos from mysql.slave_master_info;

insert into my_tx_table(a)
  select Relay_log_pos from mysql.slave_worker_info;

insert into my_non_tx_table(a)
  select Relay_log_pos from mysql.slave_worker_info;

use test;
drop table if exists marker_delete;

delete from performance_schema.setup_actors
  where user in ('XXX', 'YYY', 'ZZZ');

delete from performance_schema.setup_objects
  where object_schema like 'DB%';

use test;
drop table if exists marker_end;

drop database my_local_db;

# The content of the binlog dumped in the result file
# should not contain any references to non-replicated tables.

--source include/show_binlog_events.inc

