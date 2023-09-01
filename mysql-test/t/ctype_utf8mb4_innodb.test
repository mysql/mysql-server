let $engine= InnoDB;
--source include/ctype_utf8mb4.inc

#
# Bug 4521: unique key prefix interacts poorly with utf8mb4
# InnoDB: keys with prefix compression, case insensitive collation.
#
--disable_warnings
create table t1 (c varchar(30) character set utf8mb4, unique(c(10))) engine=innodb;
--enable_warnings
insert into t1 values ('1'),('2'),('3'),('x'),('y'),('z');
insert into t1 values ('aaaaaaaaaa');
--error ER_DUP_ENTRY
insert into t1 values ('aaaaaaaaaaa');
--error ER_DUP_ENTRY
insert into t1 values ('aaaaaaaaaaaa');
insert into t1 values (repeat('b',20));
select c c1 from t1 where c='1';
select c c2 from t1 where c='2';
select c c3 from t1 where c='3';
select c cx from t1 where c='x';
select c cy from t1 where c='y';
select c cz from t1 where c='z';
select c ca10 from t1 where c='aaaaaaaaaa';
select c cb20 from t1 where c=repeat('b',20);
drop table t1;

#
# Bug 4521: unique key prefix interacts poorly with utf8mb4
# InnoDB: fixed length keys, case insensitive collation
#
--disable_warnings
create table t1 (c char(3) character set utf8mb4, unique (c(2))) engine=innodb;
--enable_warnings
insert into t1 values ('1'),('2'),('3'),('4'),('x'),('y'),('z');
insert into t1 values ('a');
insert into t1 values ('aa');
--error ER_DUP_ENTRY
insert into t1 values ('aaa');
insert into t1 values ('b');
insert into t1 values ('bb');
--error ER_DUP_ENTRY
insert into t1 values ('bbb');
insert into t1 values ('а');
insert into t1 values ('аа');
--error ER_DUP_ENTRY
insert into t1 values ('ааа');
insert into t1 values ('б');
insert into t1 values ('бб');
--error ER_DUP_ENTRY
insert into t1 values ('ббб');
insert into t1 values ('ꪪ');
insert into t1 values ('ꪪꪪ');
--error ER_DUP_ENTRY
insert into t1 values ('ꪪꪪꪪ');
drop table t1;

# Bug 4531: unique key prefix interacts poorly with utf8mb4
# Check BDB, case insensitive collation
#
--disable_warnings
create table t1 (
c char(10) character set utf8mb4,
unique key a (c(1))
) engine=innodb;
--enable_warnings
insert into t1 values ('a'),('b'),('c'),('d'),('e'),('f');
--error ER_DUP_ENTRY
insert into t1 values ('aa');
--error ER_DUP_ENTRY
insert into t1 values ('aaa');
insert into t1 values ('б');
--error ER_DUP_ENTRY
insert into t1 values ('бб');
--error ER_DUP_ENTRY
insert into t1 values ('ббб');
select c as c_all from t1 order by c;
select c as c_a from t1 where c='a';
select c as c_a from t1 where c='б';
drop table t1;

#
# Bug 4531: unique key prefix interacts poorly with utf8mb4
# Check BDB, binary collation
#
--disable_warnings
create table t1 (
c char(10) character set utf8mb4 collate utf8mb4_bin,
unique key a (c(1))
) engine=innodb;
--enable_warnings
insert into t1 values ('a'),('b'),('c'),('d'),('e'),('f');
--error ER_DUP_ENTRY
insert into t1 values ('aa');
--error ER_DUP_ENTRY
insert into t1 values ('aaa');
insert into t1 values ('Ã±');
--error ER_DUP_ENTRY
insert into t1 values ('Ã±Ã±');
--error ER_DUP_ENTRY
insert into t1 values ('Ã±Ã±Ã±');
select c as c_all from t1 order by c;
select c as c_a from t1 where c='a';
select c as c_a from t1 where c='Ã±';
drop table t1;

# Bug#4594: column index make = failed for gbk, but like works
# Check InnoDB
#
--disable_warnings
create table t1 (
  str varchar(255) character set utf8mb4 not null,
  key str  (str(2))
) engine=innodb;
--enable_warnings
INSERT INTO t1 VALUES ('str');
INSERT INTO t1 VALUES ('str2');
select * from t1 where str='str';
drop table t1;

