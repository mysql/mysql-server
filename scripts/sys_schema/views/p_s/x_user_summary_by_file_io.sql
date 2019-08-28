-- Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is also distributed with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have included with MySQL.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License, version 2.0, for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

--
-- View: x$user_summary_by_file_io
--
-- Summarizes file IO totals per user.
--
-- When the user found is NULL, it is assumed to be a "background" thread.
--
-- mysql> select * from x$user_summary_by_file_io;
-- +------------+-------+----------------+
-- | user       | ios   | io_latency     |
-- +------------+-------+----------------+
-- | root       | 26457 | 21579585586390 |
-- | background |  1189 |   394212617370 |
-- +------------+-------+----------------+
--

CREATE OR REPLACE
  ALGORITHM = TEMPTABLE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW x$user_summary_by_file_io (
  user,
  ios,
  io_latency
) AS
SELECT IF(user IS NULL, 'background', user) AS user,
       SUM(count_star) AS ios,
       SUM(sum_timer_wait) AS io_latency 
  FROM performance_schema.events_waits_summary_by_user_by_event_name
 WHERE event_name LIKE 'wait/io/file/%'
 GROUP BY IF(user IS NULL, 'background', user)
 ORDER BY SUM(sum_timer_wait) DESC;
