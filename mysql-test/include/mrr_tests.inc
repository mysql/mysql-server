
create table t1(a int);
show create table t1;
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t2(a int);
insert into t2 select A.a + 10*(B.a + 10*C.a) from t1 A, t1 B, t1 C;


create table t3 (
  a char(8) not null, b char(8) not null, filler char(200),
  key(a)
);
insert into t3 select @a:=concat('c-', 1000+ A.a, '=w'), @a, 'filler' from t2 A;
insert into t3 select concat('c-', 1000+A.a, '=w'), concat('c-', 2000+A.a, '=w'), 
                      'filler-1' from t2 A;
insert into t3 select concat('c-', 1000+A.a, '=w'), concat('c-', 3000+A.a, '=w'), 
                      'filler-2' from t2 A;

# Test empty result set
select a,filler from t3 where a >= 'c-9011=w';

# Ok, t3.ref_length=6, limit is 64 => 10 elements fit into the buffer
# Test the cases when buffer gets exhausted at different points in source
# intervals:

# 1. Split is in the middle of the range
--sorted_result
select a,filler from t3 where a >= 'c-1011=w' and a <= 'c-1015=w'; 

# 2. Split is at range edge 
--sorted_result
select a,filler from t3 where (a>='c-1011=w' and a <= 'c-1013=w') or
                              (a>='c-1014=w' and a <= 'c-1015=w');

# 3. Split is at range edge, with some rows between ranges.
insert into t3 values ('c-1013=z', 'c-1013=z', 'err');
insert into t3 values ('a-1014=w', 'a-1014=w', 'err');

--sorted_result
select a,filler from t3 where (a>='c-1011=w' and a <= 'c-1013=w') or
                              (a>='c-1014=w' and a <= 'c-1015=w');
delete from t3 where b in ('c-1013=z', 'a-1014=w');

# 4. Split is within the equality range.
--sorted_result
select a,filler from t3 where a='c-1011=w' or a='c-1012=w' or a='c-1013=w' or
                              a='c-1014=w' or a='c-1015=w';

# 5. Split is at the edge of equality range.
insert into t3 values ('c-1013=w', 'del-me', 'inserted');
--sorted_result
select a,filler from t3 where a='c-1011=w' or a='c-1012=w' or a='c-1013=w' or
                              a='c-1014=w' or a='c-1015=w';
delete from t3 where b='del-me';

# PK tests are not included here.

alter table t3 add primary key(b);

##  PK scan tests
# 6. Split is between 'unique' PK ranges
select b,filler from t3 where (b>='c-1011=w' and b<= 'c-1018=w') or 
                              b IN ('c-1019=w', 'c-1020=w', 'c-1021=w', 
                                    'c-1022=w', 'c-1023=w', 'c-1024=w');

# 7. Between non-uniq and uniq range
select b,filler from t3 where (b>='c-1011=w' and b<= 'c-1020=w') or 
                              b IN ('c-1021=w', 'c-1022=w', 'c-1023=w');

# 8. Between uniq and non-uniq range
select b,filler from t3 where (b>='c-1011=w' and b<= 'c-1018=w') or 
                              b IN ('c-1019=w', 'c-1020=w') or 
                              (b>='c-1021=w' and b<= 'c-1023=w');
## End of PK scan tests

#
# Now try different keypart types and special values
#
create table t4 (a varchar(10), b int, c char(10), filler char(200),
                 key idx1 (a, b, c));

# insert buffer_size * 1.5 all-NULL tuples
insert into t4 (filler) select concat('NULL-', 15-a) from t2 order by a limit 15;

insert into t4 (a,b,c,filler) 
  select 'b-1',NULL,'c-1', concat('NULL-', 15-a) from t2 order by a limit 15;
insert into t4 (a,b,c,filler) 
  select 'b-1',NULL,'c-222', concat('NULL-', 15-a) from t2 order by a limit 15;
insert into t4 (a,b,c,filler) 
  select 'bb-1',NULL,'cc-2', concat('NULL-', 15-a) from t2 order by a limit 15;
