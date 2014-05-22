create database innodb_memcache;

use innodb_memcache;

#-- ------------------------------------------------------------------------
#-- Table `cache_policies`
#--
#-- Each record in this table represents a named caching policy, specifying:
#--  * How the memcache GET command is executed, including whether to get
#--    records from local cache only, from InnoDB only, from local cache if
#--    present (treating InnoDB as a backing store), or not at all.
#--  * Similarly, how memcache SET commands are executed.
#--  * How memcache DELETE commands are executed.
#--  * Whether flushing the cache should cause a mass delete from NDB.
#--
#-- ------------------------------------------------------------------------
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


#-- ------------------------------------------------------------------------
#-- Table `containers`
#--
#-- A container record describes an InnoDB table used for data storage by
#-- InnoDB Memcache.
#-- There must be a unique index on the `key column`, and unique index name
#-- is specified in the `unique_idx_name_on_key` column of the table
#-- `value_columns` are comma-separated lists of the columns that make up
#-- the memcache key and value.
#--
#-- ------------------------------------------------------------------------

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
	
