--disable_query_log
# Cryptic variable name to avoid name conflicts
set @previous_sql_mode_htnt542nh=@@sql_mode;
set sql_mode=(select replace(@@sql_mode,'ONLY_FULL_GROUP_BY',''));
--enable_query_log

