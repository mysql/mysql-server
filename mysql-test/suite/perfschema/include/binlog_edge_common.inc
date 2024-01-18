# Test replication, when using special non-replicated tables.
#
# This test involve special statements that use non-replicated tables.
# Changes affecting non replicated tables are never written to the binlog.
# Executing these statements may or may not work, as the statements involved
# are edge cases.
# In MIXED or ROW binlog format, execution should succeed,
# and only partial data (the rows affecting replicated tables only)
# should be written to the binlog.
# In STATEMENT binlog format, execution should
# raise a warning (ER_BINLOG_UNSAFE_STATEMENT) if a non replicated table is
# only read from, or fail with an error (ER_BINLOG_STMT_MODE_AND_NO_REPL_TABLES)
# if a non replicated table is written to.
#
# SHOW ERRORS will print in the
# test .result file the exact outcome.

RESET BINARY LOGS AND GTIDS;

--disable_warnings
drop database if exists my_replicated_db;
--enable_warnings

call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");

create database my_replicated_db;

create table my_replicated_db.my_tx_table(a bigint) engine = innodb;
create table my_replicated_db.my_bh_table(a bigint) engine = blackhole;

use test;
drop table if exists marker_start;

use my_replicated_db;

insert into my_tx_table(a)
  values (1000), (2000), (3000);

insert into my_bh_table(a)
  values (1000), (2000), (3000);

use test;
drop table if exists marker_insert_select;

use my_replicated_db;

# Note:
# The queries used here do not make any sense (no semantic).
# What this test is interrested in, is check the behavior
# when replicating queries that mix both:
# - non replicated tables
# - replicated tables

insert into my_tx_table(a)
  select thread_id from performance_schema.threads;

insert into my_bh_table(a)
  select thread_id from performance_schema.threads;

# For the information_schema,
# no error is enforced yet.
# Documenting the current behavior

insert into my_tx_table(a)
  select id from information_schema.processlist;

insert into my_bh_table(a)
  select id from information_schema.processlist;

insert into my_tx_table(a)
  select thread_id from mysql.general_log;

insert into my_bh_table(a)
  select thread_id from mysql.general_log;

insert into my_tx_table(a)
  select thread_id from mysql.slow_log;

insert into my_bh_table(a)
  select thread_id from mysql.slow_log;

insert into my_tx_table(a)
  select Relay_log_pos from mysql.slave_relay_log_info;

insert into my_bh_table(a)
  select Relay_log_pos from mysql.slave_relay_log_info;

insert into my_tx_table(a)
  select Master_log_pos from mysql.slave_master_info;

insert into my_bh_table(a)
  select Master_log_pos from mysql.slave_master_info;

insert into my_tx_table(a)
  select Relay_log_pos from mysql.slave_worker_info;

insert into my_bh_table(a)
  select Relay_log_pos from mysql.slave_worker_info;

use test;
drop table if exists marker_multi_update;

use my_replicated_db;

--error 0, ER_BINLOG_STMT_MODE_AND_NO_REPL_TABLES
update my_tx_table, performance_schema.setup_instruments
  set my_tx_table.a = my_tx_table.a + 1,
  performance_schema.setup_instruments.timed= 'NO';

--error 0, ER_BINLOG_STMT_MODE_AND_NO_REPL_TABLES
update my_bh_table, performance_schema.setup_instruments
  set my_bh_table.a = my_bh_table.a + 1,
  performance_schema.setup_instruments.timed= 'NO';

use test;
drop table if exists marker_multi_delete;

use my_replicated_db;

insert into performance_schema.setup_actors
  values ('FOO', 'FOO', 'FOO', 'YES', 'YES');

--error 0, ER_BINLOG_STMT_MODE_AND_NO_REPL_TABLES
delete my_tx_table.*, performance_schema.setup_actors.*
  from my_tx_table, performance_schema.setup_actors
  where my_tx_table.a != 1000
  or performance_schema.setup_actors.role='FOO';

insert into performance_schema.setup_actors
  values ('BAZ', 'BAZ', 'BAZ', 'YES', 'YES');

--error 0, ER_BINLOG_STMT_MODE_AND_NO_REPL_TABLES
delete my_bh_table.*, performance_schema.setup_actors.*
  from my_bh_table, performance_schema.setup_actors
  where my_bh_table.a != 1000
  or performance_schema.setup_actors.role='BAZ';

use test;
drop table if exists marker_end;

drop database my_replicated_db;

--source include/rpl/deprecated/show_binlog_events.inc

# Restore performance_schema.setup_actors, damaged by this script

truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors values ('%', '%', '%', 'YES', 'YES');

