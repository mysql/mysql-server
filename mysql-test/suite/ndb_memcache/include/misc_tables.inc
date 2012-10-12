
--disable_query_log
--disable_result_log

--source suite/ndb_memcache/include/memcached_wait_for_ready.inc

use ndbmemcache;
let $done = query_get_value (select key_prefix k from key_prefixes where container = 'test_mkey', k, 1);
if ($done != "hashtag:") 
{
                
set default_storage_engine = ndbcluster;

CREATE TABLE hashtags (
  hashtag varchar(20),
  tweet_id int,
  author varchar(15),
  primary key(hashtag, tweet_id));

CREATE TABLE test_unique_idx (pkey int PRIMARY KEY, 
                              mkey char(40), 
                              val varchar(200),
                              UNIQUE INDEX (mkey)
                              );


INSERT INTO containers (name, db_schema, db_table, key_columns, value_columns)
  VALUES("test_mkey", "ndbmemcache", "hashtags", "hashtag,tweet_id", "author");

INSERT INTO key_prefixes(server_role_id, key_prefix, policy, container) 
  VALUES(0, "hashtag:", "ndb-only", "test_mkey");

INSERT INTO containers (name, db_schema, db_table, key_columns, value_columns)
  VALUES("tt_uidx_pk", "ndbmemcache", "test_unique_idx", "pkey", "mkey,val"),
        ("tt_uidx_rd", "ndbmemcache", "test_unique_idx", "mkey", "pkey,val"),
        ("tt_uidx_uk", "ndbmemcache", "test_unique_idx", "mkey", "val");

INSERT INTO key_prefixes(server_role_id, key_prefix, policy, container) 
  VALUES(0, "tup:", "ndb-only", "tt_uidx_pk"),
        (0, "tur:", "ndb-only", "tt_uidx_rd"),
        (0, "tui:", "ndb-only", "tt_uidx_uk");

UPDATE memcache_server_roles set update_timestamp = NOW() where role_id = 0;

--source suite/ndb_memcache/include/wait_for_reconf.inc
}

--enable_query_log
--enable_result_log

