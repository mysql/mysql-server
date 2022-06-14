# This script assumes that the caller did the following;
#     LET $MYSQLD_DATADIR = `select @@datadir`;
#     LET $INNODB_PAGE_SIZE = `select @@innodb_page_size`;
--echo === information_schema.innodb_tablespaces and innodb_datafiles ===
--disable_query_log
--replace_regex  /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/ /#P#/#p#/  /#SP#/#sp#/
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/  $MYSQL_TMP_DIR MYSQL_TMP_DIR  $INNODB_PAGE_SIZE DEFAULT
SELECT s.name 'Space_Name',
	s.space_type 'Space_Type',
	s.page_size 'Page_Size',
	s.zip_page_size 'Zip_Size',
	s.fs_block_size!=0 'BlockSize!=0',
	s.file_size!=0 'FileSize!=0',
	s.row_format 'Formats_Permitted',
	d.path 'Path'
	FROM information_schema.innodb_tablespaces s,
	     information_schema.innodb_datafiles d
	WHERE s.space = d.space
	AND d.path NOT LIKE '%/sys/%'
	AND d.path NOT LIKE '%\\\sys\\\%'
	AND s.name NOT LIKE 'mysql/%'
	AND s.name NOT LIKE 'sys/%'
	AND s.name NOT LIKE '%undo%'
	ORDER BY s.name, d.path;

# This SELECT will not show UNDO or TEMPORARY tablespaces
--echo === information_schema.files ===
LET $REGEX = /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/   /#P#/#p#/  /#SP#/#sp#/   $EXTRA_I_S_FILES_REGEX;
--replace_regex  $REGEX
--replace_result ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/  $MYSQL_TMP_DIR MYSQL_TMP_DIR  $INNODB_PAGE_SIZE DEFAULT
# Hypergraph optimizer has different warnings, so disable them temporarily.
--disable_warnings
SELECT	s.name 'Space_Name',
	f.file_type 'File_Type',
	f.engine 'Engine',
	f.status 'Status',
	f.tablespace_name 'Tablespace_Name',
	f.file_name 'Path'
	FROM information_schema.files f,
	     information_schema.innodb_tablespaces s
	WHERE f.file_id = s.space
	AND f.file_name NOT LIKE '%/sys/%'
	AND f.file_name NOT LIKE '%\\\sys\\\%'
	AND s.name NOT LIKE 'mysql'
	AND s.name NOT LIKE 'sys/%'
	AND s.name NOT LIKE 'innodb_temporary'
	AND s.name NOT LIKE '%undo%'
	ORDER BY s.name, f.file_name;
--enable_warnings
--enable_query_log
