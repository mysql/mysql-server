--eval EXPLAIN $ERC_query
FLUSH STATUS;
--sorted_result
--eval $ERC_query
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS LIKE 'Handler_read_%';