insert into t4 (a,b,c,filler) 
  select 'zz-1',NULL,'cc-2', 'filler-data' from t2 order by a limit 500;

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t4;
-- enable_result_log
-- enable_query_log

explain 
  select * from t4 where a IS NULL and b IS NULL and (c IS NULL or c='no-such-row1'
                                                      or c='no-such-row2');
select * from t4 where a IS NULL and b IS NULL and (c IS NULL or c='no-such-row1'
                                                    or c='no-such-row2');

explain 
  select * from t4 where (a ='b-1' or a='bb-1') and b IS NULL and (c='c-1' or c='cc-2');
select * from t4 where (a ='b-1' or a='bb-1') and b IS NULL and (c='c-1' or c='cc-2');

select * from t4 ignore index(idx1) where (a ='b-1' or a='bb-1') and b IS NULL and (c='c-1' or c='cc-2');
drop table t1, t2, t3, t4;

#
# Check how ICP works with NULLs and partially-covered indexes
#
create table t1 (a int, b int not null,unique key (a,b),index(b));
insert ignore into t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(null,7),(9,9),(8,8),(7,7),(null,9),(null,9),(6,6);
create table t2 like t1;
insert into t2 select * from t1;
alter table t1 modify b blob not null, add c int not null, drop key a, add unique key (a,b(20),c), drop key b, add key (b(10));

select * from t1 where a is null;
select * from t1 where (a is null or a > 0 and a < 3) and b > 7 limit 3;

select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
drop table t1, t2;

#
# BUG#30622: Incorrect query results for MRR + filesort
# 
CREATE TABLE t1 (
  ID int(10) unsigned NOT NULL AUTO_INCREMENT,
  col1 int(10) unsigned DEFAULT NULL,
  key1 int(10) unsigned NOT NULL DEFAULT '0',
  key2 int(10) unsigned DEFAULT NULL,
  text1 text,
  text2 text,
  col2 smallint(6) DEFAULT '100',
  col3 enum('headers','bodyandsubject') NOT NULL DEFAULT 'bodyandsubject',
  col4 tinyint(3) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (ID),
  KEY (key1),
  KEY (key2)
) AUTO_INCREMENT=6 DEFAULT CHARSET=utf8;

INSERT INTO t1 VALUES
(1,NULL,1130,NULL,'Hello',NULL,100,'bodyandsubject',0),
(2,NULL,1130,NULL,'bye',NULL,100,'bodyandsubject',0),
(3,NULL,1130,NULL,'red',NULL,100,'bodyandsubject',0),
(4,NULL,1130,NULL,'yellow',NULL,100,'bodyandsubject',0),
(5,NULL,1130,NULL,'blue',NULL,100,'bodyandsubject',0);

select * FROM t1 WHERE key1=1130 AND col1 IS NULL ORDER BY text1;

drop table t1;


--echo 
--echo  BUG#37851: Crash in test_if_skip_sort_order tab->select is zero
--echo 
CREATE TABLE t1 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk)
);
INSERT INTO t1 VALUES (1);

CREATE TABLE t2 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  int_key int(11) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY int_key (int_key)
);
INSERT INTO t2 VALUES (1,1),(2,6),(3,0);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

EXPLAIN EXTENDED
SELECT MIN(t1.pk)
FROM t1 WHERE EXISTS (
 SELECT t2.pk
 FROM t2
 WHERE t2.int_key IS NULL
 GROUP BY t2.pk
);

DROP TABLE t1, t2;

-- echo #
-- echo # BUG#42048 Discrepancy between MyISAM and Maria's ICP implementation
-- echo #
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (a int, b char(20), filler char(200), key(a,b(10)));
insert into t1 select A.a + 10*(B.a + 10*C.a), 'bbb','filler' from t0 A, t0 B, t0 C;
update t1 set b=repeat(char(65+a), 20) where a < 25;

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

--echo This must show range + using index condition:
--replace_column 10 x 11 x
explain select * from t1 where a < 10 and b = repeat(char(65+a), 20);
select * from t1 where a < 10 and b = repeat(char(65+a), 20);
drop table t0,t1;

