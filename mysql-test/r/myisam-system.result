drop table if exists t1,t2;
create table t1 (a int) engine=myisam;
drop table t1;
ERROR HY000: Storage engine can't drop table 'test.t1' because it is missing. Use DROP TABLE IF EXISTS to remove it from data-dictionary.
select * from t1;
ERROR HY000: Can't find file: 't1' (errno: 2 - No such file or directory)
drop table if exists t1;
Warnings:
Warning	1017	Can't find file: 't1' (errno: 2 - No such file or directory)
create table t1 (a int) engine=myisam;
drop table t1;
ERROR HY000: Storage engine can't drop table 'test.t1' because it is missing. Use DROP TABLE IF EXISTS to remove it from data-dictionary.
select * from t1;
ERROR HY000: Can't find file: 't1' (errno: 2 - No such file or directory)
drop table if exists t1;
Warnings:
Warning	1017	Can't find file: 't1' (errno: 2 - No such file or directory)
drop table t1;
ERROR 42S02: Unknown table 'test.t1'
