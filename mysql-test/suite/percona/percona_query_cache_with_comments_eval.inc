echo -----------------------------------------------------;
echo $query;
echo -----------------------------------------------------;
--source percona_query_cache_with_comments_show.inc
eval $query;
eval $query;
--source percona_query_cache_with_comments_show.inc
