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

# Tests for PERFORMANCE_SCHEMA table io
#
# Payload fragment to test table io for basic DML.
#
# $table_item = <schema>.<tablename> must be set before sourcing this script.
#

insert into marker set a = 1;
eval insert into $table_item set a = 'foo', b = 1;
insert into marker set a = 1;
eval insert into $table_item set a = 'foo', b = 2;
insert into marker set a = 1;
eval insert into $table_item set a = 'foo', b = 3;
insert into marker set a = 1;
eval select * from $table_item;
insert into marker set a = 1;
eval update $table_item set a = 'bar';
insert into marker set a = 1;
eval select * from $table_item limit 2;
insert into marker set a = 1;
eval delete from $table_item where b = 3;
insert into marker set a = 1;
# This may record a fetch for "deleted" records
eval select * from $table_item;
insert into marker set a = 1;
eval optimize table $table_item;
insert into marker set a = 1;
# Same data after optimize
eval select * from $table_item;
insert into marker set a = 1;
# truncate will fail for views
--disable_abort_on_error
eval truncate table $table_item;
--enable_abort_on_error
insert into marker set a = 1;

