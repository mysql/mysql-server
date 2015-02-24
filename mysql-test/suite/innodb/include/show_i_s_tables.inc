--echo === information_schema.innodb_sys_tables  and innodb_sys_tablespaces ===
--disable_query_log
--replace_result  #P# #p#  #SP# #sp#
SELECT t.name 'Table Name',
	s.name 'Tablespace',
	t.flag 'Table Flags',
	t.n_cols 'Columns',
	t.file_format 'File Format',
	t.row_format 'Row Format',
	t.zip_page_size 'Zip Size',
	t.space_type 'Space Type'
	FROM information_schema.innodb_sys_tables t,
	     information_schema.innodb_sys_tablespaces s
	WHERE t.space = s.space
	AND t.name not like 'SYS_%'
	AND t.name not like 'mysql\/%'
	ORDER BY t.name;
--enable_query_log

