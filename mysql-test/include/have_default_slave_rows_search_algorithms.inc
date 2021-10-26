if (`SELECT @@global.slave_rows_search_algorithms <> 'INDEX_SCAN,HASH_SCAN'`)
{
  --skip Test runs only with default value for slave_rows_search_algorithms
}