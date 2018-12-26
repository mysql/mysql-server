#
# Test legacy compatibility of no fk foreign key support
#  - snippets of other tests converted to use different engines
#    for quick and easy compatibility test
#

if (!$engine)
{
  die Missing parameter $engine;
}

#
# Test 1
#

SET foreign_key_checks=0;
--replace_result $engine engine
eval CREATE TABLE t1 (id int primary key, foreign key (id) references t2(id)) ENGINE=$engine;
--replace_result $engine engine
eval CREATE TABLE t2 (id int primary key, foreign key (id) references t1(id)) ENGINE=$engine;
SET foreign_key_checks=1;
SELECT COUNT(*) FROM information_schema.key_column_usage
  WHERE REFERENCED_TABLE_NAME in ('t1', 't2');
SET foreign_key_checks=0;
#
# Bug#16817928
# Check dropping in both order
DROP TABLE t1, t2;

SET foreign_key_checks=0;
--replace_result $engine engine
eval CREATE TABLE t1 (id int primary key, foreign key (id) references t2(id)) ENGINE=$engine;
--replace_result $engine engine
eval CREATE TABLE t2 (id int primary key, foreign key (id) references t1(id)) ENGINE=$engine;
SET foreign_key_checks=1;
SELECT COUNT(*) FROM information_schema.key_column_usage
  WHERE REFERENCED_TABLE_NAME in ('t1', 't2');
SET foreign_key_checks=0;
#
# Bug#16817928
# Check dropping in both order
DROP TABLE t2, t1;

SET foreign_key_checks=1;

#
# Test foreign key referencing table in db which does not exist
#
SET foreign_key_checks=0;
--replace_result $engine engine
eval CREATE TABLE t1 (id int primary key, foreign key (id) references no_such_db.t2(id)) ENGINE=$engine;
drop table t1;
SET foreign_key_checks=1;

#
# Test adding multiple fks with same name
#
--replace_result $engine engine
eval create table t1(pk1 int primary key auto_increment,
  a int,
  b int
) engine=$engine;
--replace_result $engine engine
eval create table t2(pk2 int primary key auto_increment,
  c int not null,
  unique(c),
  d int not null,
  unique(d)
) engine=$engine;
--error ER_DUP_KEYNAME
alter table t1
  add constraint fk3 foreign key (a) references t2(c),
  add constraint fk3 foreign key (b) references t2(d);
# Try same alter without foreign_key_checks
SET foreign_key_checks=0;
--error ER_DUP_KEYNAME
alter table t1
  add constraint fk3 foreign key (a) references t2(c),
  add constraint fk3 foreign key (b) references t2(d);
SET foreign_key_checks=1;
drop table t1, t2;


#
# Test TRUNCATE of table referenced by fk
#
--replace_result $engine engine
eval create table t2 (
  pk2 int not null primary key,
  c int
) engine=$engine;
--replace_result $engine engine
eval create table t1 (
  pk1 int not null primary key,
  b int,
  foreign key (b) references t2(pk2)
) engine=$engine;

--replace_regex /\(.*\)/(<fk_def>)/
--error ER_TRUNCATE_ILLEGAL_FK
truncate table t2;

set foreign_key_checks=1;
--replace_regex /\(.*\)/(<fk_def>)/
--error ER_TRUNCATE_ILLEGAL_FK
truncate table t2;
set foreign_key_checks=0;

drop table t1, t2;

#
# Test TRUNCATE of referencing table in fk
#
--replace_result $engine engine
eval create table t2 (
  pk2 int not null primary key,
  c int
) engine=$engine;
--replace_result $engine engine
eval create table t1 (
  pk1 int not null primary key,
  b int,
  foreign key (b) references t2(pk2)
) engine=$engine;

truncate table t1;

set foreign_key_checks=1;
truncate table t1;
set foreign_key_checks=0;

drop table t1, t2;

#
# Test fk referencing columns in reverse order to existing index
#  - should return an error saying "missing index for constraint .."
#
# NOTE! column order does not matter if parent index is unique
#
--replace_result $engine engine
eval create table t1 (
  a1 int primary key,
  b1 int not null,
  c1 int not null,
  key (b1,c1)
) engine $engine;

--replace_result $engine engine
eval create table t2 (
  a2 int primary key auto_increment,
  b2 int not null,
  c2 int not null
) engine $engine;

--error ER_FK_NO_INDEX_PARENT
alter table t2
  add constraint foreign key fk1 (b2,c2) references t1 (c1,b1);

insert into t1 (a1,b1,c1) values
  (1,11,12),(2,21,22),(3,31,32);

insert into t2 (b2,c2)
  select c1, b1 from t1;

drop table t2,t1;

#
# Test n
#


