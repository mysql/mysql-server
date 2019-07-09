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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

--
-- View: schema_unused_indexes
-- 
-- Finds indexes that have had no events against them (and hence, no usage).
--
-- To trust whether the data from this view is representative of your workload,
-- you should ensure that the server has been up for a representative amount of
-- time before using it.
--
-- PRIMARY (key) indexes are ignored.
--
-- mysql> select * from schema_unused_indexes limit 5;
-- +--------------------+---------------------+--------------------+
-- | object_schema      | object_name         | index_name         |
-- +--------------------+---------------------+--------------------+
-- | mem30__bean_config | plists              | path               |
-- | mem30__config      | group_selections    | name               |
-- | mem30__config      | notification_groups | name               |
-- | mem30__config      | user_form_defaults  | FKC1AEF1F9E7EE2CFB |
-- | mem30__enterprise  | whats_new_entries   | entryId            |
-- +--------------------+---------------------+--------------------+
--

CREATE OR REPLACE
  ALGORITHM = MERGE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW schema_unused_indexes (
  object_schema,
  object_name,
  index_name
) AS
SELECT t.object_schema,
       t.object_name,
       t.index_name
  FROM performance_schema.table_io_waits_summary_by_index_usage t
 INNER JOIN information_schema.statistics s
    ON t.object_schema = s.table_schema
   AND t.object_name = s.table_name
   AND t.index_name = s.index_name
 WHERE t.index_name IS NOT NULL
   AND t.count_star = 0
   AND t.object_schema != 'mysql'
   AND t.index_name != 'PRIMARY'
   AND s.NON_UNIQUE = 1
   AND s.SEQ_IN_INDEX = 1
 ORDER BY object_schema, object_name;
