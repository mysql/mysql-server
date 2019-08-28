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
-- View: waits_global_by_latency
--
-- Lists the top wait events by their total latency, ignoring idle (this may be very large).
--
-- mysql> select * from waits_global_by_latency limit 5;
-- +--------------------------------------+------------+---------------+-------------+-------------+
-- | event                                | total      | total_latency | avg_latency | max_latency |
-- +--------------------------------------+------------+---------------+-------------+-------------+
-- | wait/io/file/myisam/dfile            | 3623719744 | 00:47:49.09   | 791.70 ns   | 312.96 ms   |
-- | wait/io/table/sql/handler            |   69114944 | 00:44:30.74   | 38.64 us    | 879.49 ms   |
-- | wait/io/file/innodb/innodb_log_file  |   28100261 | 00:37:42.12   | 80.50 us    | 476.00 ms   |
-- | wait/io/socket/sql/client_connection |  200704863 | 00:18:37.81   | 5.57 us     | 1.27 s      |
-- | wait/io/file/innodb/innodb_data_file |    2829403 | 00:08:12.89   | 174.20 us   | 455.22 ms   |
-- +--------------------------------------+------------+---------------+-------------+-------------+
--

CREATE OR REPLACE
  ALGORITHM = MERGE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW waits_global_by_latency (
  events,
  total,
  total_latency,
  avg_latency,
  max_latency
) AS
SELECT event_name AS event,
       count_star AS total,
       sys.format_time(sum_timer_wait) AS total_latency,
       sys.format_time(avg_timer_wait) AS avg_latency,
       sys.format_time(max_timer_wait) AS max_latency
  FROM performance_schema.events_waits_summary_global_by_event_name
 WHERE event_name != 'idle'
   AND sum_timer_wait > 0
 ORDER BY sum_timer_wait DESC;
