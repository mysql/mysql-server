-- Copyright (c)2011, Oracle and/or its affiliates. All rights
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

-- Configuration version 1.2


CREATE SCHEMA IF NOT EXISTS `ndbmemcache` DEFAULT CHARACTER SET latin1 ;
USE `ndbmemcache` ;

-- ------------------------------------------------------------------------
-- Table `meta`
--
-- At startup, memcached (and possibly other applications) will check for the 
-- existance of one or more <my-application-name, metadata-version> tuples, 
-- and proceed to process the configuration based on the ones found.  
-- End-users should consider it to be a private and undocumented system table.
-- 
-- ------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `meta` (
  `application` VARCHAR(16) NOT NULL ,
  `metadata_version` VARCHAR(6) NOT NULL ,
  PRIMARY KEY (`application`, `metadata_version`)
)
ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- Table `ndb_clusters`
--
-- Each record in this table represents a MySQL Cluster.  
-- The record with cluster_id 0 is the primary cluster, which stores this
-- metadata schema.  Additional clusters, storing application data only, may
-- also be defined.  
-- The microsec_rtt field is one of two performance-related parameters -- see 
-- the discussion below regarding memcache_server_roles. 
-- 
-- ------------------------------------------------------------------------
CREATE  TABLE IF NOT EXISTS `ndb_clusters` (
  `cluster_id` INT NOT NULL ,
  `ndb_connectstring` VARCHAR(128) NULL ,
  `microsec_rtt` INT UNSIGNED NOT NULL default 250, 
  PRIMARY KEY (`cluster_id`) )
ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- Table `memcache_server_roles`
-- 
-- Each memcache_server performs a role, which can be selected by the role 
-- name supplied on the memcached command line.  The default is named 
-- "default_role".  The role_id from this table is used in the primary key
-- of the key_prefixes table.
-- 
-- The two performance-related tunables in the NDB Memcache configuration are  
-- the fields ndb_clusters.microsec_rtt and memcache_server_roles.max_tps.
-- The NDB Memcache engine will attempt to size its internal data structures so 
-- that max_tps transactions per second can be attained.  This involves
-- multiplying max_tps times microsec_rtt times a fixed constant, to calculate 
-- the number of concurrent in-flight transactions that must be supported.  
-- For example, using the default values of 100,000 TPS and 250 usec RTT: 
-- 100000 tx/sec. * .00025 sec * 5 = 125 in-flight transactions.
-- (The value of this fixed constant is an internal implementation detail and
-- is subject to change, but if you start the server with "debug=true" you can
-- see the results of this calculation in how many NDB instances are created).
--
-- ------------------------------------------------------------------------
CREATE  TABLE IF NOT EXISTS `memcache_server_roles` (
  `role_name` VARCHAR(40) NOT NULL ,
  `role_id` INT UNSIGNED NOT NULL ,
  `max_tps` INT UNSIGNED NOT NULL default 100000,
  `update_timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`role_name`) )
ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- Table `cache_policies`
--
-- Each record in this table represents a named caching policy, specifying:
--  * How the memcache GET command is executed, including whether to get
--    records from local cache only, from NDB only, from local cache if 
--    present (treating NDB as a backing store), or not at all.  
--  * Similarly, how memcache SET commands are executed.
--  * How memcache DELETE commands are executed. 
--  * Whether flushing the cache should cause a mass delete from NDB.
--
-- ------------------------------------------------------------------------
CREATE  TABLE IF NOT EXISTS `cache_policies` (
  `policy_name` VARCHAR(40) NOT NULL PRIMARY KEY,
  `get_policy` ENUM('cache_only','ndb_only','caching','disabled') NOT NULL ,
  `set_policy` ENUM('cache_only','ndb_only','caching','disabled') NOT NULL ,
  `delete_policy` ENUM('cache_only','ndb_only','caching','disabled') NOT NULL ,
  `flush_from_db` ENUM('false', 'true') NOT NULL DEFAULT 'false'
) ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- Table `containers`
-- 
-- A container record describes an NDB table used for data storage by
-- NDB Memcache.
-- `key_columns` and `value_columns` are comma-separated lists of the 
-- columns that make up the memcache key and value.  
-- If `flags` is numeric, its value is used by memcached as the flags 
-- value for every response.  If it holds the name of a column, then 
-- that column is used to store and retrieve the flags.
--  
-- ------------------------------------------------------------------------
CREATE  TABLE IF NOT EXISTS `containers` (
  `name` varchar(50) not null primary key,
  `db_schema` VARCHAR(250) NOT NULL,
  `db_table` VARCHAR(250) NOT NULL,
  `key_columns` VARCHAR(250) NOT NULL,
  `value_columns` VARCHAR(250),
  `flags` VARCHAR(250) NOT NULL DEFAULT "0",
  `increment_column` VARCHAR(250),
  `cas_column` VARCHAR(250),
  `expire_time_column` VARCHAR(250),
  `large_values_table` VARCHAR(250) 
) ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- Table `key_prefixes`
-- 
-- A key_prefixes record is the culmination of the configuration.  
-- For a particular memcache server role, and a prefix of a memcache key,
-- it defines a cluster, cache policy, and container.  
-- 
-- ------------------------------------------------------------------------
CREATE  TABLE IF NOT EXISTS `key_prefixes` (
  `server_role_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `key_prefix` VARCHAR(250) NOT NULL ,
  `cluster_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `policy` VARCHAR(40) NOT NULL,
  `container` VARCHAR(50), 
  PRIMARY KEY (`server_role_id`, `key_prefix`)
) ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- Table `last_memcached_signon`
-- 
-- Whenever a memcached server starts up and reads its configuration, it
-- updates its signon record in this table.
-- 
-- ------------------------------------------------------------------------
CREATE  TABLE IF NOT EXISTS `last_memcached_signon` (
  `ndb_node_id` INT UNSIGNED NOT NULL PRIMARY KEY,
  `hostname` VARCHAR(255) NOT NULL, 
  `server_role` VARCHAR(40) NOT NULL,
  `signon_time` timestamp NOT NULL 
) ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- DEMONSTRATION TABLES 
-- ------------------------------------------------------------------------

-- ------------------------------------------------------------------------
-- Table `demo_table`
-- 
-- The demo_table is not strictly part of the configuration schema, but
-- it is defined as a container by the pre-loaded default configuration
-- and holds application data.
-- 
-- ------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `demo_table` (
  `mkey` VARCHAR(250) NOT NULL ,
  `math_value` BIGINT UNSIGNED,
  `cas_value` BIGINT UNSIGNED, 
  `string_value` VARBINARY(13500),
  PRIMARY KEY USING HASH (mkey)
) ENGINE = ndbcluster;

-- ------------------------------------------------------------------------
-- Table `demo_table_large`
--
-- A table demonstrating externalized large values.
-- Use with key prefix "b:"
--
-- ------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `demo_table_large` (
  `mkey` VARCHAR(250) NOT NULL,
  `cas_value` BIGINT UNSIGNED,
  `string_value` VARBINARY(2000), 
  `ext_id` INT UNSIGNED,
  `ext_size` INT UNSIGNED,
  PRIMARY KEY USING HASH (mkey)
) ENGINE = ndbcluster;

-- ------------------------------------------------------------------------
-- Table `external_values` 
--
-- Used with demo_table_large to store large values 
--
-- ------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `external_values` (
  `id` INT UNSIGNED AUTO_INCREMENT NOT NULL,
  `part` SMALLINT NOT NULL,
  `content` VARBINARY(13950) NOT NULL,
  PRIMARY KEY (id,part)
 ) ENGINE = ndbcluster;


-- ------------------------------------------------------------------------
-- Table `demo_table_tabs`
--
-- A table demonstrating three value columns, tab-separated output, and 
-- expire times. 
-- By default you can use this table with key prefix "t:"
--
-- ------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `demo_table_tabs` (
  `mkey` INT NOT NULL PRIMARY KEY,
  `val1` VARCHAR(60),
  `val2` varchar(10),
  `val3` INT,
  `flags` INT UNSIGNED NULL,
   expire_time TIMESTAMP NULL
) ENGINE = ndbcluster;


-- -----------------------------------------------------
-- Default entries for all tables 
-- -----------------------------------------------------

-- meta table
-- Create an entry for ndbmemcache metadata v1
INSERT INTO meta VALUES ("ndbmemcache", "1.2");

-- memcache_server_roles table
-- Create an entry for the default role and example roles.
INSERT INTO memcache_server_roles (role_name, role_id) VALUES ("default_role", 0);
INSERT INTO memcache_server_roles (role_name, role_id) VALUES ("db-only", 1);
INSERT INTO memcache_server_roles (role_name, role_id) VALUES ("mc-only", 2);
INSERT INTO memcache_server_roles (role_name, role_id) VALUES ("ndb-caching", 3);
INSERT INTO memcache_server_roles (role_name, role_id) VALUES ("large", 4);

-- ndb_clusters table 
-- Create an entry for the primary cluster.
INSERT INTO ndb_clusters values (0, @@ndb_connectstring, 250);

-- cache_policies table
-- Create some sample policies.
INSERT INTO cache_policies (policy_name, get_policy, set_policy, delete_policy)
  VALUES("memcache-only", "cache_only", "cache_only", "cache_only"),
        ("ndb-only", "ndb_only", "ndb_only", "ndb_only"),  /* no flush */
        ("ndb-test", "ndb_only", "ndb_only", "ndb_only"),  /* flush allowed */
        ("caching", "caching", "caching", "caching"),
        ("caching-with-local-deletes", "caching", "caching", "cache_only"),
        ("ndb-read-only", "ndb_only", "disabled", "disabled");

-- FLUSH_ALL is enabled on the ndb-test policy
UPDATE cache_policies set flush_from_db = "true" where policy_name = "ndb-test";

-- containers table 
INSERT INTO containers 
  SET name = "demo_table", db_schema = "ndbmemcache", db_table = "demo_table",
      key_columns = "mkey", value_columns = "string_value", 
      increment_column = "math_value", cas_column = "cas_value";

INSERT INTO containers 
  SET name = "demo_tabs", db_schema = "ndbmemcache", db_table = "demo_table_tabs",
      key_columns = "mkey", value_columns = "val1,val2,val3", flags = "flags",
      expire_time_column = "expire_time";

INSERT INTO containers
  SET name = "demo_ext", db_schema = "ndbmemcache", db_table = "demo_table_large",
      key_columns = "mkey", value_columns = "string_value", 
      cas_column = "cas_value", 
      large_values_table = "ndbmemcache.external_values";

-- key_prefixes table
INSERT INTO key_prefixes (server_role_id, key_prefix, cluster_id, 
                          policy, container)
  VALUES (0, "",    0, "ndb-test", "demo_table"),
         (0, "mc:", 0, "memcache-only", NULL),
         (0, "t:",  0, "ndb-test", "demo_tabs"),    /* default role */
         (0, "b:",  0, "ndb-test", "demo_ext"),

         (1, "",    0, "ndb-only", "demo_table"),
         (1, "t:",  0, "ndb-only", "demo_tabs"),    /* db-only role */
         (1, "b:",  0, "ndb-only", "demo_ext"),

         (2, "",    0, "memcache-only", NULL),      /* mc-only role */

         (3, "",    0, "caching", "demo_table"),    /* ndb-caching role */
         (3, "t:",  0, "caching", "demo_tabs"),
         (3, "b:",  0, "caching", "demo_ext"),
         
         (4, ""  ,  0, "ndb-test", "demo_ext");
;




 