-- Copyright (C) 2007, 2008 MySQL AB
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

create database atrt;
use atrt;

create table host (
   id int primary key,
   name varchar(255),
   port int unsigned,
   unique(name, port)
   ) engine = myisam;

create table cluster ( 
   id int primary key,
   name varchar(255),
   unique(name)
   ) engine = myisam;

create table process (
  id int primary key,
  host_id int not null,
  cluster_id int not null,
  node_id int not null,
  type enum ('ndbd', 'ndbapi', 'ndb_mgmd', 'mysqld', 'mysql') not null,
  state enum ('starting', 'started', 'stopping', 'stopped') not null
  ) engine = myisam;

create table options (
  id int primary key,
  process_id int not null,
  name varchar(255) not null,
  value varchar(255) not null
  ) engine = myisam;

create table repl (
  id int auto_increment primary key,
  master_id int not null,
  slave_id int not null
  ) engine = myisam;

create table command (
  id int auto_increment primary key,
  state enum ('new', 'running', 'done') not null default 'new',
  cmd int not null,
  process_id int not null,
  process_args varchar(255) default NULL
  ) engine = myisam;
