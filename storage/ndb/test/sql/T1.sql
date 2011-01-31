-- Copyright (C) 2005 MySQL AB
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

create database if not exists TEST_DB;
use TEST_DB;
drop table if exists T1;
create table T1 (KOL1 int unsigned not null,
                 KOL2 int unsigned not null,
	         KOL3 int unsigned not null,
	         KOL4 int unsigned not null,
	         KOL5 int unsigned not null,
	         primary key using hash(KOL1)) engine=ndb;
