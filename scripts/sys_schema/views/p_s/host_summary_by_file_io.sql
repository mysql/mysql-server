<<<<<<< HEAD
-- Copyright (c) 2014, 2022, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
=======
-- Copyright (c) 2014, 2023, Oracle and/or its affiliates.
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
>>>>>>> pr/231
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
<<<<<<< HEAD
-- GNU General Public License for more details.
=======
-- GNU General Public License, version 2.0, for more details.
>>>>>>> pr/231
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

--
-- View: host_summary_by_file_io
--
-- Summarizes file IO totals per host.
--
-- When the host found is NULL, it is assumed to be a "background" thread.
--
-- mysql> select * from host_summary_by_file_io;
-- +------------+-------+------------+
-- | host       | ios   | io_latency |
-- +------------+-------+------------+
-- | hal1       | 26457 | 21.58 s    |
-- | hal2       |  1189 | 394.21 ms  |
-- +------------+-------+------------+
--

CREATE OR REPLACE
  ALGORITHM = TEMPTABLE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW host_summary_by_file_io (
  host,
  ios,
  io_latency
) AS
SELECT IF(host IS NULL, 'background', host) AS host,
       SUM(count_star) AS ios,
<<<<<<< HEAD
       format_pico_time(SUM(sum_timer_wait)) AS io_latency 
=======
       sys.format_time(SUM(sum_timer_wait)) AS io_latency 
>>>>>>> pr/231
  FROM performance_schema.events_waits_summary_by_host_by_event_name
 WHERE event_name LIKE 'wait/io/file/%'
 GROUP BY IF(host IS NULL, 'background', host)
 ORDER BY SUM(sum_timer_wait) DESC;
