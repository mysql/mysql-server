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

drop table if exists mysqljs_multidb_test1.tbl1;
drop table if exists mysqljs_multidb_test2.tbl2;
drop table if exists mysqljs_multidb_test3.tbl3;
drop table if exists mysqljs_multidb_test4.tbl4;
drop table if exists mysqljs_multidb_test5.tbl5;
drop table if exists mysqljs_multidb_test6.tbl6;
drop table if exists mysqljs_multidb_test7.tbl7;
drop table if exists mysqljs_multidb_test8.tbl8;

drop database if exists test1;
drop database if exists mysqljs_multidb_test2;
drop database if exists mysqljs_multidb_test3;
drop database if exists mysqljs_multidb_test4;
drop database if exists mysqljs_multidb_test5;
drop database if exists mysqljs_multidb_test6;
drop database if exists mysqljs_multidb_test7;
drop database if exists mysqljs_multidb_test8;
