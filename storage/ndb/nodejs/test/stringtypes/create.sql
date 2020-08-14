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
create table if not exists charset_test (
  id int not null PRIMARY KEY,
  str_fix_latin1 CHAR(20) character set latin1,
  str_var_latin1 VARCHAR(20) character set latin1,
  str_fix_latin2 CHAR(20) character set latin2,
  str_var_latin2 VARCHAR(20) character set latin2,
  str_fix_utf8  CHAR(20) character set utf8,
  str_var_utf8  VARCHAR(20) character set utf8,
  str_fix_utf16 CHAR(20) character set utf16,
  str_var_utf16 VARCHAR(20) character set utf16,
  str_fix_ascii CHAR(20) character set ascii,
  str_var_ascii VARCHAR(20) character set ascii,
  str_fix_utf32 CHAR(20) character set utf32,
  str_var_utf32 VARCHAR(20) character set utf32  
);

create table if not exists binary_test (
  id int not null PRIMARY KEY,
  bin_fix BINARY(20),
  bin_var VARBINARY(200),
  bin_var_long VARBINARY(2000),
  bin_lob BLOB
);

create table if not exists text_blob_test ( 
  id int not null primary key,
  blob_col BLOB,
  text_col TEXT character set utf8
);

create table if not exists text_charset_test (
  id int not null primary key,
  ascii_text TEXT character set ascii,
  latin1_text TEXT character set latin1,
  utf16_text TEXT character set utf16
);

delete from charset_test;
delete from binary_test;
delete from text_blob_test;
delete from text_charset_test;
