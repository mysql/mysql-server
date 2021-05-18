
--disable_query_log
--disable_result_log

use ndbmemcache;
let $done = query_get_value (select key_prefix k from key_prefixes where container = 'test_mkey', k, 1);
if ($done != "hashtag:") 
{
--source suite/ndb_memcache/include/memcached_wait_for_ready.inc
set default_storage_engine = ndbcluster;

CREATE TABLE hashtags (
  hashtag varchar(20),
  tweet_id int,
  author varchar(15),
  primary key(hashtag, tweet_id));

INSERT INTO containers (name, db_schema, db_table, key_columns, value_columns)
  VALUES("test_mkey", "ndbmemcache", "hashtags", "hashtag,tweet_id", "author");

INSERT INTO key_prefixes(server_role_id, key_prefix, policy, container) 
  VALUES(0, "hashtag:", "ndb-only", "test_mkey");


CREATE TABLE test_unique_idx (pkey int PRIMARY KEY, 
                              mkey char(40), 
                              val varchar(200),
                              UNIQUE INDEX (mkey)
                              );
INSERT INTO containers (name, db_schema, db_table, key_columns, value_columns)
  VALUES("tt_uidx_pk", "ndbmemcache", "test_unique_idx", "pkey", "mkey,val"),
        ("tt_uidx_rd", "ndbmemcache", "test_unique_idx", "mkey", "pkey,val"),
        ("tt_uidx_uk", "ndbmemcache", "test_unique_idx", "mkey", "val");

INSERT INTO key_prefixes(server_role_id, key_prefix, policy, container) 
  VALUES(0, "tup:", "ndb-only", "tt_uidx_pk"),
        (0, "tur:", "ndb-only", "tt_uidx_rd"),
        (0, "tui:", "ndb-only", "tt_uidx_uk");

SET ndb_table_no_logging = 1;
CREATE TABLE IF NOT EXISTS large_nolog (
  `mkey` VARCHAR(250) NOT NULL,
  `cas_value` BIGINT UNSIGNED,
  `string_value` VARBINARY(2000), 
  `ext_id` INT UNSIGNED,
  `ext_size` INT UNSIGNED,
  PRIMARY KEY USING HASH (mkey)
);

CREATE TABLE `external_nolog` (
  `id` INT UNSIGNED AUTO_INCREMENT NOT NULL,
  `part` SMALLINT NOT NULL,
  `content` VARBINARY(13950) NOT NULL,
  PRIMARY KEY (id,part)
 );
SET ndb_table_no_logging = 0;

INSERT INTO containers
  SET name = "ext_no_log", db_schema = "ndbmemcache", 
      db_table = "large_nolog",
      key_columns = "mkey", value_columns = "string_value", 
      cas_column = "cas_value", 
      large_values_table = "ndbmemcache.external_nolog";

INSERT INTO key_prefixes(server_role_id, key_prefix, policy, container) 
  VALUES(0, "bxx:", "ndb-test", "ext_no_log");

UPDATE memcache_server_roles set update_timestamp = NOW() where role_id = 0;

--source suite/ndb_memcache/include/wait_for_reconf.inc
}

--enable_query_log
--enable_result_log

