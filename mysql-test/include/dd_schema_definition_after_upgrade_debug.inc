if (!$VERSION)
{
  --die
}

--echo ########################################################################
--echo # Unzip a $VERSION datadir, start the server against it and prepare a
--echo # file containing the DD table definitions after upgrade.
--echo ########################################################################

--echo ########################################################################
--echo # Copy and unzip the datadir.
--echo ########################################################################
  # The datadir is created by building the appropriate server version and running
  # 'mtr 1st', then moving data/ to data_$VERSION/, and finally zipping the
  # entire data_$VERSION/ directory (zip -r data_$VERSION.zip data_$VERSION).
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/data_$VERSION.zip $MYSQL_TMP_DIR/data_$VERSION.zip
--file_exists $MYSQL_TMP_DIR/data_$VERSION.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_$VERSION.zip -d $MYSQL_TMP_DIR
--let $MYSQLD_DATADIR_UPGRADE = $MYSQL_TMP_DIR/data_$VERSION

--echo ########################################################################
--echo # Restart the server against the unzipped datadir.
--echo ########################################################################
--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade_$VERSION.log
--exec echo "restart:" --datadir=$MYSQLD_DATADIR_UPGRADE --log-error-verbosity=3 --log-error=$MYSQLD_LOG --secure-file-priv="" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc
--source include/mysql_upgrade_cleanup.inc

--echo ########################################################################
--echo # Verify that we can restart after upgrade.
--echo ########################################################################
--source include/shutdown_mysqld.inc
--exec echo "restart:" --datadir=$MYSQLD_DATADIR_UPGRADE --log-error-verbosity=3 --log-error=$MYSQLD_LOG --secure-file-priv="" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--echo ########################################################################
--echo # Target schema ids for an upgrade from $VERSION should hold.
--echo ########################################################################
--source include/dd_schema_assert_ids.inc

--echo ########################################################################
--echo # Assert actual schema table names after upgrade.
--echo ########################################################################
--source include/dd_schema_assert_and_fill_table_names.inc

--echo ########################################################################
--echo # Dump actual DD table definitions after upgrade to file and
--echo # compare to target.
--echo ########################################################################
--let $actual_table_defs = $MYSQL_TMP_DIR/actual_definitions.txt
--let $file = $actual_table_defs
--source include/dd_schema_dump_table_defs_debug.inc
--diff_files $target_table_defs $actual_table_defs

--echo ########################################################################
--echo # Dump actual DD table properties after upgrade to file and
--echo # compare to target.
--echo ########################################################################
--let $actual_dd_props = $MYSQL_TMP_DIR/actual_dd_properties.txt
--let $file = $actual_dd_props
--let $filter = id|root|MYSQLD_VERSION_LO
--source include/dd_schema_dd_properties.inc
--diff_files $target_dd_props $actual_dd_props

--echo ########################################################################
--echo # Remove copied files and stop the server.
--echo ########################################################################
--source include/shutdown_mysqld.inc
--remove_file $MYSQL_TMP_DIR/data_$VERSION.zip
--remove_file $actual_table_defs
--remove_file $actual_dd_props
--force-rmdir $MYSQL_TMP_DIR/data_$VERSION
