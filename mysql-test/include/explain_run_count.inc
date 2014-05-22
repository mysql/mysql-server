--eval EXPLAIN $ERC_query
FLUSH STATUS;
--eval $ERC_query
SHOW STATUS LIKE 'Handler_read_%';
