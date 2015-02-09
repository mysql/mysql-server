--disable_query_log
# Cryptic variable name to avoid name conflicts
set @previous_sql_mode_htnt142nh=@@sql_mode;
set sql_mode=(select replace(replace(@@sql_mode, 'STRICT_ALL_TABLES', ''),
			     'STRICT_TRANS_TABLES', ''));
--enable_query_log
