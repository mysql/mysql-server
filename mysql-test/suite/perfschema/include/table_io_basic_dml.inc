# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

# Tests for PERFORMANCE_SCHEMA

# Payload fragment to test table io for basic DML.

# Setup

--disable_warnings
drop table if exists test.no_index_tab;
drop temporary table if exists test.temp_tab;
--enable_warnings

# Start recording events
update performance_schema.SETUP_CONSUMERS set enabled='YES';

# Code to test

insert into marker set a=1;
eval create table test.no_index_tab
  ( a varchar(255) not null, b int not null) engine=$engine_type;
# Make sure the proper engine is used
show create table test.no_index_tab;
insert into marker set a=1;
insert into no_index_tab set a = 'foo', b = 1;
insert into marker set a=1;
insert into no_index_tab set a = 'foo', b = 2;
insert into marker set a=1;
insert into no_index_tab set a = 'foo', b = 3;
insert into marker set a=1;
select * from test.no_index_tab;
insert into marker set a=1;
update test.no_index_tab set a = 'bar';
insert into marker set a=1;
select * from test.no_index_tab limit 2;
insert into marker set a=1;
delete from test.no_index_tab where b = 3;
insert into marker set a=1;
# This may record a fetch for "deleted" records
select * from test.no_index_tab;
insert into marker set a=1;
optimize table test.no_index_tab;
insert into marker set a=1;
# Same data after optimize
select * from test.no_index_tab;
insert into marker set a=1;
truncate table test.no_index_tab;

insert into marker set a=1;
eval create temporary table test.temp_tab
  ( a varchar(255) not null, b int not null) engine=$engine_type;
show create table test.temp_tab;
insert into marker set a=1;
insert into temp_tab set a = 'foo', b = 1;
insert into marker set a=1;
insert into temp_tab set a = 'foo', b = 2;
insert into marker set a=1;
insert into temp_tab set a = 'foo', b = 3;
insert into marker set a=1;
select * from test.temp_tab;
insert into marker set a=1;
update test.temp_tab set a = 'bar';
insert into marker set a=1;
select * from test.temp_tab limit 2;
insert into marker set a=1;
delete from test.temp_tab where b = 3;
insert into marker set a=1;
select * from test.temp_tab;
insert into marker set a=1;
truncate table test.temp_tab;

# Stop recording events
update performance_schema.SETUP_CONSUMERS set enabled='NO';

# Cleanup

drop table test.no_index_tab;
drop temporary table test.temp_tab;

