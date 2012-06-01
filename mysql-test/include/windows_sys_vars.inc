#
# This file contains default,min and max values for sys variables for windows
#

--disable_query_log

SET @default_key_buffer_size= 131072;

SET @default_max_join_size = 4294967295;
SET @min_max_join_size = 1;
SET @max_max_join_size = 4294967295;

SET @default_sql_max_join_size = 4294967295;
SET @min_sql_max_join_size = 1;
SET @max_sql_max_join_size = 4294967295;

SET @default_sql_select_limit = 4294967295;
SET @min_sql_select_limit = 0;
SET @max_sql_select_limit = 4294967295;

let $default_wait_timeout = 28800;
let $min_wait_timeout = 1;
let $max_wait_timeout = 2147483;

--enable_query_log
