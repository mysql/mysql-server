use test;
create table IF NOT EXISTS tbl1 (i int primary key not null, j int) engine = ndb;
create table IF NOT EXISTS tbl2 (i int primary key not null, j int) engine = ndb;