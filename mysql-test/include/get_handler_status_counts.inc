#
# Get Handler_% values and suppress SHOW STATUS deprecation warning.
#
--disable_query_log
--skip_if_hypergraph  #  Depends on the query plan.
eval $get_handler_status_counts;
--enable_query_log
