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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

--
-- View: memory_by_user_by_current_bytes
--
-- Summarizes memory use by user using the 5.7 Performance Schema instrumentation.
-- 
-- When the user found is NULL, it is assumed to be a "background" thread.  
--
-- mysql> select * from memory_by_user_by_current_bytes;
-- +------+--------------------+-------------------+-------------------+-------------------+-----------------+
-- | user | current_count_used | current_allocated | current_avg_alloc | current_max_alloc | total_allocated |
-- +------+--------------------+-------------------+-------------------+-------------------+-----------------+
-- | root |               1401 | 1.09 MiB          | 815 bytes         | 334.97 KiB        | 42.73 MiB       |
-- | mark |                201 | 496.08 KiB        | 2.47 KiB          | 334.97 KiB        | 5.50 MiB        |
-- +------+--------------------+-------------------+-------------------+-------------------+-----------------+
--

CREATE OR REPLACE
  ALGORITHM = TEMPTABLE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW memory_by_user_by_current_bytes (
  user,
  current_count_used,
  current_allocated,
  current_avg_alloc,
  current_max_alloc,
  total_allocated
) AS
SELECT IF(user IS NULL, 'background', user) AS user,
       SUM(current_count_used) AS current_count_used,
<<<<<<< HEAD
       format_bytes(SUM(current_number_of_bytes_used)) AS current_allocated,
       format_bytes(IFNULL(SUM(current_number_of_bytes_used) / NULLIF(SUM(current_count_used), 0), 0)) AS current_avg_alloc,
       format_bytes(MAX(current_number_of_bytes_used)) AS current_max_alloc,
       format_bytes(SUM(sum_number_of_bytes_alloc)) AS total_allocated
=======
       sys.format_bytes(SUM(current_number_of_bytes_used)) AS current_allocated,
       sys.format_bytes(IFNULL(SUM(current_number_of_bytes_used) / NULLIF(SUM(current_count_used), 0), 0)) AS current_avg_alloc,
       sys.format_bytes(MAX(current_number_of_bytes_used)) AS current_max_alloc,
       sys.format_bytes(SUM(sum_number_of_bytes_alloc)) AS total_allocated
>>>>>>> pr/231
  FROM performance_schema.memory_summary_by_user_by_event_name
 GROUP BY IF(user IS NULL, 'background', user)
 ORDER BY SUM(current_number_of_bytes_used) DESC;
