include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create database d1;
use d1;
create table t (s1 int) engine=innodb;
set @@autocommit=0;
start transaction;
insert into t values (1);
create user x@y;
grant select on t to x@y;
rollback;
show grants for x@y;
Grants for x@y
GRANT USAGE ON *.* TO `x`@`y`
GRANT SELECT ON `d1`.`t` TO `x`@`y`
start transaction;
insert into t values (2);
revoke select on t from x@y;
commit;
select * from t;
s1
1
2
show grants for x@y;
Grants for x@y
GRANT USAGE ON *.* TO `x`@`y`
drop user x@y;
drop database d1;
include/rpl_end.inc
