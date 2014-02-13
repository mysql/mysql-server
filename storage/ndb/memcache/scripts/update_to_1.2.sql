-- Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.
-- reserved.
-- 
-- This program is free software; you can redistribute it and/or
-- modify it under the terms of the GNU General Public License
-- as published by the Free Software Foundation; version 2 of
-- the License.
-- 
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
-- 02110-1301  USA


-- Upgrade from configuration version 1.1 to version 1.2

-- This update adds schema support for externalized large values

-- Config version 1.2 became current with MySQL Cluster release 7.2.4
-- It remained current through release 7.2.11 
-- and was then replaced by config version 1.2a


USE ndbmemcache;

ALTER TABLE containers add `large_values_table` VARCHAR(250) ;
ALTER TABLE demo_table change string_value `string_value` VARBINARY(13500);

ALTER TABLE demo_table_tabs 
 ADD flags INT UNSIGNED NULL,
 ADD expire_time timestamp NULL;


CREATE TABLE IF NOT EXISTS `demo_table_large` (
  `mkey` VARCHAR(250) NOT NULL,
  `cas_value` BIGINT UNSIGNED,
  `string_value` VARBINARY(2000), 
  `ext_id` INT UNSIGNED,
  `ext_size` INT UNSIGNED,
  PRIMARY KEY USING HASH (mkey)
) ENGINE = ndbcluster;


CREATE TABLE IF NOT EXISTS `external_values` (
  `id` INT UNSIGNED AUTO_INCREMENT NOT NULL,
  `part` SMALLINT NOT NULL,
  `content` VARBINARY(13950) NOT NULL,
  PRIMARY KEY (id,part)
 ) ENGINE = ndbcluster;

INSERT INTO meta VALUES ("ndbmemcache", "1.2");

INSERT INTO memcache_server_roles (role_name, role_id) VALUES ("large", 4);

UPDATE  containers 
  SET   expire_time_column = "expire_time",
        flags = "flags" 
  WHERE name = "demo_tabs";

INSERT INTO containers
  SET name = "demo_ext", db_schema = "ndbmemcache", 
      db_table = "demo_table_large",
      key_columns = "mkey", value_columns = "string_value", 
      cas_column = "cas_value", 
      large_values_table = "ndbmemcache.external_values";


INSERT INTO key_prefixes (server_role_id, key_prefix, cluster_id, 
                          policy, container)
  VALUES
          (0, "b:",  0, "ndb-test", "demo_ext"),
          (1, "b:",  0, "ndb-only", "demo_ext"),
          (3, "t:",  0, "caching", "demo_tabs"),
          (3, "b:",  0, "caching", "demo_ext"),
          (4, ""  ,  0, "ndb-test", "demo_ext");

 