-- echo #
-- echo # BUG#41136: ORDER BY + range access: EXPLAIN shows "Using MRR" while MRR is actually not used
-- echo #
create table t0 (a int);
insert into t0 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t1 (a int, b int, key(a));
insert into t1 select A.a + 10 *(B.a + 10*C.a), A.a + 10 *(B.a + 10*C.a) from t0 A, t0 B, t0 C; 
-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log
-- echo This mustn't show "Using MRR":
explain select * from t1 where a < 20  order by a;
drop table t0, t1;

# Try big rowid sizes
set @read_rnd_buffer_size_save= @@read_rnd_buffer_size;
set read_rnd_buffer_size=64;

# By default InnoDB will fill values only for key parts used by the query,
# which will cause DS-MRR to supply an invalid tuple on scan restoration. 
# This test was originally developed for verifying that DS-MRR's code 
# extra(HA_EXTRA_RETRIEVE_ALL_COLS) call has effect. This has now been 
# replaced by using the table's read_set bitmap.
create table t1(a int);
insert into t1 values (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
create table t2(a char(8), b char(8), c char(8), filler char(100), key k1(a,b,c) );

insert into t2 select 
  concat('a-', 1000 + A.a, '-a'),
  concat('b-', 1000 + B.a, '-b'),
  concat('c-', 1000 + C.a, '-c'),
  'filler'
from t1 A, t1 B, t1 C;

# The use of "force index" is to ensure the query is done as a range scan.
# Without "force index", InnoDB's record count estimate is sometimes 
# ~400 instead of 1000, which causes a table scan.
let query=
select count(length(a) + length(filler))
from t2 force index (k1)
where a>='a-1000-a' and a <'a-1001-a';

eval EXPLAIN $query;
eval $query;

drop table t2;

# Try a very big rowid
create table t2 (a char(100), b char(100), c char(100), d int, 
                 filler char(10), key(d), primary key (a,b,c));
insert into t2 select A.a, B.a, B.a, A.a, 'filler' from t1 A, t1 B;
-- disable_query_log
-- disable_result_log
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log
--replace_column 10 #
explain select * from t2 force index (d) where d < 10;
drop table t2;

drop table t1;
set @@read_rnd_buffer_size= @read_rnd_buffer_size_save;

#
# BUG#33033 "MySQL/InnoDB crashes with simple select range query"
#
create table t1 (f1 int not null, f2 int not null,f3 int not null, f4 char(1), primary key (f1,f2), key ix(f3));

--disable_query_log
let $1=55;

while ($1)
{
  eval insert into t1(f1,f2,f3,f4) values ($1,$1,$1,'A');
  dec $1;
}
--enable_query_log

# The following must not crash:
--sorted_result
select * from t1 where (f3>=5 and f3<=10) or (f3>=1 and f3<=4);

drop table t1;

--echo
--echo BUG#37977: Wrong result returned on GROUP BY + OR + Innodb
--echo
CREATE TABLE t1 (
  `pk` int(11) NOT NULL AUTO_INCREMENT,
  `int_nokey` int(11) NOT NULL,
  `int_key` int(11) NOT NULL,
  `date_key` date NOT NULL,
  `date_nokey` date NOT NULL,
  `time_key` time NOT NULL,
  `time_nokey` time NOT NULL,
  `datetime_key` datetime NOT NULL,
  `datetime_nokey` datetime NOT NULL,
  `varchar_key` varchar(5) DEFAULT NULL,
  `varchar_nokey` varchar(5) DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `int_key` (`int_key`),
  KEY `date_key` (`date_key`),
  KEY `time_key` (`time_key`),
  KEY `datetime_key` (`datetime_key`),
  KEY `varchar_key` (`varchar_key`)
);

INSERT IGNORE INTO t1 VALUES 
(1,5,5,'2009-10-16','2009-10-16','09:28:15','09:28:15','2007-09-14 05:34:08','2007-09-14 05:34:08','qk','qk'),
(2,6,6,'0000-00-00','0000-00-00','23:06:39','23:06:39','0000-00-00 00:00:00','0000-00-00 00:00:00','j','j'),
(3,10,10,'2000-12-18','2000-12-18','22:16:19','22:16:19','2006-11-04 15:42:50','2006-11-04 15:42:50','aew','aew'),
(4,0,0,'2001-09-18','2001-09-18','00:00:00','00:00:00','2004-03-23 13:23:35','2004-03-23 13:23:35',NULL,NULL),
(5,6,6,'2007-08-16','2007-08-16','22:13:38','22:13:38','2004-08-19 11:01:28','2004-08-19 11:01:28','qu','qu');
select pk from t1 WHERE `varchar_key` > 'kr' group by pk;
select pk from t1 WHERE `int_nokey` IS NULL OR  `varchar_key` > 'kr' group by pk;
drop table t1;

--echo #
--echo # BUG#39447: Error with NOT NULL condition and LIMIT 1
--echo #
CREATE TABLE t1 (                 
   id int(11) NOT NULL,                
   parent_id int(11) DEFAULT NULL,     
   name varchar(10) DEFAULT NULL,      
   PRIMARY KEY (id),                   
   KEY ind_parent_id (parent_id)     
);

insert into t1 (id, parent_id, name) values
(10,NULL,'A'),
(20,10,'B'),
(30,10,'C'),
(40,NULL,'D'),
(50,40,'E'),
(60,40,'F'),
(70,NULL,'J');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

SELECT id FROM t1 WHERE parent_id IS NOT NULL ORDER BY id DESC LIMIT 1;
--echo This must show type=index, extra=Using where
# The filtering effect of IS NOT NULL is from stats of the range optimizer.
explain SELECT * FROM t1 WHERE parent_id IS NOT NULL ORDER BY id DESC LIMIT 1;
SELECT * FROM t1 WHERE parent_id IS NOT NULL ORDER BY id DESC LIMIT 1;
drop table t1;

--echo #
--echo # Bug#50381 "Assertion failing in handler.h:1283: 
--echo #            void COST_VECT::add_io(double, double)"
--echo #

CREATE TABLE t1 (
  c1 INT NOT NULL,
  c2 VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (c1)
);

CREATE TABLE t2 (
  c1 INT NOT NULL,
  c2 VARCHAR(1) DEFAULT NULL,
  PRIMARY KEY (c1)
);

INSERT INTO t2 VALUES (10,'v');
INSERT INTO t2 VALUES (11,'r');
 
SELECT t1.c2  
FROM t2 STRAIGHT_JOIN t1 ON t1.c1 < t2.c1;

DROP TABLE t1, t2;

--echo #
--echo # Bug#58463: Error Can't find record on SELECT with JOIN and ORDER BY
--echo #

# To produce the same query plan as in the bug report the first table
# must be stored in MyISAM.
CREATE TABLE t1 (
  pk INT NOT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (2);

CREATE TABLE t2 (
  pk INT NOT NULL,
  i1 INT NOT NULL,
  i2 INT NOT NULL,
  c1 VARCHAR(1024) CHARACTER SET utf8,
  PRIMARY KEY (pk),
  KEY k1 (i1)
);

INSERT INTO t2 VALUES (3, 9, 1, NULL);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let query=
SELECT i1
FROM t1 LEFT JOIN t2 ON t1.pk = t2.i2
WHERE t2.i1 > 5
AND t2.pk IS  NULL
ORDER BY i1;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

--echo #
--echo # Bug#12321461: CRASH IN DSMRR_IMPL::DSMRR_INIT ON SELECT STRAIGHT_JOIN
--echo #

# This test should run without join buffering
set @save_optimizer_switch = @@optimizer_switch;
set optimizer_switch='block_nested_loop=off,batched_key_access=off';

CREATE TABLE t1 (
  pk INTEGER,
  c1 VARCHAR(1) NOT NULL,
  PRIMARY KEY (pk)
);

CREATE TABLE t2 (
  c1 VARCHAR(1) NOT NULL
);

INSERT INTO t2 VALUES ('v'), ('c');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let query=
SELECT STRAIGHT_JOIN t1.c1
FROM t1 RIGHT OUTER JOIN t2 ON t1.c1 = t2.c1
WHERE t1.pk > 176;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2;

# Restore join buffer settings to their original values
set optimizer_switch= @save_optimizer_switch;

--echo #
--echo # Bug#13249966 MRR: RANDOM ERROR DUE TO UNINITIALIZED RES WITH 
--echo #              SMALL READ_RND_BUFFER_SIZE
--echo #

set @read_rnd_buffer_size_save= @@read_rnd_buffer_size;
set read_rnd_buffer_size=1;
select @@read_rnd_buffer_size;

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  i2 INTEGER NOT NULL,
  KEY (i2)
);

INSERT INTO t1 VALUES (0,1),(1,2),(2,3);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

let query=
SELECT i1
FROM t1
WHERE i2 > 2;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1;

set @@read_rnd_buffer_size= @read_rnd_buffer_size_save;
select @@read_rnd_buffer_size;

--echo #
--echo # Bug 12365385 STRAIGHT_JOIN QUERY QUICKLY EXHAUSTS SYSTEM+VIRT. 
--echo #              MEMORY LEADING TO SYSTEM CRASH
--echo #

CREATE TABLE ten (a INTEGER);
INSERT INTO ten VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t1 (
  pk INTEGER NOT NULL,
  i1 INTEGER NOT NULL,
  c1 VARCHAR(10) NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t1
  SELECT a, 1, 'MySQL' FROM ten;

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  c1 VARCHAR(10) NOT NULL,
  c2 varchar(10) NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t2
  SELECT a, 'MySQL', 'MySQL' FROM ten;

CREATE TABLE t3 (
  pk INTEGER NOT NULL,
  c1 VARCHAR(10) NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t3
  SELECT a, 'MySQL' FROM ten;

CREATE TABLE t4 (
  pk int(11) NOT NULL,
  c1_key varchar(10) CHARACTER SET utf8 NOT NULL,
  c2 varchar(10) NOT NULL,
  c3 varchar(10) NOT NULL,
  PRIMARY KEY (pk),
  KEY k1 (c1_key)
);

# t4 is empty

CREATE TABLE t5 (
  pk INTEGER NOT NULL,
  c1 VARCHAR(10) NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t5
  SELECT a, 'MySQL' FROM ten;

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;
ANALYZE TABLE t4;
ANALYZE TABLE t5;
-- enable_result_log
-- enable_query_log

let query=
SELECT STRAIGHT_JOIN *
FROM 
  (t1 LEFT JOIN 
     (t2 LEFT JOIN
       (t3 LEFT OUTER JOIN t4 ON t3.c1 <= t4.c1_key)
      ON t2.c1 = t4.c3) 
   ON t1.c1 = t4.c2)
  RIGHT OUTER JOIN t5 ON t2.c2 <= t5.c1
WHERE t1.i1 = 1;

eval EXPLAIN $query;
eval $query;

DROP TABLE ten, t1, t2, t3, t4, t5;

--echo #
--echo # Bug 18172819 CRASH ON DSMRR_IMPL::CHOOSE_MRR_IMPL IN SQL/HANDLER.CC 
--echo #

# Test needs to run with big_tables ON
set @big_tables_save= @@big_tables;
set @optimizer_switch_save=@@optimizer_switch;
set big_tables=ON;
set optimizer_switch='derived_merge=off';
SELECT @@big_tables;

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1)
);

INSERT INTO t1 VALUES (1,'a'), (2, NULL);

CREATE TABLE t2 (
  c1 VARCHAR(1),
  i1 INTEGER NOT NULL,
  KEY (c1)
);

INSERT INTO t2 VALUES ('a', 1), (NULL, 2);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let query=
SELECT *
FROM
  ( SELECT * FROM  t1 ) AS d1
WHERE d1.c1 IN ( SELECT c1 FROM t2 )
  AND d1.c1 IS NULL
ORDER BY d1.i1;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

# Restore the configuration for big_tables
set big_tables=@big_tables_save;
set optimizer_switch=@optimizer_switch_save;
