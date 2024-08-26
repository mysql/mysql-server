--source include/have_case_sensitive_file_system.inc

--echo # Stop DB server which was created by MTR default
--source include/shutdown_mysqld.inc

--echo # ------------------------------------------------------------------
--echo # Check upgrade from 8.0 when events/triggers/views/routines contain GROUP BY DESC.
--echo # ------------------------------------------------------------------

--echo # Set different path for --datadir
let $MYSQLD_DATADIR1 = $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_cs;

--echo # DB server which was started above is not running, no need for shutdown

--echo # Copy the remote tablespace & DB zip files from suite location to working location.
--copy_file $MYSQLTEST_VARDIR/std_data/data80011_upgrade_groupby_desc_cs.zip $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_cs.zip

--echo # Check that the file exists in the working folder.
--file_exists $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_cs.zip

--echo # Unzip the zip file.
--exec unzip -qo $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_cs.zip -d $MYSQL_TMP_DIR

--echo #
--echo # Upgrade tests for WL#8693
--echo #

--echo # Starting the DB server will fail since the data dir contains
--echo # events/triggers/views/routines contain GROUP BY DESC
let MYSQLD_LOG= $MYSQL_TMP_DIR/server.log;
--error 1
--exec $MYSQLD --no-defaults $extra_args --innodb_dedicated_server=OFF --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR1

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Trigger 'trigger_groupby_desc' has an error in its body
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Error in parsing Event 'test'.'event_groupby_desc' during upgrade
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Error in parsing Routine 'test'.'procedure_groupby_desc' during upgrade
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Error in parsing View 'test'.'view_groupby_desc' during upgrade
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Data Dictionary initialization failed.
--source include/search_pattern.inc

--echo # Remove copied files
--remove_file $MYSQLD_LOG
--remove_file $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_cs.zip
--force-rmdir $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_cs

--echo ########################################################################
--echo #
--echo # Bug#28204431: mysql upgrade from "version < 8.0.13" to "version >= 8.0.13"
--echo #
--echo # Verify that the server rejects upgrade if there are partitioned
--echo # InnoDB tables using shared tablespaces.
--echo #
--echo # Unzip a 8.0.12 datadir containing partitioned InnoDB tables using shared
--echo # tablespaces, start the server against it, verify that the server errs out
--echo # during upgrade.
--echo #
--echo ########################################################################

--echo ########################################################################
--echo # Copy and unzip the datadir.
--echo ########################################################################

# The datadir is created by buildling the appropriate server version and
# running the following SQL statements:
#
# CREATE TABLESPACE s1 ADD DATAFILE 's1.ibd';
# CREATE TABLESPACE s2 ADD DATAFILE 's2.ibd';
#
# CREATE TABLE t0 (i int PRIMARY KEY)
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1, PARTITION p2, PARTITION p3);
#
# CREATE TABLE t1 (i int PRIMARY KEY) TABLESPACE innodb_file_per_table
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1, PARTITION p2, PARTITION p3);
#
# CREATE TABLE t2 (i int PRIMARY KEY) TABLESPACE innodb_system
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1, PARTITION p2, PARTITION p3);
#
# CREATE TABLE t3 (i int PRIMARY KEY) TABLESPACE s1
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1, PARTITION p2, PARTITION p3);
#
# CREATE TABLE t4 (i int PRIMARY KEY)
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1,
#    PARTITION p2 TABLESPACE innodb_system,
#    PARTITION p3 TABLESPACE s1);
#
# CREATE TABLE t5 (i int PRIMARY KEY)
#   TABLESPACE s1
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1,
#    PARTITION p2 TABLESPACE innodb_file_per_table,
#    PARTITION p3 TABLESPACE innodb_system);
#
# CREATE TABLE t6 (i int PRIMARY KEY)
#   TABLESPACE s1
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1 TABLESPACE innodb_file_per_table,
#    PARTITION p2 TABLESPACE innodb_file_per_table,
#    PARTITION p3 TABLESPACE innodb_system);
#
# CREATE TABLE t7 (i int PRIMARY KEY)
#   TABLESPACE s1
#   PARTITION BY KEY(i) PARTITIONS 3
#   (PARTITION p1 TABLESPACE s2,
#    PARTITION p2 TABLESPACE innodb_file_per_table,
#    PARTITION p3 TABLESPACE innodb_system);
#
# CREATE TABLE t8 (i int PRIMARY KEY)
#   TABLESPACE s1;
#
# then stopping the server, moving the data directory to 'data_80012_part', and zipping the
# entire data_80012_part directory (zip -r data_80012_part.zip data_80012).

--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/data_80012_part.zip $MYSQL_TMP_DIR/data_80012_part.zip
--file_exists $MYSQL_TMP_DIR/data_80012_part.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_80012_part.zip -d $MYSQL_TMP_DIR

--echo ########################################################################
--echo # Starting the DB server will fail since the data dir contains
--echo # tables with non native partitioning.
--echo ########################################################################
let $MYSQLD_DATADIR_80012_PART = $MYSQL_TMP_DIR/data_80012_part;
let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade_800012_part.log;
--error 1
--exec $MYSQLD --no-defaults --innodb_dedicated_server=OFF --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR_80012_PART

--echo ########################################################################
--echo # Look for errors.
--echo ########################################################################
let SEARCH_FILE= $MYSQLD_LOG;
--let SEARCH_PATTERN= Partitioned table 't2' is not allowed to use shared tablespace 'innodb_system'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't3' is not allowed to use shared tablespace 's1'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't4' is not allowed to use shared tablespace 'innodb_system'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't4' is not allowed to use shared tablespace 's1'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't5' is not allowed to use shared tablespace 'innodb_system'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't5' is not allowed to use shared tablespace 's1'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't6' is not allowed to use shared tablespace 'innodb_system'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't6' is not allowed to use shared tablespace 's1'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't7' is not allowed to use shared tablespace 'innodb_system'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't7' is not allowed to use shared tablespace 's2'.
--source include/search_pattern.inc
--let SEARCH_PATTERN= Partitioned table 't7' is not allowed to use shared tablespace 's1'.
--source include/search_pattern.inc

--echo ########################################################################
--echo # Remove copied files.
--echo ########################################################################
--remove_file $MYSQL_TMP_DIR/data_80012_part.zip
--force-rmdir $MYSQL_TMP_DIR/data_80012_part

--echo ########################################################################
--echo # Cleanup: Restart with default options.
--echo ########################################################################
let $restart_parameters =;
--source include/start_mysqld.inc
