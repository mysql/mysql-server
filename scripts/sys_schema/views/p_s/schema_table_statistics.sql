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
-- View: schema_table_statistics
--
-- Statistics around tables.
--
-- Ordered by the total wait time descending - top tables are most contended.
-- 
-- mysql> SELECT * FROM schema_table_statistics\G
-- *************************** 1. row ***************************
--      table_schema: sys
--        table_name: sys_config
--     total_latency: 0 ps
--      rows_fetched: 0
--     fetch_latency: 0 ps
--     rows_inserted: 0
--    insert_latency: 0 ps
--      rows_updated: 0
--    update_latency: 0 ps
--      rows_deleted: 0
--    delete_latency: 0 ps
--  io_read_requests: 8
--           io_read: 2.28 KiB
--   io_read_latency: 727.32 us
-- io_write_requests: 0
--          io_write: 0 bytes
--  io_write_latency: 0 ps
--  io_misc_requests: 10
--   io_misc_latency: 126.88 us
--

CREATE OR REPLACE
  ALGORITHM = TEMPTABLE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW schema_table_statistics (
  table_schema,
  table_name,
  total_latency,
  rows_fetched,
  fetch_latency,
  rows_inserted,
  insert_latency,
  rows_updated,
  update_latency,
  rows_deleted,
  delete_latency,
  io_read_requests,
  io_read,
  io_read_latency,
  io_write_requests,
  io_write,
  io_write_latency,
  io_misc_requests,
  io_misc_latency
) AS
SELECT pst.object_schema AS table_schema,
       pst.object_name AS table_name,
<<<<<<< HEAD
       format_pico_time(pst.sum_timer_wait) AS total_latency,
       pst.count_fetch AS rows_fetched,
       format_pico_time(pst.sum_timer_fetch) AS fetch_latency,
       pst.count_insert AS rows_inserted,
       format_pico_time(pst.sum_timer_insert) AS insert_latency,
       pst.count_update AS rows_updated,
       format_pico_time(pst.sum_timer_update) AS update_latency,
       pst.count_delete AS rows_deleted,
       format_pico_time(pst.sum_timer_delete) AS delete_latency,
       fsbi.count_read AS io_read_requests,
       format_bytes(fsbi.sum_number_of_bytes_read) AS io_read,
       format_pico_time(fsbi.sum_timer_read) AS io_read_latency,
       fsbi.count_write AS io_write_requests,
       format_bytes(fsbi.sum_number_of_bytes_write) AS io_write,
       format_pico_time(fsbi.sum_timer_write) AS io_write_latency,
       fsbi.count_misc AS io_misc_requests,
       format_pico_time(fsbi.sum_timer_misc) AS io_misc_latency
=======
       sys.format_time(pst.sum_timer_wait) AS total_latency,
       pst.count_fetch AS rows_fetched,
       sys.format_time(pst.sum_timer_fetch) AS fetch_latency,
       pst.count_insert AS rows_inserted,
       sys.format_time(pst.sum_timer_insert) AS insert_latency,
       pst.count_update AS rows_updated,
       sys.format_time(pst.sum_timer_update) AS update_latency,
       pst.count_delete AS rows_deleted,
       sys.format_time(pst.sum_timer_delete) AS delete_latency,
       fsbi.count_read AS io_read_requests,
       sys.format_bytes(fsbi.sum_number_of_bytes_read) AS io_read,
       sys.format_time(fsbi.sum_timer_read) AS io_read_latency,
       fsbi.count_write AS io_write_requests,
       sys.format_bytes(fsbi.sum_number_of_bytes_write) AS io_write,
       sys.format_time(fsbi.sum_timer_write) AS io_write_latency,
       fsbi.count_misc AS io_misc_requests,
       sys.format_time(fsbi.sum_timer_misc) AS io_misc_latency
>>>>>>> pr/231
  FROM performance_schema.table_io_waits_summary_by_table AS pst
  LEFT JOIN x$ps_schema_table_statistics_io AS fsbi
    ON pst.object_schema = fsbi.table_schema
   AND pst.object_name = fsbi.table_name
 ORDER BY pst.sum_timer_wait DESC;
