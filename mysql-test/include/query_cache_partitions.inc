# include/query_cache_partitions.inc
#
# The variables
#     $engine_type       -- storage engine to be tested
# have to be set before sourcing this script.

eval SET SESSION STORAGE_ENGINE = $engine_type;

# Initialise
--disable_warnings
drop table if exists t1;
--enable_warnings

set @save_query_cache_size = @@global.query_cache_size;

--echo # Test that partitions works with query cache

flush query cache;

SET GLOBAL query_cache_size=1024*1024*512;
  CREATE TABLE `t1` (
    `id` int(11) NOT NULL ,
    `created_at` datetime NOT NULL,
    `cool` tinyint default 0
  );

  ALTER TABLE t1 PARTITION BY RANGE (TO_DAYS(created_at)) (
    PARTITION month_2010_4 VALUES LESS THAN (734258),
    PARTITION month_2010_5 VALUES LESS THAN (734289),
    PARTITION month_max VALUES LESS THAN MAXVALUE
  );

show create table t1;

INSERT INTO t1 VALUES (1, now(), 0);

flush status;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";


SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;

show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";

drop table t1;

--echo # Test that sub-partitions works with query cache

flush query cache;

SET GLOBAL query_cache_size=1024*1024*512;
  CREATE TABLE `t1` (
    `id` int(11) NOT NULL ,
    `created_at` datetime NOT NULL,
    `cool` tinyint default 0
  )
    PARTITION BY RANGE (TO_DAYS(created_at))
    subpartition by hash(cool) subpartitions 3 (
    PARTITION month_2010_4 VALUES LESS THAN (734258),
    PARTITION month_2010_5 VALUES LESS THAN (734289),
    PARTITION month_max VALUES LESS THAN MAXVALUE
  );

show create table t1;

INSERT INTO t1 VALUES (1, now(), 0);

flush status;
show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";

SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;

show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";

drop table t1;

--echo #
--echo # MySQL bug#53775 Query on partitioned table returns cached result
--echo # from previous transaction
--echo #

flush query cache;
flush status;

SET GLOBAL query_cache_size=1024*1024*512;
  CREATE TABLE `t1` (
    `id` int(11) NOT NULL ,
    `created_at` datetime NOT NULL,
    `cool` tinyint default 0
  );

  ALTER TABLE t1 PARTITION BY RANGE (TO_DAYS(created_at)) (
    PARTITION month_2010_4 VALUES LESS THAN (734258),
    PARTITION month_2010_5 VALUES LESS THAN (734289),
    PARTITION month_max VALUES LESS THAN MAXVALUE
  );

INSERT INTO t1 VALUES (1, now(), 0);

show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";

BEGIN;
UPDATE `t1` SET `cool` = 1 WHERE `id` = 1;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;
ROLLBACK;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;
BEGIN;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;
ROLLBACK;
SELECT cool FROM `t1` WHERE (`t1`.id = 1) LIMIT 1;

show status like "Qcache_queries_in_cache";
show status like "Qcache_hits";

drop table t1;

set @@global.query_cache_size = @save_query_cache_size;
