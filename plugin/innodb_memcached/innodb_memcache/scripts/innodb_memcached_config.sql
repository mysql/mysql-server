create database innodb_memcache;

use innodb_memcache;

-- ------------------------------------------------------------------------
-- Table `cache_policies`
--
-- Each record in this table represents a named caching policy, specifying:
--  * How the memcache GET command is executed, including whether to get
--    records from local cache only, from InnoDB only, from local cache if
--    present (treating InnoDB as a backing store), or not at all.
--  * Similarly, how memcache SET commands are executed.
--  * How memcache DELETE commands are executed.
--  * Whether flushing the cache should cause a mass delete from NDB.
--
-- ------------------------------------------------------------------------
CREATE  TABLE IF NOT EXISTS `cache_policies` (
	`policy_name` VARCHAR(40) PRIMARY KEY,
	`get_policy` ENUM('innodb_only', 'cache_only', 'caching','disabled')
	 NOT NULL ,
	`set_policy` ENUM('innodb_only', 'cache_only','caching','disabled')
	 NOT NULL ,
	`delete_policy` ENUM('innodb_only', 'cache_only', 'caching','disabled')
	 NOT NULL,
	`flush_policy` ENUM('innodb_only', 'cache_only', 'caching','disabled')
	 NOT NULL
) ENGINE = innodb;


-- ------------------------------------------------------------------------
-- Table `containers`
--
-- A container record describes an InnoDB table used for data storage by
-- InnoDB Memcache.
-- There must be a unique index on the `key column`, and unique index name
-- is specified in the `unique_idx_name_on_key` column of the table
-- `value_columns` are comma-separated lists of the columns that make up
-- the memcache key and value.
--
-- ------------------------------------------------------------------------

CREATE  TABLE IF NOT EXISTS `containers` (
	`name` varchar(50) not null primary key,
	`db_schema` VARCHAR(250) NOT NULL,
	`db_table` VARCHAR(250) NOT NULL,
	`key_columns` VARCHAR(250) NOT NULL,
	`value_columns` VARCHAR(250),
	`flags` VARCHAR(250) NOT NULL DEFAULT "0",
	`cas_column` VARCHAR(250),
	`expire_time_column` VARCHAR(250),
	`unique_idx_name_on_key` VARCHAR(250) NOT NULL
) ENGINE = InnoDB;

CREATE  TABLE IF NOT EXISTS `config_options` (
	`name` varchar(50) not null primary key,
	`value` varchar(50)) ENGINE = InnoDB;
	
-- ------------------------------------------------------------------------
-- Follow is an example
-- We create a InnoDB table `demo_test` is the `test` database
-- and insert an entry into contrainers' table to tell InnoDB Memcache
-- that we has such InnoDB table as back store:
-- c1 -> key
-- c2 -> value
-- c3 -> flags
-- c4 -> cas
-- c5 -> exp time
-- primary -> Use Primary index for search
-- ------------------------------------------------------------------------

INSERT INTO containers VALUES ("example_1", "test", "demo_test",
			       "c1", "c2",  "c3", "c4", "c5", "PRIMARY");

INSERT INTO cache_policies VALUES("cache_policy", "innodb_only",
				  "innodb_only", "innodb_only", "innodb_only");

INSERT INTO config_options VALUES("separator", "|");

USE test

CREATE TABLE demo_test (c1 CHAR(64), c2 VARCHAR(1024),
			c3 INT, c4 BIGINT UNSIGNED, C5 INT, PRIMARY KEY(c1))
ENGINE = INNODB;

INSERT INTO demo_test VALUES ("AA", "HELLO, HELLO", 0, 0, 0);

-- ------------------------------------------------------------------------
-- Follow shows another example, in that case, there are more table columns
-- than actually being mapped. And we also specified a secondary index
-- for the query
-- c1 -> key
-- c2 -> value
-- c3 -> flags
-- c4 -> cas
-- c5 -> exp time
-- idx -> unique index on key
-- ------------------------------------------------------------------------
-- ------------------------------------------------------------------------
-- INSERT INTO containers VALUES ("aaa", "test", "demo_test",
--			       "c1", "c2, cx",  "c3", "c4", "c5", "idx");
--
-- INSERT INTO cache_policies VALUES("cache_policy", "innodb_only",
--				  "innodb_only", "innodb_only", "innodb_only");
--
-- INSERT INTO config_options VALUES("separator", "|");
--
-- USE test
--
-- CREATE TABLE demo_test (cx VARCHAR(10), cy INT, c1 VARCHAR(32),
--			cz INT, c2 VARCHAR(1024), ca INT, CB INT,
--			c3 INT, cu INT, c4 BIGINT UNSIGNED, C5 INT)
-- ENGINE = INNODB;
--
-- CREATE UNIQUE INDEX idx ON demo_test(c1);
--
-- INSERT INTO demo_test VALUES ("9", 3 , "AA", 2, "HELLO, HELLO",
--			      8, 8, 0, 1, 3, 0);
-- ------------------------------------------------------------------------


