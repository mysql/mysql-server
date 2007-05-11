--require r/lowercase1.require
--disable_query_log
show variables like 'lower_case_table_names';
--enable_query_log
