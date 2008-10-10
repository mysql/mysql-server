#
# Check if the variable MYSQL_BUG25714 is set
#
--require r/have_bug25714.require
disable_query_log;
eval select LENGTH('$MYSQL_BUG25714') > 0 as 'have_bug25714_exe';
enable_query_log;
