--echo ########################################################################
--echo # This test scripts covers meta data related aspects of upgrade
--echo # after 8.0. For upgrade from 5.7 to 8.0, see dd_upgrade_test.
--echo ########################################################################

--source include/have_case_insensitive_file_system.inc
--source include/not_valgrind.inc
--source include/shutdown_mysqld.inc

--echo #
--echo # Bug#29917793: REJECT UPGRADE 8.0.14-16 -> 8.0.17 FOR LCTN=1 AND PARTITIONED TABLES
--echo #
--echo # This is the test case for case insensitive file systems. A corresponding test case
--echo # for case sensitive file systems can be found in dd_upgrade_cs.test.
--echo #

--echo # Test with lctn=1 and no partitioned tables. Upgrade should succeed.
--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade_0.log
--let $name_base = data_80016_lctn1_lin_nopart
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/$name_base.zip $MYSQL_TMP_DIR/$name_base.zip
--file_exists $MYSQL_TMP_DIR/$name_base.zip
--exec unzip -qo $MYSQL_TMP_DIR/$name_base.zip -d $MYSQL_TMP_DIR

--echo # Set different path for --datadir and check that it exists.
--let $MYSQLD_DATADIR = $MYSQL_TMP_DIR/$name_base
--file_exists $MYSQLD_DATADIR

--echo # Upgrade the server.
--let $wait_counter= 3000
--let $restart_parameters= restart: --datadir=$MYSQLD_DATADIR --log-error=$MYSQLD_LOG --lower_case_table_names=1
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR $MYSQLD_LOG MYSQLD_LOG
--source include/start_mysqld.inc

--echo # Stop the server and do cleanup.
--source include/shutdown_mysqld.inc
--remove_file $MYSQL_TMP_DIR/$name_base.zip
--force-rmdir $MYSQLD_DATADIR

--echo # Test with lctn=1 and partitioned tables. Upgrade should succeed since we are on a
--echo # case insensitive file system.
--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade_1.log
--let $name_base = data_80016_lctn1_lin_part
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/$name_base.zip $MYSQL_TMP_DIR/$name_base.zip
--file_exists $MYSQL_TMP_DIR/$name_base.zip
--exec unzip -qo $MYSQL_TMP_DIR/$name_base.zip -d $MYSQL_TMP_DIR

--echo # Set different path for --datadir and check that it exists.
--let $MYSQLD_DATADIR = $MYSQL_TMP_DIR/$name_base
--file_exists $MYSQLD_DATADIR

--echo # Upgrade the server.
--let $wait_counter= 3000
--let $restart_parameters= restart: --datadir=$MYSQLD_DATADIR --log-error=$MYSQLD_LOG --lower_case_table_names=1
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR $MYSQLD_LOG MYSQLD_LOG
--source include/start_mysqld.inc

--echo # Table rebuild will succeed.
ALTER TABLE test.t ENGINE = InnoDB;

--echo # Stop the server and do cleanup.
--source include/shutdown_mysqld.inc
--remove_file $MYSQL_TMP_DIR/$name_base.zip
--force-rmdir $MYSQLD_DATADIR


--echo # Restart the server with default options.
--let $restart_parameters= restart:
--source include/start_mysqld.inc
