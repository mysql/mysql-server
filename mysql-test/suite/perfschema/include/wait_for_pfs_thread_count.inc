# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Tests for the performance schema
# This helper can be used to enforce that no threads from previous
# tests are still running, which can impact some test scripts.

# Wait until there is only one session left, this one.

let $wait_condition=
  select count(*) = 1 from information_schema.processlist;
--source include/wait_condition.inc

# Threads are removed from information_schema.processlist
# very soon, but continue to execute in the server,
# before finally be removed from performance_schema.threads.
# Because instrumentation is optional, we use "<=" here.

let $wait_condition=
  select count(*) <= 1 from performance_schema.threads
  where `TYPE`='FOREGROUND';
--source include/wait_condition.inc

