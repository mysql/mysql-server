#
# This file contains default,min and max values for sys variables for redhat linux
#

--disable_query_log

SET @default_key_buffer_size = 131072;

SET @default_max_join_size = 4294967295;
SET @min_max_join_size = 1;
SET @max_max_join_size = 4294967295;

SET @default_sql_max_join_size = 4294967295;
SET @min_sql_max_join_size = 1;
SET @max_sql_max_join_size = 4294967295;

SET @default_sql_select_limit = 4294967295;
SET @min_sql_select_limit = 0;
SET @max_sql_select_limit = 4294967295;

SET @default_wait_timeout= 28800;
SET @min_wait_timeout= 1;
SET @max_wait_timeout= 31536000;

--enable_query_log
