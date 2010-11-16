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

# Performance schema test template

# How to use this script in a test case
# =====================================
#
# The general table io test template is as follows
#
# --source include/not_embedded.inc
# --source include/have_perfschema.inc
# --source ../include/table_io_setup_helper.inc
# ... more setup scripts as needed ...
# update performance_schema.setup_consumers set enabled='YES';
# ... test payload here ...
# ... optionally, add this insert between statements
# ... to make the final output more readable
# insert into test.marker set a=1;
# ... more test payload here ...
# ... optionaly, add the following line (by default, only "test" is dumped) ...
# let $schema_to_dump="db1", "db2", "db3";
# --source ../include/table_io_result_helper.inc
# Optional: Repeat several times
#    update performance_schema.setup_consumers set enabled='YES';
#    ... test payload here ...
#    --source ../include/table_io_result_helper.inc
# ... cleanup
# --source ../include/table_io_cleanup_helper.inc
#
# (end of template)

# Setup

--disable_warnings
drop table if exists test.marker;
--enable_warnings

# To be used in the test payload,
# insert into marker makes the test output easier to read,
# to separate table io events between statements.
create table test.marker(a int);

update performance_schema.setup_consumers set enabled='NO';

update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES'
  where name like "wait/io/table/%";

truncate table performance_schema.events_waits_history_long;

# Reset lost counters to a known state
flush status;

# By default, dump table io only for test.
# A test can overide this
let $schema_to_dump="test";

