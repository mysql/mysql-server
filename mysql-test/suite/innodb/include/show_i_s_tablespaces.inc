# This script assumes that the caller did the following;
#     LET $MYSQLD_DATADIR = `select @@datadir`;
#     LET $INNODB_PAGE_SIZE = `select @@innodb_page_size`;
--echo === information_schema.innodb_sys_tablespaces and innodb_sys_datafiles ===
--disable_query_log
--replace_regex  /#P#/#p#/  /#SP#/#sp#/
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/  $MYSQL_TMP_DIR MYSQL_TMP_DIR  $INNODB_PAGE_SIZE DEFAULT
SELECT s.name 'Space_Name',
	s.space_type 'Space_Type',
	s.page_size 'Page_Size',
	s.zip_page_size 'Zip_Size',
	s.row_format 'Formats_Permitted',
	d.path 'Path'
	FROM information_schema.innodb_sys_tablespaces s,
	     information_schema.innodb_sys_datafiles d
	WHERE s.space = d.space
	AND s.name NOT LIKE 'mysql/%'
	AND s.name NOT LIKE 'sys/%'
	ORDER BY s.space;

# This SELECT will not show UNDO or TEMPORARY tablespaces since
# they are only in FILES, not SYS_TABLESPACES.
--echo === information_schema.files ===
--replace_regex  /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/   /#P#/#p#/  /#SP#/#sp#/
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/  $MYSQL_TMP_DIR MYSQL_TMP_DIR  $INNODB_PAGE_SIZE DEFAULT
SELECT 	s.name 'Space_Name',
	f.file_type 'File_Type',
	f.engine 'Engine',
	f.status 'Status',
	f.tablespace_name 'Tablespace_Name',
	f.file_name 'Path'
	FROM information_schema.files f,
	     information_schema.innodb_sys_tablespaces s
	WHERE f.file_id = s.space
	AND s.name NOT LIKE 'mysql/%'
	AND s.name NOT LIKE 'sys/%'
	ORDER BY f.file_id;
--enable_query_log
