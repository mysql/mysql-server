use test;
drop table if exists t_basic;
create table if not exists t_basic (
  id int not null,
  name varchar(32) default 'Employee 666',
  age int,
  magic int not null,
  primary key(id),

  unique key idx_unique_hash_magic (magic) using hash,
  key idx_btree_age (age)
);
insert into t_basic values(0, 'Employee 0', 0, 0);
insert into t_basic values(1, 'Employee 1', 1, 1);
insert into t_basic values(2, 'Employee 2', 2, 2);
insert into t_basic values(3, 'Employee 3', 3, 3);
insert into t_basic values(4, 'Employee 4', 4, 4);
insert into t_basic values(5, 'Employee 5', 5, 5);
insert into t_basic values(6, 'Employee 6', 6, 6);
insert into t_basic values(7, 'Employee 7', 7, 7);
insert into t_basic values(8, 'Employee 8', 8, 8);
insert into t_basic values(9, 'Employee 9', 9, 9);
