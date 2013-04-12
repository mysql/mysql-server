# Note : The contents of this file have been taken from ps_query.inc file as
# the output differs for different MAX_INDEXES value.

--disable_query_log
select '------ explain select tests ------' as test_sequence ;
--enable_query_log
# table with many column types
prepare stmt1 from ' explain select * from t9 ' ;
--enable_metadata
execute stmt1;
--disable_metadata

