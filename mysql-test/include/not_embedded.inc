#
# suite.pm will make sure that all tests including this file
# will be skipped unless this is an embedded test run
#
# The test below is redundant

-- require r/not_embedded.require
disable_query_log;
select version() like '%embedded%' as 'have_embedded';
enable_query_log;

