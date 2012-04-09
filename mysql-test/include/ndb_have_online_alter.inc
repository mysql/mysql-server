#
# Check if the MySQL Server have support for
#  ALTER [ONLINE|OFFLINE] TABLE syntax
#
--disable_query_log
--disable_result_log
let $have_online_alter = 1;
CREATE TABLE check_online_alter(a int primary key);
--error 0,ER_PARSE_ERROR, ER_NOT_SUPPORTED_YET
ALTER ONLINE TABLE check_online_alter ADD COLUMN b int;
if (`SELECT '$mysql_errno' = '1064'`)
{
  let $have_online_alter= 0;
}

# Cleanup
DROP TABLE check_online_alter;

--enable_query_log
--enable_result_log

if (!$have_online_alter)
{
  skip Need ALTER ONLINE syntax;
}
