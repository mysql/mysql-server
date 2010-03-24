# Copyright (C) 2008-2009 Sun Microsystems, Inc
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

# Tests for PERFORMANCE_SCHEMA

show databases;

select count(*) from performance_schema.PERFORMANCE_TIMERS;
select count(*) from performance_schema.SETUP_CONSUMERS;
select count(*) > 0 from performance_schema.SETUP_INSTRUMENTS;
select count(*) from performance_schema.SETUP_TIMERS;

# Make sure we don't crash, no matter what the starting parameters are

--disable_result_log
select * from performance_schema.COND_INSTANCES;
select * from performance_schema.EVENTS_WAITS_CURRENT;
select * from performance_schema.EVENTS_WAITS_HISTORY;
select * from performance_schema.EVENTS_WAITS_HISTORY_LONG;
select * from performance_schema.EVENTS_WAITS_SUMMARY_BY_EVENT_NAME;
select * from performance_schema.EVENTS_WAITS_SUMMARY_BY_INSTANCE;
select * from performance_schema.EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME;
select * from performance_schema.FILE_INSTANCES;
select * from performance_schema.FILE_SUMMARY_BY_EVENT_NAME;
select * from performance_schema.FILE_SUMMARY_BY_INSTANCE;
select * from performance_schema.MUTEX_INSTANCES;
select * from performance_schema.PERFORMANCE_TIMERS;
select * from performance_schema.PROCESSLIST;
select * from performance_schema.RWLOCK_INSTANCES;
select * from performance_schema.SETUP_CONSUMERS;
select * from performance_schema.SETUP_INSTRUMENTS;
select * from performance_schema.SETUP_OBJECTS;
select * from performance_schema.SETUP_TIMERS;
--enable_result_log

# This has a stable output, printing the result:
show variables like "performance_schema%";

# This has an unrepeatable output, it does depends too much on
# - the platform hardware (sizeof structures, padding)
# - the compiler used (sizeof(enum))
# - the platform header files (sizeof(size_t))
# - the code path in the server (what gets executed and instrumented
#   at runtime)

--disable_result_log
show engine PERFORMANCE_SCHEMA status;
show status like "performance_schema%";
--enable_result_log

