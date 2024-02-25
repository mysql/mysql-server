drop table if exists t1,t2,t3,t4,t5,t6,t7;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create table t1 (a text, unique (a(2100))) engine=myisam;
ERROR 42000: Specified key was too long; max key length is 1000 bytes
create table t1 (a text, key (a(2100))) engine=myisam;
Warnings:
Warning	1071	Specified key was too long; max key length is 1000 bytes
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` text,
  KEY `a` (`a`(250))
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
drop table t1;
