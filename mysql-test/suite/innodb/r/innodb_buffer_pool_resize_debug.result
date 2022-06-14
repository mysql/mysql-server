set global innodb_file_per_table=ON;
set global innodb_thread_concurrency=20;
create table t2 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
insert into t2 (c1, c2) values (1, 1);
create table t3 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
insert into t3 (c1, c2) values (1, 1);
create table t4 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
insert into t4 (c1, c2) values (1, 1);
create table t5 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
insert into t5 (c1, c2) values (1, 1);
create table t6 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
insert into t6 (c1, c2) values (1, 1);
create database test2;
create table test2.t7 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
insert into test2.t7 (c1, c2) values (1, 1);
drop table test2.t7;
set global DEBUG="+d,ib_buf_pool_resize_wait_before_resize";
set global innodb_buffer_pool_size = 12*1024*1024;
set global innodb_buffer_pool_size = 8*1024*1024;
Warnings:
Warning	3166	Another buffer pool resize is already in progress.
select @@global.innodb_buffer_pool_size;
@@global.innodb_buffer_pool_size
12582912
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
0
set global innodb_adaptive_hash_index = ON;
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
0
set global innodb_adaptive_hash_index = OFF;
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
0
create table t1 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
analyze table t2;
alter table t3 algorithm=inplace, add index idx (c1);
alter table t4 rename to t0;
drop table t5;
drop database test2;
select count(*) > 0 from information_schema.innodb_buffer_page;
set global DEBUG="-d,ib_buf_pool_resize_wait_before_resize";
Table	Op	Msg_type	Msg_text
test.t2	analyze	status	OK
count(*) > 0
1
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* buffer pool 0 : failed to allocate new memory.");
set global DEBUG="+d,ib_buf_chunk_init_fails";
set global innodb_buffer_pool_size = 16*1024*1024;
set global DEBUG="-d,ib_buf_chunk_init_fails";
create table t8 (c1 int not null primary key, c2 int not null default 0) engine=InnoDB;
insert into t8 (c1, c2) values (1, 1);
drop table t8;
drop table t1;
drop table t2;
drop table t3;
drop table t0;
drop table t6;
#
# BUG#23590280 NO WARNING WHEN REDUCING INNODB_BUFFER_POOL_SIZE INSIZE THE FIRST CHUNK
#
SET GLOBAL innodb_fast_shutdown = 0;
# restart: --innodb-buffer-pool-size=24M --innodb-buffer-pool-chunk-size=24M
set @before_innodb_buffer_pool_size = @@innodb_buffer_pool_size;
set global innodb_buffer_pool_size=@before_innodb_buffer_pool_size div 2;
Warnings:
Warning	1210	InnoDB: Cannot resize buffer pool to lesser than chunk size of 25165824 bytes.
set global innodb_buffer_pool_size=@before_innodb_buffer_pool_size * 2;
set global innodb_buffer_pool_size=@before_innodb_buffer_pool_size;
set global innodb_buffer_pool_size=@before_innodb_buffer_pool_size;
# restart:
