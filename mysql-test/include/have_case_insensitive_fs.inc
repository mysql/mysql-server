--require r/case_insensitive_fs.require
--disable_query_log
show variables like 'lower_case_file_system';
--enable_query_log
