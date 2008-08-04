--require r/have_ndbapi_examples.require
disable_query_log;
eval select LENGTH('$NDB_EXAMPLES_BINARY') > 0 as 'have_ndb_example';
enable_query_log;
