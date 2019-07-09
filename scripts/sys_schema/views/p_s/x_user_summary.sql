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
-- View: x$user_summary
--
-- Summarizes statement activity and connections by user
-- 
-- When the user found is NULL, it is assumed to be a "background" thread.  
--
-- mysql> select * from x$user_summary;
-- +------+------------+-----------------+------------------+---------------------+-------------------+--------------+----------------+------------------------+
-- | user | statements | total_latency   | avg_latency      | current_connections | total_connections | unique_hosts | current_memory | total_memory_allocated |
-- +------+------------+-----------------+------------------+---------------------+-------------------+--------------+----------------+------------------------+
-- | root |       5685 | 107175100271000 | 18852260381.8821 |                   1 |                 1 |            1 |        1459022 |              572855680 |
-- | mark |        225 |  14489223428000 | 64396548568.8889 |                   1 |                 1 |            1 |         724578 |               84958286 |
-- +------+------------+-----------------+------------------+---------------------+-------------------+--------------+----------------+------------------------+
--

CREATE OR REPLACE
  ALGORITHM = TEMPTABLE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW x$user_summary (
  user,
  statements,
  statement_latency,
  statement_avg_latency,
  table_scans,
  file_ios,
  file_io_latency,
  current_connections,
  total_connections,
  unique_hosts,
  current_memory,
  total_memory_allocated
) AS
SELECT IF(accounts.user IS NULL, 'background', accounts.user) AS user,
       SUM(stmt.total) AS statements,
       SUM(stmt.total_latency) AS statement_latency,
       IFNULL(SUM(stmt.total_latency) / NULLIF(SUM(stmt.total), 0), 0) AS statement_avg_latency,
       SUM(stmt.full_scans) AS table_scans,
       SUM(io.ios) AS file_ios,
       SUM(io.io_latency) AS file_io_latency,
       SUM(accounts.current_connections) AS current_connections,
       SUM(accounts.total_connections) AS total_connections,
       COUNT(DISTINCT host) AS unique_hosts,
       SUM(mem.current_allocated) AS current_memory,
       SUM(mem.total_allocated) AS total_memory_allocated
  FROM performance_schema.accounts
  LEFT JOIN sys.x$user_summary_by_statement_latency AS stmt ON IF(accounts.user IS NULL, 'background', accounts.user) = stmt.user
  LEFT JOIN sys.x$user_summary_by_file_io AS io ON IF(accounts.user IS NULL, 'background', accounts.user) = io.user
  LEFT JOIN sys.x$memory_by_user_by_current_bytes mem ON IF(accounts.user IS NULL, 'background', accounts.user) = mem.user
 GROUP BY IF(accounts.user IS NULL, 'background', accounts.user)
 ORDER BY SUM(stmt.total_latency) DESC;
