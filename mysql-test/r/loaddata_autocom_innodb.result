SET SESSION DEFAULT_STORAGE_ENGINE = InnoDB;
drop table if exists t1;
create table t1 (id int unsigned not null auto_increment primary key, a text, b text);
start transaction;
load data infile 'LOAD_FILE' ignore into table t1 fields terminated by ',' enclosed by '''' (a, b);
Warnings:
Warning	1261	Row 3 doesn't contain data for all columns
commit;
select count(*) from t1;
count(*)
4
truncate table t1;
start transaction;
load data infile 'LOAD_FILE' ignore into table t1 fields terminated by ',' enclosed by '''' (a, b);
Warnings:
Warning	1261	Row 3 doesn't contain data for all columns
rollback;
select count(*) from t1;
count(*)
0
drop table t1;
