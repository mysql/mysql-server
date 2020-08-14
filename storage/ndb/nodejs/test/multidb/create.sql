-- Copyright (c) 2013, 2020, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
--
-- This program is also distributed with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have included with MySQL.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

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

