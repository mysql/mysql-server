-- source include/have_query_cache.inc

set GLOBAL query_cache_size=1355776;

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1 (a int not null);
insert into t1 values (1),(2),(3);

--source include/percona_query_cache_with_comments_clear.inc
