# Test for bug #12400341: INNODB CAN LEAVE ORPHAN IBD FILES AROUND
set @old_innodb_rollback_segments = @@innodb_rollback_segments;
set global innodb_rollback_segments=1;
show variables like "max_connections";
Variable_name	Value
max_connections	10
show variables like "innodb_thread_concurrency";
Variable_name	Value
innodb_thread_concurrency	0
show variables like "innodb_file_per_table";
Variable_name	Value
innodb_file_per_table	ON
show variables like "innodb_rollback_segments";
Variable_name	Value
innodb_rollback_segments	1
show variables like "innodb_trx_rseg_n_slots_debug";
Variable_name	Value
innodb_trx_rseg_n_slots_debug	5
drop database if exists mysqltest;
create database mysqltest;
CREATE TABLE mysqltest.transtable (id int unsigned NOT NULL PRIMARY KEY, val int DEFAULT 0) ENGINE=InnoDB;
select count(*) from information_schema.processlist;
count(*)
12
CREATE TABLE mysqltest.testtable (id int unsigned not null primary key) ENGINE=InnoDB;
ERROR HY000: Too many active concurrent transactions
select count(*) from information_schema.processlist;
count(*)
12
select count(*) from information_schema.processlist;
count(*)
12
drop database mysqltest;
set global innodb_rollback_segments = @old_innodb_rollback_segments;
