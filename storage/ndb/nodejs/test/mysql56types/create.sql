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
create table if not exists mysql56strings (
  id int not null PRIMARY KEY,
  str_fix_utf16le CHAR(20) character set utf16le,
  str_var_utf16le VARCHAR(20) character set utf16le,
  str_fix_utf8mb4 CHAR(20) character set utf8mb4,
  str_var_utf8mb4 VARCHAR(20) character set utf8mb4,
  str_fix_utf8mb3 CHAR(20) character set utf8mb3,
  str_var_utf8mb3 VARCHAR(20) character set utf8mb3
);

create table if not exists mysql56times ( 
  id int not null primary key,
  a time(1) null,
  b datetime(2) null,
  c timestamp(3) null,
  d time(4) null,
  e datetime(5) null,
  f timestamp(6) null
);

create table if not exists mysql56stringsWithText (
  id int not null PRIMARY KEY,
  str_var_utf16le VARCHAR(20) character set utf16le,
  str_var_utf8mb4 VARCHAR(20) character set utf8mb4,
  utf16le_text TEXT character set utf16le
);

delete from mysql56strings;
delete from mysql56times;
delete from mysql56stringsWithText;
