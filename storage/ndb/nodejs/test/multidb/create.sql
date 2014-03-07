
create database IF NOT EXISTS mysqljs_multidb_test1;
use mysqljs_multidb_test1;
drop table if exists tbl1;
create table tbl1 (i int not null, j int, primary key using hash(i)) engine = ndb;

create database IF NOT EXISTS mysqljs_multidb_test2;
use mysqljs_multidb_test2;
drop table if exists tbl2;
create table tbl2 (i int not null, j int, primary key using hash(i)) engine = ndb;

create database IF NOT EXISTS mysqljs_multidb_test3;
use mysqljs_multidb_test3;
drop table if exists tbl3;
create table tbl3 (i int not null, j int, primary key using hash(i)) engine = ndb;

create database IF NOT EXISTS  mysqljs_multidb_test4;
use mysqljs_multidb_test4;
drop table if exists tbl4;
create table tbl4 (i int not null, j int, primary key using hash(i)) engine = ndb;

create database IF NOT EXISTS mysqljs_multidb_test5;
use mysqljs_multidb_test5;
drop table if exists tbl5;
create table tbl5 (i int not null, j int, primary key using hash(i)) engine = ndb;

create database IF NOT EXISTS mysqljs_multidb_test6;
use mysqljs_multidb_test6;
drop table if exists tbl6;
create table tbl6 (i int not null, j int, primary key using hash(i)) engine = ndb;

create database IF NOT EXISTS mysqljs_multidb_test7;
use mysqljs_multidb_test7;
drop table if exists tbl7;
create table tbl7 (i int not null, j int, primary key using hash(i)) engine = ndb;

create database IF NOT EXISTS mysqljs_multidb_test8;
use mysqljs_multidb_test8;
drop table if exists tbl8;
create table tbl8 (i int not null, j int, primary key using hash(i)) engine = ndb;

