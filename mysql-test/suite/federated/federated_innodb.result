CREATE DATABASE federated;
CREATE DATABASE federated;
create table federated.t1 (a int primary key, b varchar(64))
engine=myisam;
create table federated.t1 (a int primary key, b varchar(64))
engine=federated
connection='mysql://root@127.0.0.1:SLAVE_PORT/federated/t1';
insert into federated.t1 values (1,"Larry"), (2,"Curly"), (1,"Moe");
ERROR 23000: Can't write; duplicate key in table 't1'
select * from federated.t1;
a	b
1	Larry
2	Curly
truncate federated.t1;
alter table federated.t1 engine=innodb;
insert into federated.t1 values (1,"Larry"), (2,"Curly"), (1,"Moe");
ERROR 23000: Can't write; duplicate key in table 't1'
select * from federated.t1;
a	b
drop table federated.t1;
drop table federated.t1;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
