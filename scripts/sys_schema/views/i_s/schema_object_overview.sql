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
-- View: schema_object_overview
-- 
-- Shows an overview of the types of objects within each schema
--
-- Note: On instances with a large number of objects, this could take
--       some time to execute, and is not recommended.
--
-- mysql> select * from schema_object_overview;
-- +---------------------------------+---------------+-------+
-- | db                              | object_type   | count |
-- +---------------------------------+---------------+-------+
-- | information_schema              | SYSTEM VIEW   |    59 |
-- | mem30_test__instruments         | BASE TABLE    |     1 |
-- | mem30_test__instruments         | INDEX (BTREE) |     2 |
-- | mem30_test__test                | BASE TABLE    |     9 |
-- | mem30_test__test                | INDEX (BTREE) |    19 |
-- ...
-- | sys                             | FUNCTION      |     8 |
-- | sys                             | PROCEDURE     |    16 |
-- | sys                             | VIEW          |    59 |
-- +---------------------------------+---------------+-------+
--

CREATE OR REPLACE
  ALGORITHM = TEMPTABLE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW schema_object_overview (
  db,
  object_type,
  count
) AS
SELECT ROUTINE_SCHEMA AS db, ROUTINE_TYPE AS object_type, COUNT(*) AS count FROM information_schema.routines GROUP BY ROUTINE_SCHEMA, ROUTINE_TYPE
 UNION 
SELECT TABLE_SCHEMA, TABLE_TYPE, COUNT(*) FROM information_schema.tables GROUP BY TABLE_SCHEMA, TABLE_TYPE
 UNION
SELECT TABLE_SCHEMA, CONCAT('INDEX (', INDEX_TYPE, ')'), COUNT(*) FROM information_schema.statistics GROUP BY TABLE_SCHEMA, INDEX_TYPE
 UNION
SELECT TRIGGER_SCHEMA, 'TRIGGER', COUNT(*) FROM information_schema.triggers GROUP BY TRIGGER_SCHEMA
 UNION
SELECT EVENT_SCHEMA, 'EVENT', COUNT(*) FROM information_schema.events GROUP BY EVENT_SCHEMA
ORDER BY DB, OBJECT_TYPE;
