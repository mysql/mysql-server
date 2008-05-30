#
# Retrieve the value of  'show status like "Qcache_[inserts, hits, queries]"'
#  and display it in a minimal fashion on one line
#
let $_qcache_inserts= query_get_value(SHOW STATUS LIKE "Qcache_inserts", Value, 1);
let $_qcache_hits= query_get_value(SHOW STATUS LIKE "Qcache_hits", Value, 1);
let $_qcache_queries= query_get_value(SHOW STATUS LIKE "Qcache_queries_in_cache", Value, 1);
echo Qcache_queries          $_qcache_queries;
echo Qcache_inserts          $_qcache_inserts;
echo Qcache_hits             $_qcache_hits;
