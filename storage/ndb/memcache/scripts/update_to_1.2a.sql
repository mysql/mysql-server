-- Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.
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

-- Upgrade from configuration version 1.2 to version 1.2a
-- Introduced in MySQL Cluster releases 7.2.12 and 7.3

-- This upgrade ensures that Memcache flags are correctly supported in the 
-- demo tables, which allows certain clients to work better in an 
-- "out of the box" demo.

-- Only the demo tables are updated, so we do not create a "meta" record 
-- for this version, and we refer to it as 1.2a.



USE ndbmemcache;

ALTER TABLE demo_table 
  ADD flags int unsigned AFTER math_value;

ALTER TABLE demo_table_large 
  ADD flags int unsigned AFTER mkey;
  
UPDATE containers 
  SET flags = "flags" where name = "demo_table";

UPDATE containers
  SET flags = "flags" where name = "demo_ext";


