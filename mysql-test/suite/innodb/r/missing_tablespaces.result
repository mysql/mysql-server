#
# Bug#19419026 WHEN A TABLESPACE IS NOT FOUND, DO NOT REPORT "TABLE NOT FOUND"
#
CREATE DATABASE `..................................................`;
USE `..................................................`;
CREATE TABLE `..................................................` (id int) engine=innodb;
# restart
select * from `..................................................`;
ERROR HY000: Tablespace is missing for table `..................................................`.`..................................................`.
DROP TABLE `..................................................`;
DROP DATABASE `..................................................`;
#
# Bug #26761960	TABLESPACE WITH SPECIAL CHARACTER MAY
#		BE NOT FOUND AFTER RESTART
#
create database `test#_1`;
create table `test#_1`.t1(f1 int);
insert into `test#_1`.t1 values(1);
insert into `test#_1`.t1 select * from `test#_1`.t1;
# Kill and restart
insert into `test#_1`.t1 values(2);
drop table `test#_1`.t1;
drop database `test#_1`;
