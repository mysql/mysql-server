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
-- View: version
--
<<<<<<< HEAD
-- Shows the sys schema and mysql versions
--
-- NOTE: This view is deprecated and will be removed in a future release.
--
=======
>>>>>>> pr/231
-- mysql> select * from sys.version;
-- +-------------+---------------+
-- | sys_version | mysql_version |
-- +-------------+---------------+
<<<<<<< HEAD
-- | 2.1.2       | 8.0.28        |
=======
-- | 1.5.2       | 5.7.28        |
>>>>>>> pr/231
-- +-------------+---------------+
-- 

CREATE OR REPLACE
  DEFINER = 'mysql.sys'@'localhost'
  SQL SECURITY INVOKER 
VIEW version (
  sys_version,
  mysql_version
) AS 
<<<<<<< HEAD
SELECT '2.1.2' AS sys_version,
=======
SELECT '1.5.2' AS sys_version,
>>>>>>> pr/231
        version() AS mysql_version;
