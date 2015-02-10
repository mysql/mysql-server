include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create view coord_wait_list as
SELECT id from Information_Schema.processlist
where state like 'Waiting for Slave Worker%';
include/stop_slave.inc
set @save.slave_parallel_workers= @@global.slave_parallel_workers;
set @@global.slave_parallel_workers= 4;
create view worker_wait_list as
SELECT id from Information_Schema.processlist
where state like 'Waiting for an event from Coordinator';
include/stop_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
set @save_slave_pending_jobs_size_max = @@global.slave_pending_jobs_size_max;
set @@global.slave_pending_jobs_size_max= 1024;
include/start_slave.inc
Configured number of Workers is started.
create database d0;
create table d0.t0 (a int auto_increment primary key, b text null) engine=innodb;
include/sync_slave_sql_with_master.inc
begin;
insert into d0.t0 set a= 1;
begin;
commit;
*** Coordinator must be waiting for Workers have released pending events mem ***
rollback;
set @@global.slave_pending_jobs_size_max= @save_slave_pending_jobs_size_max;
drop database d0;
include/sync_slave_sql_with_master.inc
drop view coord_wait_list;
drop view worker_wait_list;
set @@global.slave_parallel_workers= @save.slave_parallel_workers;
include/rpl_end.inc
