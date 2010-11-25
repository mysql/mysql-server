echo -----------------------------------------------------;
echo $query;
echo -----------------------------------------------------;
--source include/percona_query_cache_with_comments_show.inc
eval $query;
eval $query;
--source include/percona_query_cache_with_comments_show.inc
