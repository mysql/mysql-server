# Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
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

RESET MASTER;

select count(*) > 0 from performance_schema.setup_instruments;

# Note:
# Do not include records that could depend on
# compiling options (storage engines, SSL),
# to ensure the expected output in the binlog is predictable.
update performance_schema.setup_instruments set enabled='NO'
  where name like "wait/synch/rwlock/sql/%"
  and name not in ("wait/synch/rwlock/sql/CRYPTO_dynlock_value::lock");

select count(*) > 0 from performance_schema.events_waits_current;

--disable_warnings
drop table if exists test.t1;
drop table if exists test.t2;
--enable_warnings

create table test.t1 (thread_id integer);
create table test.t2 (name varchar(128));

insert into test.t1
  select thread_id from performance_schema.events_waits_current;

insert into test.t2
  select name from performance_schema.setup_instruments
    where name like "wait/synch/rwlock/sql/%"
    and name not in ("wait/synch/rwlock/sql/CRYPTO_dynlock_value::lock");

drop table test.t1;
drop table test.t2;

update performance_schema.setup_instruments set enabled='YES'
  where name like "wait/synch/rwlock/sql/%"
  and name not in ("wait/synch/rwlock/sql/CRYPTO_dynlock_value::lock");

--source include/show_binlog_events.inc

