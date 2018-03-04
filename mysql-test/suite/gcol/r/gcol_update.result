set global innodb_purge_stop_now = 1;
create table t1(f1 int not null, f2 blob not null, f3 blob not null,
vchar char(2) as (substr(f3,2,2)) virtual,
primary key(f1, f3(5)), index(vchar))engine=innodb;
insert into t1(f1,f2,f3) values(1, repeat('a',8000), repeat('b', 9000));
update t1 set f1=5 where f1=1;
delete from t1 where f1=5;
set global innodb_purge_run_now=1;
set global innodb_fast_shutdown=0;
# restart
set global innodb_purge_stop_now = 1;
drop table t1;
create table t1(f1 int not null, f2 blob not null, f3 blob not null,
vchar char(2) as (substr(f3,2,2)) virtual,
primary key(f1, f3(5)), index(vchar, f3(2)))engine=innodb;
insert into t1(f1,f2,f3) values(1, repeat('a',8000), repeat('b', 9000));
update t1 set f1=5 where f1=1;
delete from t1 where f1=5;
set global innodb_purge_run_now=1;
set global innodb_fast_shutdown=0;
# restart
set global innodb_purge_stop_now = 1;
drop table t1;
create table t1(f1 int not null, f2 blob not null, f3 blob not null,
vchar blob as (f3) virtual,
primary key(f1, f3(5)), index(vchar(3)))engine=innodb;
insert into t1(f1,f2,f3) values(1, repeat('a',8000), repeat('b', 9000));
update t1 set f1=5 where f1=1;
delete from t1 where f1=5;
set global innodb_purge_run_now=1;
set global innodb_fast_shutdown=0;
# restart
drop table t1;
#
# Bug #26838771: GCOL: INCORRECT BEHAVIOR WITH UPDATE ON FLY
#
CREATE TABLE t1(c1 INT);
CREATE TABLE t2(c1 INT, c2 JSON, c3 varchar(15) GENERATED ALWAYS AS
(concat(c2,_utf8mb4'x')) VIRTUAL);
CREATE TABLE t3(c1 JSON, c2 INT GENERATED ALWAYS AS ((c1 + 1)) VIRTUAL);
INSERT INTO t2(c1,c2) VALUES(1, '{"tr": "x"}'), (2, '{"tr": "x"}');
INSERT INTO t3(c1) VALUES(CAST(7 AS JSON));
UPDATE t3 SET c2 = 0;
ERROR HY000: The value specified for generated column 'c2' in table 't3' is not allowed.
UPDATE t3 JOIN t2 ON (t3.c1 = t2.c1) SET t3.c2 = 0;
ERROR HY000: The value specified for generated column 'c2' in table 't3' is not allowed.
UPDATE t1 RIGHT JOIN
t2 ON (t1.c1 = t2.c1),
t3
SET t2.c2 = 'tr', t3.c2 = 0;
ERROR HY000: The value specified for generated column 'c2' in table 't3' is not allowed.
DROP TABLE t1,t2,t3;
