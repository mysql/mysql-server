select @@collation_connection;

#
# Create a table with a nullable varchar(10) column
#  using currect character_set_connection.
create table t1 as select repeat(' ',10) as a union select null;
alter table t1 add key(a);
show create table t1;
insert into t1 values ("a"),("abc"),("abcd"),("hello"),("test");
explain select * from t1 where a like 'abc%';
explain select * from t1 where a like concat('abc','%');
select * from t1 where a like "abc%";
select * from t1 where a like concat("abc","%");
select * from t1 where a like "ABC%";
select * from t1 where a like "test%";
select * from t1 where a like "te_t";
select * from t1 where a like "%a%";
select * from t1 where a like "%abcd%";
select * from t1 where a like "%abc\d%";
drop table t1;

#
# Bug #2619 ucs2 LIKE comparison fails in some cases
#

select 'AA' like 'AA'; 
select 'AA' like 'A%A';
select 'AA' like 'A%%A';
select 'AA' like 'AA%';
select 'AA' like '%AA%';
select 'AA' like '%A';
select 'AA' like '%AA';
select 'AA' like 'A%A%';
select 'AA' like '_%_%';
select 'AA' like '%A%A';
select 'AAA'like 'A%A%A';

select 'AZ' like 'AZ'; 
select 'AZ' like 'A%Z';
select 'AZ' like 'A%%Z';
select 'AZ' like 'AZ%';
select 'AZ' like '%AZ%';
select 'AZ' like '%Z';
select 'AZ' like '%AZ';
select 'AZ' like 'A%Z%';
select 'AZ' like '_%_%';
select 'AZ' like '%A%Z';
select 'AZ' like 'A_';
select 'AZ' like '_Z';
select 'AMZ'like 'A%M%Z';
