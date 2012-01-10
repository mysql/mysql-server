# Helper for hostcache_*.test

show global status like "%peer%";
--vertical_results
select * from performance_schema.host_cache;
--horizontal_results

