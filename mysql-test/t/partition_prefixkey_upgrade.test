--source include/have_case_sensitive_file_system.inc
--source include/not_valgrind.inc

###############################################################################
#
# WL#16054 : Remove support for prefix keys in partition functions
# Checks whether upgrade fails when prefix keys are found in partition function
#
################################################################################
#   Test for upgrade error to be shown in the error log on upgrade
#   from an earlier 8.0.x version.
#
#   To create the file std_data/data80019_partition_prefix_key.zip
#
#   - In 8.0.19, execute:
#
#       CREATE SCHEMA test;
#       USE test;
#
#       CREATE TABLE `t1` (
#        `a` VARCHAR(10000) NOT NULL,
#        `b` VARCHAR(10) NOT NULL,
#        PRIMARY KEY (`a`(100),`b`)
#       ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
#       PARTITION BY KEY () PARTITIONS 3;
#
#   - then zip the data folder
#
#       zip -r data80019_partition_prefix_key.zip db/
#
################################################################################

--echo
--echo # Copy the 8.0.19 data zip file to working directory.
--copy_file $MYSQLTEST_VARDIR/std_data/data80019_partition_prefix_key.zip $MYSQL_TMP_DIR/data80019_partition_prefix_key.zip

--echo
--echo # Check that the zip file exists in the working directory.
--file_exists $MYSQL_TMP_DIR/data80019_partition_prefix_key.zip

--echo
--echo # Unzip 8.0.19 data directory.
--exec unzip -qo $MYSQL_TMP_DIR/data80019_partition_prefix_key.zip -d $MYSQL_TMP_DIR/data80019_partition_prefix_key

--echo
--echo # Set data directory to the 8.0.19 data directory.
--let $MYSQLD_DATADIR1= $MYSQL_TMP_DIR/data80019_partition_prefix_key/db

--echo
--echo # Set log directory.
--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/data80019_partition_prefix_key.log

--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR1 MYSQLD_DATADIR1 $MYSQLD_LOG MYSQLD_LOG

--echo
--echo # Restart server to trigger upgrade.

--error 1
--exec $MYSQLD --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR1 --secure-file-priv=""

--echo
--echo # Check for errors in the error log.
--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= \[ERROR\]
--source include/search_pattern.inc

--echo
--echo # Should show error message in the error log when upgrading
--echo # from 8.4
--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Column 'test\.t1\.a' having prefix key part 'a\(100\)' in the PARTITION BY KEY\(\) clause is not supported\.
--source include/search_pattern.inc

--echo
--echo # Cleanup.
--remove_file $MYSQL_TMP_DIR/data80019_partition_prefix_key.zip
--force-rmdir $MYSQL_TMP_DIR/data80019_partition_prefix_key
