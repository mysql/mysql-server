--eval EXPLAIN $ERC_query
FLUSH STATUS;
--sorted_result
--eval $ERC_query
SHOW STATUS LIKE 'Handler_read_%';
