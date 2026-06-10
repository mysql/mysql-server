--source include/not_valgrind.inc

--echo ########################################################################
--echo # Bug#36890891: Specific table schema causes table to become corrupted
--echo #               during MySQL 8 upgrade
--echo ########################################################################

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade.log

--echo ########################################################################
--echo # Copy and unzip the datadir, and stop the server.
--echo ########################################################################

# The datadir is created by building server version 8.0.20
# and executing the following SQL statements:
#
# USE test;
# CREATE TABLE `t0` (
#   `f1` int DEFAULT NULL
# ) COMMENT='basic table without SQL functions';
#
# CREATE TABLE `t1` (
#   `custom_prefix` varchar(14) NOT NULL,
#   CONSTRAINT `ck_t_custom_prefix` CHECK (regexp_like(cast(`custom_prefix` as char charset binary),_utf8mb4'^[a-z0-9-]+$'))
# ) ENGINE=InnoDB COMMENT='broken CONSTRAINT';
#
# CREATE TABLE `t2` (
#   `a0` tinytext CHARACTER SET swe7 COLLATE swe7_swedish_ci,
#   `a1` tinytext CHARACTER SET swe7 COLLATE swe7_swedish_ci GENERATED ALWAYS AS (regexp_like(cast(`a0` as char charset binary),_utf8mb4'^[a-z0-9-]+$')) VIRTUAL,
#   `b1` float(3,2) NOT NULL,
#   `c1` enum('some','g','f') CHARACTER SET swe7 COLLATE swe7_swedish_ci NOT NULL
# ) ENGINE=InnoDB COMMENT='broken virtual column';
#
# CREATE TABLE `t3` (
#   `custom_prefix` varchar(14) NOT NULL,
#   `f1` int DEFAULT (regexp_like(cast(`custom_prefix` as char charset binary),_utf8mb4'^[a-z0-9-]+$')), `f2` int
# ) ENGINE=InnoDB COMMENT='broken DEFAULT';
#
# CREATE TABLE `t4` (
#   `f1` datetime DEFAULT NULL
# ) ENGINE=InnoDB COMMENT='PARTITION with fun' PARTITION BY HASH (year(`f1`));
#
# CREATE TABLE `t5` (
#   `custom_prefix` varchar(14) NOT NULL,
#   KEY `i1` ((regexp_like(cast(`custom_prefix` as char charset binary),_utf8mb4'^[a-z0-9-]+$')))) ENGINE=InnoDB COMMENT='broken KEY';
#
# CREATE TABLE `t6` (
#   `f1` varchar(14) DEFAULT NULL,
#   CONSTRAINT `ck_t6_f1` CHECK (regexp_like(cast(`f1` as char charset binary),_utf8mb4'^[a-z0-9-]+$'))) COMMENT='broken constraint, must not break DROP TABLE';
#
# CREATE TABLE `t7` (
#   `f1` varchar(14) DEFAULT NULL,
#   CONSTRAINT `ck_t7_f1` CHECK (regexp_like(cast(`f1` as char charset binary),_utf8mb4'^[a-z0-9-]+$'))) COMMENT='broken constraint, must not break DROP DATABASE';
#
# "basic":    contains no SQL functions. should not be inspected.
# "with fun": contains SQL functions. should be inspected, and pass.
# "broken":   contains now-broken SQL functions. should be inspected and fail.
#
# Then, move data/ to data_80017_36890891/, and finally zip the entire
# directory (zip -r data_80017_36890891.zip data_80017_36890891).

--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/data_80017_36890891.zip $MYSQL_TMP_DIR/data_80017_36890891.zip
--file_exists $MYSQL_TMP_DIR/data_80017_36890891.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_80017_36890891.zip -d $MYSQL_TMP_DIR
--let $MYSQLD_DATADIR_UPGRADE = $MYSQL_TMP_DIR/data_80017_36890891

--echo ########################################################################
--echo # Restart the server to trigger upgrade.
--echo ########################################################################
--let $shutdown_server_timeout= 300
--let $wait_counter= 10000
--let $restart_parameters= restart: --log-error=$MYSQLD_LOG --log-error-verbosity=3 --check-table-functions=WARN --lower-case-table-names=1 --datadir=$MYSQLD_DATADIR_UPGRADE
--replace_result $MYSQLD_DATADIR_UPGRADE MYSQLD_DATADIR_UPGRADE $MYSQLD_LOG MYSQLD_LOG
--source include/restart_mysqld.inc

--echo
--echo ########################################################################
--echo # Show which tables we inspected during upgrade, and what we saw.
--echo ########################################################################

--echo
--echo # Show tables we could not open during upgrade (all except t0, t4).
SELECT "errlog>>" AS indent, prio, data
  FROM performance_schema.error_log
 WHERE error_code IN("MY-014078", "MY-014079", "MY-015652")
 ORDER BY logged ASC;

--echo
--echo ########################################################################
--echo # Stop the server and do cleanup.
--echo ########################################################################
--let $shutdown_server_timeout= 300
--source include/shutdown_mysqld.inc
--remove_file $MYSQL_TMP_DIR/data_80017_36890891.zip
--force-rmdir $MYSQL_TMP_DIR/data_80017_36890891
--remove_file $MYSQLD_LOG
--let $restart_parameters= restart:
--source include/start_mysqld.inc
