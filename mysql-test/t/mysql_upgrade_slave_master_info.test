 # === Purpose ===
 #
 # This test case will verify that the mysql_upgrade script
 # corrects the order of the columns Channel_name and Tls_version
 # in the table mysql.slave_master_info if their order was wrong.
 # In case of mysql_upgrade happening from a release where the above
 # two columns are not present in the table, the test case certifies
 # that the upgrade script adds them in the correct order.
 #
 # ==== Related Bugs and Worklogs ====
 #
 # Bug #24384561: 5.7.14 COMPLAINS ABOUT WRONG
 #                SLAVE_MASTER_INFO AFTER UPGRADE FROM 5.7.13
 #
--source include/big_test.inc
--source include/not_valgrind.inc
--source include/mysql_upgrade_preparation.inc

USE test;

# Preserve the original state of the table so that it can be restored at the end of the test.
ALTER TABLE mysql.slave_master_info TABLESPACE innodb_file_per_table;
CREATE TABLE test.slave_master_info_backup LIKE mysql.slave_master_info;
ALTER TABLE mysql.slave_master_info TABLESPACE mysql;
INSERT INTO test.slave_master_info_backup SELECT * FROM mysql.slave_master_info;

CREATE TABLE test.original
SELECT COLUMN_NAME, ORDINAL_POSITION
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_NAME = "slave_master_info"
  AND TABLE_SCHEMA = "mysql";

--echo # Scenario 1:
--echo # Verify that the upgrade script works correctly when upgrading from the same version
--echo # i.e. when both the columns Channel_name and Tls_version are in the correct order.
--disable_result_log
--let $restart_parameters = restart:--upgrade=FORCE
--let $wait_counter= 10000
--source include/restart_mysqld.inc
--enable_result_log

CREATE TABLE test.upgraded
SELECT COLUMN_NAME, ORDINAL_POSITION
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_NAME = "slave_master_info"
  AND TABLE_SCHEMA = "mysql";
--let $diff_tables= test.upgraded, test.original
--source include/diff_tables.inc
DROP TABLE test.upgraded;

--echo # Scenario 2:
--echo # Verify that the upgrade script corrects the order of columns Channel_name
--echo # and Tls_version in mysql.slave_master_info if the order is found to be wrong.

ALTER TABLE mysql.slave_master_info
  MODIFY COLUMN Channel_name char(64) NOT NULL COMMENT
  'The channel on which the slave is connected to a source. Used in Multisource Replication'
  AFTER Tls_version;

--echo # Running mysql_upgrade to update slave_master_info table
--disable_result_log
--let $restart_parameters = restart:--upgrade=FORCE
--let $wait_counter= 10000
--source include/restart_mysqld.inc
--enable_result_log

--echo # Verify that the columns Channel_name and Tls_version are now in correct order.
CREATE TABLE test.upgraded
SELECT COLUMN_NAME, ORDINAL_POSITION
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_NAME = "slave_master_info"
  AND TABLE_SCHEMA = "mysql";
--let $diff_tables= test.upgraded, test.original
--source include/diff_tables.inc
DROP TABLE test.upgraded;

--echo # Scenario 3:
--echo # DROP slave_master_info table and re-create it as of MySQL 5.6.21

DROP table mysql.slave_master_info;

CREATE TABLE `mysql`.`slave_master_info` (
  `Number_of_lines` int(10) unsigned NOT NULL COMMENT 'Number of lines in the file.',
  `Master_log_name` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin NOT NULL COMMENT 'The name of the master binary log currently being read from the master.',
  `Master_log_pos` bigint(20) unsigned NOT NULL COMMENT 'The master log position of the last read event.',
  `Host` varchar(64) CHARACTER SET utf8mb3 COLLATE utf8mb3_bin NOT NULL DEFAULT '' COMMENT 'The host name of the source.',
  `User_name` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The user name used to connect to the master.',
  `User_password` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The password used to connect to the master.',
  `Port` int(10) unsigned NOT NULL COMMENT 'The network port used to connect to the master.',
  `Connect_retry` int(10) unsigned NOT NULL COMMENT 'The period (in seconds) that the slave will wait before trying to reconnect to the master.',
  `Enabled_ssl` tinyint(1) NOT NULL COMMENT 'Indicates whether the server supports SSL connections.',
  `Ssl_ca` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The file used for the Certificate Authority (CA) certificate.',
  `Ssl_capath` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The path to the Certificate Authority (CA) certificates.',
  `Ssl_cert` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The name of the SSL certificate file.',
  `Ssl_cipher` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The name of the cipher in use for the SSL connection.',
  `Ssl_key` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The name of the SSL key file.',
  `Ssl_verify_server_cert` tinyint(1) NOT NULL COMMENT 'Whether to verify the server certificate.',
  `Heartbeat` float NOT NULL,
  `Bind` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'Displays which interface is employed when connecting to the MySQL server',
  `Ignored_server_ids` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The number of server IDs to be ignored, followed by the actual server IDs',
  `Uuid` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The master server uuid.',
  `Retry_count` bigint(20) unsigned NOT NULL COMMENT 'Number of reconnect attempts, to the master, before giving up.',
  `Ssl_crl` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The file used for the Certificate Revocation List (CRL)',
  `Ssl_crlpath` text CHARACTER SET utf8mb3 COLLATE utf8mb3_bin COMMENT 'The path used for Certificate Revocation List (CRL) files',
  `Enabled_auto_position` tinyint(1) NOT NULL COMMENT 'Indicates whether GTIDs will be used to retrieve events from the master.',
  PRIMARY KEY (`Host`,`Port`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 STATS_PERSISTENT=0 COMMENT='Master Information';

--echo #Running mysql_upgrade to update slave_master_info table
--disable_result_log
--let $restart_parameters = restart:--upgrade=FORCE
--let $wait_counter= 10000
--source include/restart_mysqld.inc
--enable_result_log

--echo #Verify that the columns Channel_name and Tls_version are added and are in correct order.
CREATE TABLE test.upgraded
SELECT COLUMN_NAME, ORDINAL_POSITION
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_NAME = "slave_master_info"
  AND TABLE_SCHEMA = "mysql";
--let $diff_tables= test.upgraded, test.original
--source include/diff_tables.inc
DROP TABLE test.upgraded;

# Cleanup:
TRUNCATE TABLE mysql.slave_master_info;
INSERT INTO mysql.slave_master_info SELECT * FROM test.slave_master_info_backup;

ALTER TABLE mysql.slave_master_info
  MODIFY Host VARCHAR(255) CHARACTER SET ASCII NULL COMMENT 'The host name of the source.',
  ALTER COLUMN Channel_name DROP DEFAULT;

DROP TABLE test.slave_master_info_backup;
DROP TABLE test.original;

--echo #Restart the server
--let $restart_parameters = restart
--source include/restart_mysqld.inc
--source include/mysql_upgrade_cleanup.inc

