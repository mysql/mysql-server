#
# Test problem with characters < ' ' at end of strings (Bug #3152)
#

--disable_warnings
drop table if exists t1;
--enable_warnings

set names utf8mb4 collate utf8mb4_unicode_ci;

-- source include/endspace.inc

set names default;
#
# Test MyISAM tables.
#

create table t1 (text1 varchar(32) not NULL, KEY key1 (text1)) charset latin1;
insert into t1 values ('teststring'), ('nothing'), ('teststring\t');
-- disable_result_log
analyze table t1;
-- enable_result_log
check table t1;
select * from t1 ignore key (key1) where text1='teststring' or 
  text1 like 'teststring_%' ORDER BY text1;
--sorted_result
select * from t1 where text1='teststring' or text1 like 'teststring_%';
--sorted_result
select * from t1 where text1='teststring' or text1 > 'teststring\t';
select * from t1 order by text1;
explain select * from t1 order by text1;

alter table t1 modify text1 char(32) binary not null;
check table t1;
select * from t1 ignore key (key1) where text1='teststring' or 
  text1 like 'teststring_%' ORDER BY text1;
select concat('|', text1, '|') as c from t1 where text1='teststring' or text1 like 'teststring_%' order by c;
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring' or text1 > 'teststring\t';
select text1, length(text1) from t1 order by text1;
select text1, length(text1) from t1 order by binary text1;

alter table t1 modify text1 blob not null, drop key key1, add key key1 (text1(20));
insert into t1 values ('teststring ');
select concat('|', text1, '|') from t1 order by text1;
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring' or text1 > 'teststring\t';
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring';
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring ';

alter table t1 modify text1 text not null, pack_keys=1;
-- disable_result_log
analyze table t1;
-- enable_result_log
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring';
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring ';
explain select concat('|', text1, '|') from t1 where text1='teststring ';
--sorted_result
select concat('|', text1, '|') from t1 where text1 like 'teststring_%';
select concat('|', text1, '|') as c from t1 where text1='teststring' or text1 like 'teststring_%' order by c;
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring' or text1 > 'teststring\t';
select concat('|', text1, '|') from t1 order by text1;
drop table t1;

create table t1 (text1 varchar(32) not NULL, KEY key1 (text1)) charset latin1 pack_keys=0;
insert into t1 values ('teststring'), ('nothing'), ('teststring\t');
select concat('|', text1, '|') as c from t1 where text1='teststring' or text1 like 'teststring_%' order by c;
--sorted_result
select concat('|', text1, '|') from t1 where text1='teststring' or text1 >= 'teststring\t';
drop table t1;

# Test HEAP tables (with BTREE keys)

create table t1 (text1 varchar(32) not NULL, KEY key1 using BTREE (text1))
charset latin1 engine=heap;
insert into t1 values ('teststring'), ('nothing'), ('teststring\t');
select * from t1 ignore key (key1) where text1='teststring' or 
  text1 like 'teststring_%' ORDER BY text1;
--sorted_result
select * from t1 where text1='teststring' or text1 like 'teststring_%';
--sorted_result
select * from t1 where text1='teststring' or text1 >= 'teststring\t';
select * from t1 order by text1;
explain select * from t1 order by text1;

alter table t1 modify text1 char(32) binary not null;
select * from t1 order by text1;
drop table t1;

#
# Test InnoDB tables
#

create table t1 (text1 varchar(32) not NULL, KEY key1 (text1))
charset latin1 engine=innodb;
insert into t1 values ('teststring'), ('nothing'), ('teststring\t');
-- disable_result_log
analyze table t1;
-- enable_result_log
check table t1;
--sorted_result
select * from t1 where text1='teststring' or text1 like 'teststring_%';
--sorted_result
select * from t1 where text1='teststring' or text1 > 'teststring\t';
select * from t1 order by text1;
explain select * from t1 order by text1;

alter table t1 modify text1 char(32) binary not null;
select * from t1 order by text1;

alter table t1 modify text1 blob not null, drop key key1, add key key1 (text1(20));
insert into t1 values ('teststring ');
--sorted_result
select concat('|', text1, '|') from t1 order by text1;

alter table t1 modify text1 text not null, pack_keys=1;
select * from t1 where text1 like 'teststring_%';

# The following gives wrong result in InnoDB
--sorted_result
select text1, length(text1) from t1 where text1='teststring' or text1 like 'teststring_%';
--sorted_result
select text1, length(text1) from t1 where text1='teststring' or text1 >= 'teststring\t';
select concat('|', text1, '|') from t1 order by text1;
drop table t1;

# End of 4.1 tests
