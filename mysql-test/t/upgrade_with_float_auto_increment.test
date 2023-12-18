--source include/not_valgrind.inc
--source include/have_lowercase0.inc
--source include/big_test.inc

--echo #
--echo # WL#13103: Remove AUTO_INCREMENT on DOUBLE and FLOAT
--echo # 

######################################################################
# 8.0.29 (Just for instance):
#
# mysql> SHOW DATABASES;
# +--------------------+
# | Database           |
# +--------------------+
# | DB1                |
# | information_schema |
# | mysql              |
# | performance_schema |
# | sys                |
# +--------------------+
# 5 rows in set (0.02 sec)
#
# mysql> USE DB1;
# Database changed
#
# mysql> CREATE TABLE t1 (c1 FLOAT AUTO_INCREMENT PRIMARY KEY);
# Query OK, 0 rows affected, 1 warning(0.04 sec)
#
# mysql> CREATE TABLE t2 (c1 DOUBLE AUTO_INCREMENT PRIMARY KEY);
# Query OK, 0 rows affected, 1 warning(0.04 sec)
#
# mysql> show tables;
# +-----------------------+
# | Tables_in_test        |
# +-----------------------+
# | t1                    |
# | t2                    |
# +-----------------------+
# 2 rows in set (0.00 sec)
#
# mysql> SHOW VARIABLES LIKE "lower_case_table_names";
# +------------------------+-------+
# | Variable_name          | Value |
# +------------------------+-------+
# | lower_case_table_names | 0     |
# +------------------------+-------+
# 1 row in set (0.01 sec)
#
# Contents of datadir:
# Database dirname: DB1
# Table filenames: t1.ibd, i.ibd
#
# From build directory
# zip -r ../wl131303_datadir.zip ./data
#
########################################################################

--copy_file $MYSQLTEST_VARDIR/std_data/wl131303_datadir.zip $MYSQL_TMP_DIR/wl131303_datadir.zip
--file_exists $MYSQL_TMP_DIR/wl131303_datadir.zip
--exec unzip -qo $MYSQL_TMP_DIR/wl131303_datadir.zip -d $MYSQL_TMP_DIR
--let $MYSQLD_DATADIR1= $MYSQL_TMP_DIR/data
--let $MYSQLD_LOG= $MYSQLD_DATADIR1/error.log
--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR1 MYSQLD_DATADIR1 $MYSQLD_LOG MYSQLD_LOG

--echo # Stop DB server which was created by MTR default
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

--error 1
--exec $MYSQLD --no-defaults $extra_args --innodb_dedicated_server=OFF --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR1

--echo
--echo # Upgrade should fail with these errors in the log.
--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= AUTO_INCREMENT is not supported with DOUBLE/FLOAT field
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Data Dictionary initialization failed
--source include/search_pattern.inc

--echo
--echo # Cleanup.
--remove_file $MYSQL_TMP_DIR/wl131303_datadir.zip
--force-rmdir $MYSQLD_DATADIR1

--echo
--echo # ------------------------------------------------------------------
--echo # End-of-test cleanup.
--echo # ------------------------------------------------------------------
--echo # Restart the server with default options.
--source include/start_mysqld.inc
