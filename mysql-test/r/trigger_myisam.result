create table t1 (i int, at int, k int, key(k)) engine=myisam;
create table t2 (i int);
insert into t1 values (1, 1, 1);
insert into t2 values (1), (2), (3);
create trigger ai after insert on t1 for each row set @a:= new.at;
create trigger au after update on t1 for each row set @a:= new.at;
create trigger ad after delete on t1 for each row set @a:= old.at;
alter table t1 drop column at;
select * from t1;
i	k
1	1
insert into t1 values (2, 1);
ERROR 42S22: Unknown column 'at' in 'NEW'
select * from t1;
i	k
1	1
2	1
update t1 set k = 2 where i = 2;
ERROR 42S22: Unknown column 'at' in 'NEW'
select * from t1;
i	k
1	1
2	2
delete from t1 where i = 2;
ERROR 42S22: Unknown column 'at' in 'OLD'
select * from t1;
i	k
1	1
load data infile '../../std_data/loaddata5.dat' into table t1 fields terminated by '' enclosed by '' (i, k);
ERROR 42S22: Unknown column 'at' in 'NEW'
select * from t1;
i	k
1	1
1	2
insert into t1 select 3, 3;
ERROR 42S22: Unknown column 'at' in 'NEW'
select * from t1;
i	k
1	1
1	2
3	3
update t1, t2 set k = k + 10 where t1.i = t2.i;
ERROR 42S22: Unknown column 'at' in 'NEW'
select * from t1;
i	k
1	11
1	2
3	3
update t1, t2 set k = k + 10 where t1.i = t2.i and k < 3;
ERROR 42S22: Unknown column 'at' in 'NEW'
select * from t1;
i	k
1	11
1	12
3	3
delete t1, t2 from t1 straight_join t2 where t1.i = t2.i;
ERROR 42S22: Unknown column 'at' in 'OLD'
select * from t1;
i	k
1	12
3	3
delete t2, t1 from t2 straight_join t1 where t1.i = t2.i;
ERROR 42S22: Unknown column 'at' in 'OLD'
select * from t1;
i	k
3	3
alter table t1 add primary key (i);
insert into t1 values (3, 4) on duplicate key update k= k + 10;
ERROR 42S22: Unknown column 'at' in 'NEW'
select * from t1;
i	k
3	13
replace into t1 values (3, 3);
ERROR 42S22: Unknown column 'at' in 'OLD'
select * from t1;
i	k
drop table t1, t2;
CREATE TABLE t1 (id INTEGER) ENGINE=MyISAM;
CREATE TABLE t2 (id INTEGER) ENGINE=MyISAM;
INSERT INTO t2 VALUES (1),(2);
CREATE TRIGGER t1_test AFTER INSERT ON t1 FOR EACH ROW
INSERT INTO t2 VALUES (new.id);
SELECT GET_LOCK('B26162',120);
GET_LOCK('B26162',120)
1
SELECT 'rl_acquirer', GET_LOCK('B26162',120), id FROM t2 WHERE id = 1;
SET SESSION LOW_PRIORITY_UPDATES=1;
SET GLOBAL LOW_PRIORITY_UPDATES=1;
INSERT INTO t1 VALUES (5);
SELECT 'rl_contender', id FROM t2 WHERE id > 1;
SELECT RELEASE_LOCK('B26162');
RELEASE_LOCK('B26162')
1
rl_acquirer	GET_LOCK('B26162',120)	id
rl_acquirer	1	1
SELECT RELEASE_LOCK('B26162');
RELEASE_LOCK('B26162')
1
rl_contender	id
rl_contender	2
DROP TRIGGER t1_test;
DROP TABLE t1,t2;
SET SESSION LOW_PRIORITY_UPDATES=DEFAULT;
SET GLOBAL LOW_PRIORITY_UPDATES=DEFAULT;
CREATE TABLE t1 (id INT NOT NULL) ENGINE=MyISAM;
CREATE TABLE t2 (id INT NOT NULL) ENGINE=MyISAM;
INSERT t1 VALUES (1),(2),(3);
UPDATE IGNORE t1 SET id=NULL;
Warnings:
Warning	1048	Column 'id' cannot be null
Warning	1048	Column 'id' cannot be null
Warning	1048	Column 'id' cannot be null
CREATE TRIGGER t1_bu BEFORE UPDATE ON t1 FOR EACH ROW
INSERT INTO t2 VALUES (3);
UPDATE t1 SET id=NULL;
Warnings:
Warning	1048	Column 'id' cannot be null
Warning	1048	Column 'id' cannot be null
Warning	1048	Column 'id' cannot be null
DROP TRIGGER t1_bu;
DROP TABLE t1,t2;
