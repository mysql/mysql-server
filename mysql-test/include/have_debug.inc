#
# suite.pm will make sure that all tests including this file
# will be skipped unless this is a debug build.
#
# The test below is redundant

-- require r/have_debug.require
disable_query_log;
select (version() like '%debug%') as debug;
enable_query_log;
