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
drop table if exists autopk;
create table if not exists autopk (
  id int not null AUTO_INCREMENT,
  name varchar(32) default 'Employee 666',
  age int,
  magic int not null,
  primary key(id),

  unique key idx_unique_hash_magic (magic) using hash,
  key idx_btree_age (age)
);

insert into autopk(name, age, magic) values('Employee 1', 1, 1);
insert into autopk(name, age, magic) values('Employee 2', 2, 2);
insert into autopk(name, age, magic) values('Employee 3', 3, 3);
insert into autopk(name, age, magic) values('Employee 4', 4, 4);
insert into autopk(name, age, magic) values('Employee 5', 5, 5);
insert into autopk(name, age, magic) values('Employee 6', 6, 6);
insert into autopk(name, age, magic) values('Employee 7', 7, 7);
insert into autopk(name, age, magic) values('Employee 8', 8, 8);
insert into autopk(name, age, magic) values('Employee 9', 9, 9);

drop table if exists autouk;
create table if not exists autouk (
  id int not null,
  name varchar(32) default 'Employee 666',
  age int,
  magic int not null AUTO_INCREMENT,
  primary key(id),

  unique key idx_unique_hash_magic (magic) using hash,
  key idx_btree_age (age)
);

insert into autouk(id, name, age) values(1, 'Employee 1', 1);
insert into autouk(id, name, age) values(2, 'Employee 2', 2);
insert into autouk(id, name, age) values(3, 'Employee 3', 3);
insert into autouk(id, name, age) values(4, 'Employee 4', 4);
insert into autouk(id, name, age) values(5, 'Employee 5', 5);
insert into autouk(id, name, age) values(6, 'Employee 6', 6);
insert into autouk(id, name, age) values(7, 'Employee 7', 7);
insert into autouk(id, name, age) values(8, 'Employee 8', 8);
insert into autouk(id, name, age) values(9, 'Employee 9', 9);
