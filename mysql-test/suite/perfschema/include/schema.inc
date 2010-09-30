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

show create table COND_INSTANCES;
show create table EVENTS_WAITS_CURRENT;
show create table EVENTS_WAITS_HISTORY;
show create table EVENTS_WAITS_HISTORY_LONG;
show create table EVENTS_WAITS_SUMMARY_BY_INSTANCE;
show create table EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME;
show create table EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME;
show create table FILE_INSTANCES;
show create table FILE_SUMMARY_BY_EVENT_NAME;
show create table FILE_SUMMARY_BY_INSTANCE;
show create table MUTEX_INSTANCES;
show create table PERFORMANCE_TIMERS;
show create table RWLOCK_INSTANCES;
show create table SETUP_ACTORS;
show create table SETUP_CONSUMERS;
show create table SETUP_INSTRUMENTS;
show create table SETUP_TIMERS;
show create table THREADS;

