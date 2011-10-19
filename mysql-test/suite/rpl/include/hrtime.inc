--source include/master-slave.inc

set time_zone='+03:00';
set timestamp=unix_timestamp('2011-01-01 01:01:01') + 0.123456;

create table t1 (a timestamp(4), b varchar(100), c datetime(2));

insert t1 (b,c) values (now(6), now(6));

insert t1 values ('2010-10-10 10:10:10.101010','2010-10-10 10:10:10.101010','2010-10-10 10:10:10.101010');

set timestamp=unix_timestamp('2022-02-02 02:02:02') + 0.654321;
insert t1 (b,c) values (now(), now());
insert t1 (b,c) values (0,0);
insert t1 (a,b,c) values (0,0,now(6));

select * from t1;

sync_slave_with_master;
connection slave;
set time_zone='+03:00';
select * from t1;
connection master;
drop table t1;

--source include/rpl_end.inc

