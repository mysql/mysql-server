use test;
drop table if exists freeform;
create table if not exists freeform (
  id int not null AUTO_INCREMENT,
  SPARSE_FIELDS varchar(1000),
  primary key(id)
);

insert into freeform(id, SPARSE_FIELDS) values(1, '{"name": "Name 1", "number": 1, "a": [{"a10": "a10"}, {"a11": "a11"}]}');
insert into freeform(id, SPARSE_FIELDS) values(2, '{"name": "Name 2", "number": 2, "a": [{"a20": "a20"}, {"a21": "a21"}]}');

drop table if exists semistruct;
create table if not exists semistruct (
  id int not null AUTO_INCREMENT,
  name varchar(30),
  number int,
  SPARSE_FIELDS varchar(1000),
  primary key(id)
);

insert into semistruct(id, name, number, SPARSE_FIELDS) values(1, "Name 1", 1, '{"a": [{"a10": "a10"}, {"a11": "a11"}]}');
insert into semistruct(id, name, number, SPARSE_FIELDS) values(2, "Name 2", 2, '{"a": [{"a20": "a20"}, {"a21": "a21"}]}');
