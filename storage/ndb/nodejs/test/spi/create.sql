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

use test;
drop table if exists tbl1;
create table IF NOT EXISTS tbl1 (i int primary key not null, j int);

drop table if exists tbl2;
create table IF NOT EXISTS tbl2 (
  colbit bit(9),
  coltinyint tinyint,
  colsmallint smallint default 55,
  colmediumint mediumint,
  colint int primary key not null,
  colinteger integer,
  colbigint bigint,
  colreal real,
  coldouble double,
  colfloat float,
  coldecimal decimal(12,2),
  colnumeric numeric,

  coltinyintunsigned tinyint unsigned,
  colsmallintunsigned smallint unsigned,
  colmediumintunsigned mediumint unsigned,
  colintunsigned int unsigned,
  colintegerunsigned integer unsigned,
  colbigintunsigned bigint unsigned,
  colrealunsigned real unsigned,
  coldoubleunsigned double unsigned,
  colfloatunsigned float unsigned,
  coldecimalunsigned decimal unsigned,
  colnumericunsigned numeric unsigned,

  coldate date,
  coltime time,
  coltimestamp timestamp,
  colyear year,

  colchar char(12) collate latin1_spanish_ci,
  colvarchar varchar(16) collate big5_chinese_ci,
  colbinary binary(22),
  colvarbinary varbinary(28),
  coltinyblob tinyblob,
  colblob blob,
  colmediumblob mediumblob,
  collongblob longblob,

  coltinytext tinytext,
  coltext text character set sjis collate sjis_bin,
  colmediumtext mediumtext collate hebrew_general_ci,
  collongtext longtext character set latin5,
  unique key idxcoltinyintusinghash (coltinyint) using hash,
  unique key idxcolsmallintboth (colsmallint),
  key idxcolintunsignedcoldateusingbtree (colintunsigned, coldate)
  );

drop table if exists tbl3;
create table IF NOT EXISTS tbl3 (
  i int primary key not null,
  c varchar(120) 
  );

drop table if exists tbl4;
create table IF NOT EXISTS tbl4 (
  i int primary key not null default 1000,
  k int,
  c varchar(120),
  unique key (k)
  );
