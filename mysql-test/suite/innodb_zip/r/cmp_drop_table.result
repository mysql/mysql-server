set global innodb_file_per_table=on;
create table t1(a text) engine=innodb key_block_size=8;
SELECT page_size FROM information_schema.innodb_cmpmem WHERE pages_used > 0;
page_size
8192
drop table t1;
SELECT page_size FROM information_schema.innodb_cmpmem WHERE pages_used > 0;
page_size
8192
create table t2(a text) engine=innodb;
SELECT page_size FROM information_schema.innodb_cmpmem WHERE pages_used > 0;
page_size
drop table t2;
