let $show=show status like "Qcache_queries_in_cache";
eval $show;
let $show=show status like "Qcache_inserts";
eval $show;
let $show=show status like "Qcache_hits";
eval $show;


