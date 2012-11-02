# create table for ndb_946 tests

eval
create table t1 (
  a int not null,
  b year not null default '2000',
  c date not null default '2000-01-02',
  d time$prec not null default '10:11:12$frac',
  e datetime$prec not null default '2000-01-02 10:11:12$frac',
  f timestamp$prec not null default '2000-01-02 10:11:12$frac',
  primary key using hash (a),
  index bx (b),
  index cx (c),
  index dx (d),
  index ex (e),
  index fx (f)
) engine=ndb;


