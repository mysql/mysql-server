# Initialise
--disable_warnings
drop table if exists t1,t2,t3;
--enable_warnings

disable_query_log;
select "--- Testing varchar ---";
enable_query_log;

#
# Simple basic test that endspace is saved
#

create table t1 (v varchar(10), c char(10), t text);
insert into t1 values('+ ', '+ ', '+ ');
set @a=repeat(' ',20);
insert into t1 values (concat('+',@a),concat('+',@a),concat('+',@a));
select concat('*',v,'*',c,'*',t,'*') from t1;

# Check how columns are copied
show create table t1;
create table t2 like t1;
show create table t2;
create table t3 select * from t1;
show create table t3;
alter table t1 modify c varchar(10);
show create table t1;
alter table t1 modify v char(10);
show create table t1;
alter table t1 modify t varchar(10);
show create table t1;
select concat('*',v,'*',c,'*',t,'*') from t1;
drop table t1,t2,t3;

#
# Testing of keys
#
create table t1 (v varchar(10), c char(10), t text, key(v), key(c), key(t(10)));
show create table t1;
disable_query_log;
let $1=10;
while ($1)
{
  let $2=27;
  eval set @space=repeat(' ',10-$1);
  while ($2)
  {
    eval set @char=char(ascii('a')+$2-1);
    insert into t1 values(concat(@char,@space),concat(@char,@space),concat(@char,@space));
    dec $2;
  }
  dec $1;
}
enable_query_log;
select count(*) from t1;
insert into t1 values(concat('a',char(1)),concat('a',char(1)),concat('a',char(1)));
select count(*) from t1 where v='a';
select count(*) from t1 where c='a';
select count(*) from t1 where t='a';
select count(*) from t1 where v='a  ';
select count(*) from t1 where c='a  ';
select count(*) from t1 where t='a  ';
select count(*) from t1 where v between 'a' and 'a ';
select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';
select count(*) from t1 where v like 'a%';
select count(*) from t1 where c like 'a%';
select count(*) from t1 where t like 'a%';
select count(*) from t1 where v like 'a %';
explain select count(*) from t1 where v='a  ';
explain select count(*) from t1 where c='a  ';
explain select count(*) from t1 where t='a  ';
explain select count(*) from t1 where v like 'a%';
explain select count(*) from t1 where v between 'a' and 'a ';
explain select count(*) from t1 where v between 'a' and 'a ' and v between 'a  ' and 'b\n';

--error 1062
alter table t1 add unique(v);
alter table t1 add key(v);
select concat('*',v,'*',c,'*',t,'*') as qq from t1 where v='a';
explain select * from t1 where v='a';

# GROUP BY

select v,count(*) from t1 group by v limit 10;
select v,count(t) from t1 group by v limit 10;
select v,count(c) from t1 group by v limit 10;
select sql_big_result v,count(t) from t1 group by v limit 10;
select sql_big_result v,count(c) from t1 group by v limit 10;
select c,count(*) from t1 group by c limit 10;
select c,count(t) from t1 group by c limit 10;
select sql_big_result c,count(t) from t1 group by c limit 10;
select t,count(*) from t1 group by t limit 10;
select t,count(t) from t1 group by t limit 10;
select sql_big_result t,count(t) from t1 group by t limit 10;
drop table t1;

#
# Test unique keys
#

create table t1 (a char(10), unique (a));
insert into t1 values ('a   ');
--error 1062
insert into t1 values ('a ');

alter table t1 modify a varchar(10);
--error 1062
insert into t1 values ('a '),('a  '),('a   '),('a         ');
--error 1062
insert into t1 values ('a     ');
--error 1062
insert into t1 values ('a          ');
--error 1062
insert into t1 values ('a ');
update t1 set a='a  ' where a like 'a%';
select concat(a,'.') from t1;
update t1 set a='abc    ' where a like 'a ';
select concat(a,'.') from t1;
update t1 set a='a      ' where a like 'a %';
select concat(a,'.') from t1;
update t1 set a='a  ' where a like 'a      ';
select concat(a,'.') from t1;
drop table t1;

#
# test show create table
#

create table t1 (v varchar(10), c char(10), t text, key(v(5)), key(c(5)), key(t(5)));
show create table t1;
drop table t1;
create table t1 (v char(10) character set utf8);
show create table t1;
drop table t1;

create table t1 (v varchar(10), c char(10)) row_format=fixed;
show create table t1;
insert into t1 values('a','a'),('a ','a ');
select concat('*',v,'*',c,'*') from t1;
drop table t1;

#
# Test long varchars
#

create table t1 (v varchar(65530), key(v(10)));
insert into t1 values(repeat('a',65530));
select length(v) from t1 where v=repeat('a',65530);
drop table t1;

#
# Some errors/warnings on create
#

create table t1 (v varchar(65530), key(v));
drop table if exists t1;
create table t1 (v varchar(65536));
show create table t1;
drop table t1;
create table t1 (v varchar(65530) character set utf8);
show create table t1;
drop table t1;
