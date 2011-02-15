# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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
#    Show existing objects and information about their structure

show databases like 'performance_schema';
show create database performance_schema;

use performance_schema;

show tables;

show create table cond_instances;
show create table events_waits_current;
show create table events_waits_history;
show create table events_waits_history_long;
show create table events_waits_summary_by_instance;
show create table events_waits_summary_by_thread_by_event_name;
show create table events_waits_summary_global_by_event_name;
show create table file_instances;
show create table file_summary_by_event_name;
show create table file_summary_by_instance;
show create table mutex_instances;
show create table objects_summary_global_by_type;
show create table performance_timers;
show create table rwlock_instances;
show create table setup_actors;
show create table setup_consumers;
show create table setup_instruments;
show create table setup_objects;
show create table setup_timers;
show create table table_io_waits_summary_by_index_usage;
show create table table_io_waits_summary_by_table;
show create table table_lock_waits_summary_by_table;
show create table threads;

