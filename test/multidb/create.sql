
-- if database "test1" (etc.) already exists, then this should fail,
-- because drop.sql in ClearSmokeTest is going to drop the database, and we 
-- wouldn't want to drop a database that existed prior to testing.

create database test1;
use test1;
drop table if exists tbl1;
create table tbl1 (i int primary key not null, j int) engine = ndb;

create database test2;
use test2;
drop table if exists tbl2;
create table tbl2 (i int primary key not null, j int) engine = ndb;

create database test3;
use test3;
drop table if exists tbl3;
create table tbl3 (i int primary key not null, j int) engine = ndb;

create database test4;
use test4;
drop table if exists tbl4;
create table tbl4 (i int primary key not null, j int) engine = ndb;

create database test5;
use test5;
drop table if exists tbl5;
create table tbl5 (i int primary key not null, j int) engine = ndb;

create database test6;
use test6;
drop table if exists tbl6;
create table tbl6 (i int primary key not null, j int) engine = ndb;

create database test7;
use test7;
drop table if exists tbl7;
create table tbl7 (i int primary key not null, j int) engine = ndb;

create database test8;
use test8;
drop table if exists tbl8;
create table tbl8 (i int primary key not null, j int) engine = ndb;

