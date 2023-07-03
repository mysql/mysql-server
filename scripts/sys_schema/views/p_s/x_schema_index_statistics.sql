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
-- View: x$schema_index_statistics
--
-- Statistics around indexes.
--
-- Ordered by the total wait time descending - top indexes are most contended.
--
-- mysql> SELECT * FROM x$schema_index_statistics LIMIT 5;
-- +---------------+----------------------+-------------------+---------------+----------------+---------------+----------------+--------------+----------------+--------------+----------------+
-- | table_schema  | table_name           | index_name        | rows_selected | select_latency | rows_inserted | insert_latency | rows_updated | update_latency | rows_deleted | delete_latency |
-- +---------------+----------------------+-------------------+---------------+----------------+---------------+----------------+--------------+----------------+--------------+----------------+
-- | common_schema | _global_sql_tokens   | PRIMARY           |          1886 |     1129676730 |             0 |              0 |            0 |              0 |         1878 |              0 |
-- | common_schema | _script_statements   | PRIMARY           |          4606 |     4212160680 |             0 |              0 |            0 |              0 |            0 |              0 |
-- | common_schema | _global_qs_variables | declaration_depth |           256 |     1650193090 |             0 |              0 |           32 |     1372148050 |            0 |              0 |
-- | common_schema | _global_qs_variables | PRIMARY           |             0 |              0 |             0 |              0 |            0 |              0 |           16 |              0 |
-- | common_schema | metadata             | PRIMARY           |             5 |       76730810 |             0 |              0 |            4 |      114310170 |            0 |              0 |
-- +---------------+----------------------+-------------------+---------------+----------------+---------------+----------------+--------------+----------------+--------------+----------------+
--

CREATE OR REPLACE
  ALGORITHM = MERGE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW x$schema_index_statistics (
  table_schema,
  table_name,
  index_name,
  rows_selected,
  select_latency,
  rows_inserted,
  insert_latency,
  rows_updated,
  update_latency,
  rows_deleted,
  delete_latency
) AS
SELECT OBJECT_SCHEMA AS table_schema,
       OBJECT_NAME AS table_name,
       INDEX_NAME as index_name,
       COUNT_FETCH AS rows_selected,
       SUM_TIMER_FETCH AS select_latency,
       COUNT_INSERT AS rows_inserted,
       SUM_TIMER_INSERT AS insert_latency,
       COUNT_UPDATE AS rows_updated,
       SUM_TIMER_UPDATE AS update_latency,
       COUNT_DELETE AS rows_deleted,
       SUM_TIMER_DELETE AS delete_latency
  FROM performance_schema.table_io_waits_summary_by_index_usage
 WHERE index_name IS NOT NULL
 ORDER BY sum_timer_wait DESC;
