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

select count(*) from performance_schema.performance_timers;
select count(*) from performance_schema.setup_consumers;
select count(*) > 0 from performance_schema.setup_instruments;
select count(*) from performance_schema.setup_timers;

# Make sure we don't crash, no matter what the starting parameters are

--disable_result_log
select * from performance_schema.cond_instances;
select * from performance_schema.events_waits_current;
select * from performance_schema.events_waits_history;
select * from performance_schema.events_waits_history_long;
select * from performance_schema.events_waits_summary_by_instance;
select * from performance_schema.events_waits_summary_by_thread_by_event_name;
select * from performance_schema.events_waits_summary_global_by_event_name;
select * from performance_schema.file_instances;
select * from performance_schema.file_summary_by_event_name;
select * from performance_schema.file_summary_by_instance;
select * from performance_schema.mutex_instances;
select * from performance_schema.performance_timers;
select * from performance_schema.rwlock_instances;
select * from performance_schema.setup_consumers;
select * from performance_schema.setup_instruments;
select * from performance_schema.setup_timers;
select * from performance_schema.threads;
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

