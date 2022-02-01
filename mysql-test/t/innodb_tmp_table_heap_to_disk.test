
# This test takes rather long time so let us run it only in --big-test mode
--source include/big_test.inc
# Valgrind will make the test Timeout so skip it if only in --valgrind mode
--source include/not_valgrind.inc
# Times out with ubsan
--source include/not_ubsan.inc

#from heap to ondisk
set @old_heap_size= @@max_heap_table_size;
set max_heap_table_size = 16384;
#system table
select * from information_schema.collations order by id limit 1;

#sj_weedout
#tmp table for semi-join weedout
if (`select locate('firstmatch', @@optimizer_switch) > 0`) 
{
    set optimizer_switch='firstmatch=off';
}
if (`select locate('loosescan', @@optimizer_switch) > 0`) 
{
    set optimizer_switch='loosescan=off';
}
drop table if exists t1;
create table t1 (
  a int not null,
  b int not null,
  c datetime default null
);

insert into t1 values 
(406989,67,'2006-02-23 17:08:46'), (150078,67,'2005-10-26 11:17:45'),
(406993,67,'2006-02-27 11:20:57'), (245655,67,'2005-12-08 15:59:08'),
(406994,67,'2006-02-27 11:26:46'), (256,67,null),
(398341,67,'2006-02-20 04:48:44'), (254,67,null),(1120,67,null),
(406988,67,'2006-02-23 17:07:22'), (255,67,null),
(398340,67,'2006-02-20 04:38:53'),(406631,67,'2006-02-23 10:49:42'),
(245653,67,'2005-12-08 15:59:07'),(406992,67,'2006-02-24 16:47:18'),
(245654,67,'2005-12-08 15:59:08'),(406995,67,'2006-02-28 11:55:00'),
(127261,67,'2005-10-13 12:17:58'),(406991,67,'2006-02-24 16:42:32'),
(245652,67,'2005-12-08 15:58:27'),(398545,67,'2006-02-20 04:53:13'),
(154504,67,'2005-10-28 11:53:01'),(9199,67,null),(1,67,'2006-02-23 15:01:35'),
(223456,67,null),(4101,67,null),(1133,67,null),
(406990,67,'2006-02-23 18:01:45'),(148815,67,'2005-10-25 15:34:17'),
(148812,67,'2005-10-25 15:30:01'),(245651,67,'2005-12-08 15:58:27');
insert into t1 select a+10, b+10, c from t1;
insert into t1 select a+20, b+20, c from t1;
insert into t1 select a+30, b+30, c from t1;
insert into t1 select a+40, b+40, c from t1;
insert into t1 select a+50, b+50, c from t1;
insert into t1 select a+60, b+60, c from t1;
insert into t1 select a+70, b+70, c from t1;
insert into t1 select a+80, b+80, c from t1;
drop table if exists t11;
create table t11 select * from t1;
insert into t1 select a+80, b+80, c from t1;
--replace_column 10 x
explain select * from t1 where a in (select a from t11) order by 1, 2, 3 limit 1;
select * from t1 where a in (select a from t11) order by 1, 2, 3 limit 1;
if (`select locate('materialization', @@optimizer_switch) > 0`) 
{
    set optimizer_switch='materialization=off';
}
--replace_column 10 x
explain select * from t1 where a in (select a from t11) order by 1, 2, 3 limit 1;
select * from t1 where a in (select a from t11) order by 1, 2, 3 limit 1;
drop table t11;
set optimizer_switch=default;
#union end_write end_update end_write_group
drop table if exists t1, t2;
CREATE TABLE t1 (id INTEGER);
CREATE TABLE t2 (id INTEGER);

INSERT INTO t1 (id) VALUES (1), (1), (1),(1);
INSERT INTO t1 (id) SELECT id FROM t1; /* 8 */
INSERT INTO t1 (id) SELECT id FROM t1; /* 12 */
INSERT INTO t1 (id) SELECT id FROM t1; /* 16 */
INSERT INTO t1 (id) SELECT id FROM t1; /* 20 */
INSERT INTO t1 (id) SELECT id FROM t1; /* 24 */
INSERT INTO t1 SELECT id+1 FROM t1;
INSERT INTO t1 SELECT id+2 FROM t1;
INSERT INTO t1 SELECT id+4 FROM t1;
INSERT INTO t1 SELECT id+8 FROM t1;
INSERT INTO t1 SELECT id+16 FROM t1;
INSERT INTO t1 SELECT id+32 FROM t1;
INSERT INTO t1 SELECT id+64 FROM t1;
INSERT INTO t1 SELECT id+128 FROM t1;
INSERT INTO t1 SELECT id+256 FROM t1;
INSERT INTO t1 SELECT id+512 FROM t1;
INSERT INTO t1 SELECT id+1024 FROM t1;
#end_write
INSERT INTO t2 SELECT id + 2000 FROM t1 limit 4000; 
#union
--replace_column 10 x
explain select * from t1 union select * from t2 order by 1 limit 1;
select * from t1 union select * from t2 order by 1 limit 1;
#end_update
--replace_column 10 x
explain SELECT SUM(id) sm FROM t1 group by id order by sm limit 1;
SELECT SUM(id) sm FROM t1 group by id order by sm limit 1;
#end_write_group
--replace_column 10 x
explain SELECT SUM(DISTINCT t1.id) sm FROM t1 left join t2 on t1.id=t2.id GROUP BY t1.id order by sm limit 1;
SELECT SUM(DISTINCT t1.id) sm FROM t1 left join t2 on t1.id=t2.id GROUP BY t1.id order by sm limit 1;
#missing rollup
set max_heap_table_size = @old_heap_size;
#============================end && cleanup======================= 
drop table t1;
drop table t2;
--echo # 
