-- require r/have_query_cache.require
# As PS are not cached we disable them to ensure the we get the right number
# of query cache hits
-- disable_ps_protocol
disable_query_log;
show variables like "have_query_cache";
enable_query_log;
