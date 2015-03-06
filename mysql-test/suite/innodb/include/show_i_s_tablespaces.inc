# This script assumes that the caller did the following;
#     LET $MYSQLD_DATADIR = `select @@datadir`;
#     LET $INNODB_PAGE_SIZE = `select @@innodb_page_size`;
--echo === information_schema.innodb_sys_tablespaces and innodb_sys_datafiles ===
--disable_query_log
--replace_result  #P# #p#  #SP# #sp#  ./ MYSQLD_DATADIR/  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/  $MYSQL_TMP_DIR MYSQL_TMP_DIR  $INNODB_PAGE_SIZE DEFAULT
SELECT s.name 'Space Name',
	d.path 'Path',
	s.page_size 'Page Size',
	s.zip_page_size 'Zip Size',
	s.row_format 'Formats Permitted',
	s.space_type 'Space Type'
	FROM information_schema.innodb_sys_tablespaces s,
	     information_schema.innodb_sys_datafiles d
	WHERE s.space = d.space
	AND s.name NOT LIKE 'SYS_%'
	AND s.name NOT LIKE 'mysql\/%'
	ORDER BY s.space;
--enable_query_log
