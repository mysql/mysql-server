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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

# Tests for the performance schema
#

# See comments in include/table_aggregate_setup.inc

--disable_query_log

revoke all privileges, grant option from user1@localhost;
revoke all privileges, grant option from user2@localhost;
revoke all privileges, grant option from user3@localhost;
revoke all privileges, grant option from user4@localhost;
drop user user1@localhost;
drop user user2@localhost;
drop user user3@localhost;
drop user user4@localhost;
flush privileges;

drop prepare dump_thread;
drop prepare dump_global;
drop prepare dump_history;
drop prepare dump_index_io;
drop prepare dump_table_io;
drop prepare dump_table_lock;
drop prepare dump_objects_summary;

truncate table performance_schema.setup_actors;
insert into performance_schema.setup_actors
  select * from test.setup_actors;
drop table test.setup_actors;

truncate table performance_schema.setup_objects;
insert into performance_schema.setup_objects
  select * from test.setup_objects;
drop table test.setup_objects;

drop table test.t1;
drop table test.t2;
drop table test.t3;

update performance_schema.threads set instrumented='YES';
update performance_schema.setup_instruments set enabled='YES', timed='YES';
update performance_schema.setup_consumers set enabled='YES';

--enable_query_log