# the same for BDB
#

--disable_warnings
create table t1 (
  str varchar(255) character set utf8mb4 not null,
  key str (str(2))
) engine=innodb;
--enable_warnings
INSERT INTO t1 VALUES ('str');
INSERT INTO t1 VALUES ('str2');
select * from t1 where str='str';
drop table t1;

#
# Bug #5723: length(<varchar utf8mb4 field>) returns varying results
#
--disable_warnings
SET NAMES utf8mb4;
--disable_warnings
CREATE TABLE t1 (
  subject varchar(255) character set utf8mb4 collate utf8mb4_unicode_ci,
  p varchar(15) character set utf8mb4
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
--enable_warnings
INSERT INTO t1 VALUES ('谷川俊二と申しますが、インターネット予約の会員登録をしましたところ、メールアドレスを間違えてしまい会員ＩＤが受け取ることが出来ませんでした。間違えアドレスはtani-shun@n.vodafone.ne.jpを書き込みました。どうすればよいですか？ その他、住所等は間違えありません。連絡ください。よろしくお願いします。m(__)m','040312-000057');
INSERT INTO t1 VALUES ('aaa','bbb');
SELECT length(subject) FROM t1;
SELECT length(subject) FROM t1 ORDER BY 1;
DROP TABLE t1;

# additional tests from duplicate bug#20744 MySQL return no result

set names utf8mb4;
--disable_warnings
create table t1 (a varchar(30) not null primary key)
engine=innodb  default character set utf8mb4 collate utf8mb4_general_ci;
--enable_warnings
insert into t1 values ('あいうえおかきくけこさしすせそ');
insert into t1 values ('さしすせそかきくけこあいうえお');
select a as gci1 from t1 where a like 'さしすせそかきくけこあいうえお%';
select a as gci2 from t1 where a like 'あいうえおかきくけこさしすせそ';
drop table t1;

set names utf8mb4;
--disable_warnings
create table t1 (a varchar(30) not null primary key)
engine=innodb default character set utf8mb4 collate utf8mb4_unicode_ci;
--enable_warnings
insert into t1 values ('あいうえおかきくけこさしすせそ');
insert into t1 values ('さしすせそかきくけこあいうえお');
select a as uci1 from t1 where a like 'さしすせそかきくけこあいうえお%';
select a as uci2 from t1 where a like 'あいうえおかきくけこさしすせそ';
drop table t1;

set names utf8mb4;
--disable_warnings
create table t1 (a varchar(30) not null primary key)
engine=innodb default character set utf8mb4 collate utf8mb4_bin;
--enable_warnings
insert into t1 values ('あいうえおかきくけこさしすせそ');
insert into t1 values ('さしすせそかきくけこあいうえお');
select a as bin1 from t1 where a like 'さしすせそかきくけこあいうえお%';
select a as bin2 from t1 where a like 'あいうえおかきくけこさしすせそ';
drop table t1;

#
# Bug #6019 SELECT tries to use too short prefix index on utf8mb4 data
#
set names utf8mb4;
--disable_warnings
create table t1 (
  a int primary key,
  b varchar(6),
  index b3(b(3))
) engine=innodb character set=utf8mb4;
--enable_warnings
insert into t1 values(1,'foo'),(2,'foobar');
select * from t1 where b like 'foob%';
--disable_warnings
alter table t1 engine=innodb;
--enable_warnings
select * from t1 where b like 'foob%';
drop table t1;

#
# Bug#19960: Inconsistent results when joining
# InnoDB tables using partial utf8mb3 indexes
#
--disable_warnings
CREATE TABLE t1 (
  colA int(11) NOT NULL,
  colB varchar(255) character set utf8mb4 NOT NULL,
   PRIMARY KEY  (colA)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
--enable_warnings
INSERT INTO t1 (colA, colB) VALUES (1, 'foo'), (2, 'foo bar');
--disable_warnings
CREATE TABLE t2 (
  colA int(11) NOT NULL,
  colB varchar(255) character set utf8mb4 NOT NULL,
   KEY bad  (colA,colB(3))
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
--enable_warnings
INSERT INTO t2 (colA, colB) VALUES (1, 'foo'),(2, 'foo bar');
SELECT * FROM t1 JOIN t2 ON t1.colA=t2.colA AND t1.colB=t2.colB
WHERE t1.colA < 3;
DROP TABLE t1, t2;

