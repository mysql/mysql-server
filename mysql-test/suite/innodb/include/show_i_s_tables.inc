--echo === information_schema.innodb_sys_tables  and innodb_sys_tablespaces ===
--disable_query_log
--replace_result  #P# #p#  #SP# #sp#
--replace_regex /FTS_([0-9a-f_]+)([A-Z0-9_]+)/FTS_AUX_\2/

SELECT t.name 'Table Name',
	IFNULL(s.name, 'innodb_system') 'Tablespace',
	t.flag 'Table Flags',
	t.n_cols 'Columns',
	t.row_format 'Row Format',
	t.zip_page_size 'Zip Size',
	t.space_type 'Space Type'
	FROM information_schema.innodb_sys_tables t LEFT JOIN
	     information_schema.innodb_sys_tablespaces s
	ON t.space = s.space
	WHERE t.name not like 'SYS_%'
	AND t.name NOT LIKE 'mysql/%'
	AND t.name NOT LIKE 'sys/%'
	ORDER BY t.name;
--enable_query_log

