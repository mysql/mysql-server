--echo === information_schema.innodb_tables  and innodb_tablespaces ===
--disable_query_log
--replace_result  #P# #p#  #SP# #sp#
--replace_regex /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/ /fts_([0-9a-f_]+)([A-Z0-9_]+)/fts_aux\2/

SELECT t.name 'Table Name',
	IFNULL(s.name, 'innodb_system') 'Tablespace',
	t.flag 'Table Flags',
	t.n_cols 'Columns',
	t.row_format 'Row Format',
	t.zip_page_size 'Zip Size',
	t.space_type 'Space Type'
	FROM information_schema.innodb_tables t LEFT JOIN
	     information_schema.innodb_tablespaces s
	ON t.space = s.space
	WHERE t.name not like 'mysql/%'
	AND t.name NOT LIKE 'sys/%'
	ORDER BY t.name;
--enable_query_log

