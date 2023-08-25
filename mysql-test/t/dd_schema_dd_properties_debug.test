--echo #
--echo # Extract the contents of mysql.dd_properties and format it in a
--echo # human readable way. Include the file in the result file of
--echo # this test. Filter out version numbers. The results file is
--echo # recorded on a case sensitive file system, which has an impact
--echo # of some of the collations.
--echo #

--source include/have_case_sensitive_file_system.inc

--let $file = $MYSQL_TMP_DIR/dd_properties.txt
--let $filter = MYSQLD_VERSION|DD_VERSION|NDBINFO_VERSION|LCTN|IS_VERSION|PS_VERSION|SDI_VERSION|MINOR_DOWNGRADE_THRESHOLD|MYSQL_VERSION_STABILITY|SERVER_DOWNGRADE_THRESHOLD|SERVER_UPGRADE_THRESHOLD
--source include/dd_schema_dd_properties.inc
--cat_file $file
--remove_file $file
