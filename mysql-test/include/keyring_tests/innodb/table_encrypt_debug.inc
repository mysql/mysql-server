#------------------------------------------------------------------------------
# InnoDB transparent tablespace data encryption
# This test case will test basic recovery from known DBUG_SUICIDE point .
#  - ib_crash_during_rotation_for_encryption (assert during key rotation)
#  - ib_crash_during_create_for_encryption (assert during create)
#  - ib_crash_during_decrypt_page (assert during page decryption)
#------------------------------------------------------------------------------







--disable_query_log
call mtr.add_suppression("Error generating data for Data ID:");
--enable_query_log
let SEARCH_FILE = $MYSQLTEST_VARDIR/log/mysqld.1.err;
let $MYSQLD_DATADIR = `SELECT @@datadir`;
let $innodb_file_per_table = `SELECT @@innodb_file_per_table`;

SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;

--disable_warnings
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings

--echo #
--echo # Set encryption ON for table `mysql`
--echo #
ALTER TABLESPACE mysql ENCRYPTION='Y';
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';

--echo #
--echo # Test crash point `ib_crash_during_rotation_for_encryption`
--echo #
SET SESSION DEBUG="+d,ib_encryption_rotate_crash";

CREATE TABLE tde_db.t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';
SHOW CREATE TABLE tde_db.t1;
INSERT INTO tde_db.t1 VALUES(0, "aaaaa");
INSERT INTO tde_db.t1 VALUES(1, "bbbbb");
INSERT INTO tde_db.t1 VALUES(2, "ccccc");
INSERT INTO tde_db.t1 VALUES(3, "ddddd");

--echo # Execute the statement that causes the crash
# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart: $PLUGIN_DIR_OPT" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--error 2013
ALTER INSTANCE ROTATE INNODB MASTER KEY;
--source include/wait_until_connected_again.inc
SET SESSION DEBUG="-d,ib_encryption_rotate_crash";
let SEARCH_PATTERN = ib_encryption_rotate_crash;
--source include/search_pattern.inc

--echo # Show that encryption is OK
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';
INSERT INTO tde_db.t1 VALUES(4, "eeeee");
SELECT * FROM tde_db.t1 ORDER BY c1;

--echo # Create a second table with encryption
CREATE TABLE tde_db.t2(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
SHOW CREATE TABLE tde_db.t2;
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';
INSERT INTO tde_db.t2 VALUES(0, "aaaaa");
INSERT INTO tde_db.t2 VALUES(1, "bbbbb");

--echo # Remove the current auto.cnf file to make sure a new server uuid is
--echo # generated at restart.
--remove_file $MYSQLD_DATADIR/auto.cnf

--echo # Execute the statement that causes the crash
# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart: $PLUGIN_DIR_OPT" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION DEBUG="+d,ib_encryption_rotate_crash";
--error 2013
ALTER INSTANCE ROTATE INNODB MASTER KEY;
--source include/wait_until_connected_again.inc
SET SESSION DEBUG="-d,ib_encryption_rotate_crash";
let SEARCH_PATTERN = ib_encryption_rotate_crash;
--source include/search_pattern.inc

--echo # Show that encryption is OK
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';
INSERT INTO tde_db.t1 VALUES(5, "fffff");
SELECT * FROM tde_db.t1 ORDER BY c1;
SELECT * FROM tde_db.t2 ORDER BY c1;

--echo # Rotate Encryption again without the Crash
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT * FROM tde_db.t1 ORDER BY c1 ;

--echo # Show that encryption is OK
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';
SELECT * FROM tde_db.t1 ORDER BY c1 ;
SELECT * FROM tde_db.t2 ORDER BY c1 ;

DROP TABLE tde_db.t2;
ALTER TABLESPACE mysql ENCRYPTION='N';
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';

--echo #
--echo # Test crash point `ib_crash_during_create_for_encryption` during a CREATE TABLE
--echo #
# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart: $PLUGIN_DIR_OPT" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
SET SESSION DEBUG="+d,ib_crash_during_create_for_encryption";

--echo # Execute the statement that causes the crash
--error 2013
CREATE TABLE tde_db.t2(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;
--source include/wait_until_connected_again.inc
SET SESSION DEBUG="-d,ib_crash_during_create_for_encryption";

--echo # Show encrypted tables.
SET debug='+d,skip_dd_table_access_check';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE OPTIONS LIKE '%encryption=Y%';
SET debug='-d,skip_dd_table_access_check';
SELECT * FROM tde_db.t1 ORDER BY c1 LIMIT 5;
INSERT INTO tde_db.t1 VALUES(6, "ggggg");
SELECT * FROM tde_db.t1 ORDER BY c1 ;
--error 1146
SELECT * FROM tde_db.t2;

--echo #
--echo # Test crash point `ib_crash_during_decrypt_page` during an IMPORT TABLESPACE
--echo #
# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart: $PLUGIN_DIR_OPT" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

FLUSH TABLE tde_db.t1 FOR EXPORT;
--copy_file $MYSQLD_DATADIR/tde_db/t1.cfg $MYSQLD_DATADIR/t1.cfg_back
--copy_file $MYSQLD_DATADIR/tde_db/t1.cfp $MYSQLD_DATADIR/t1.cfp_back
--copy_file $MYSQLD_DATADIR/tde_db/t1.ibd $MYSQLD_DATADIR/t1.ibd_back
UNLOCK TABLES;
ALTER TABLE tde_db.t1 DISCARD TABLESPACE;
--copy_file $MYSQLD_DATADIR/t1.cfg_back $MYSQLD_DATADIR/tde_db/t1.cfg
--copy_file $MYSQLD_DATADIR/t1.cfp_back $MYSQLD_DATADIR/tde_db/t1.cfp
--copy_file $MYSQLD_DATADIR/t1.ibd_back $MYSQLD_DATADIR/tde_db/t1.ibd

SET SESSION DEBUG="+d,ib_crash_during_decrypt_page";

--echo # Execute the statement that causes the crash
--error 2013
eval ALTER TABLE tde_db.t1 IMPORT TABLESPACE;
--source include/wait_until_connected_again.inc
SET SESSION DEBUG="-d,ib_crash_during_decrypt_page";

--error ER_TABLESPACE_DISCARDED
INSERT INTO tde_db.t1 VALUES(7, "hhhhh");
--error ER_TABLESPACE_DISCARDED
SELECT * FROM tde_db.t1 ORDER BY c1 ;
DROP TABLE tde_db.t1;

--exec echo "restart: $PLUGIN_DIR_OPT" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--remove_files_wildcard $MYSQLD_DATADIR/tde_db/ t1.*
DROP DATABASE tde_db;

--echo #
--echo # Bug #27307740 [ERROR] [MY-011066] INNODB: CORRUPT LOG RECORD FOUND
--echo # DURING CRASH RECOVERY
--echo #

CREATE DATABASE tde_db;

# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart: $PLUGIN_DIR_OPT" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

CREATE TABLE tde_db.test_tbl(c1 int) ENCRYPTION="Y" ENGINE=InnoDB;

# Crash the server while executing ALTER INSTANCE ROTATE
SET SESSION DEBUG="+d, keyring_generate_fail";
--error 2013
ALTER INSTANCE ROTATE INNODB MASTER KEY;
--source include/wait_until_connected_again.inc
SET SESSION debug="-d, keyring_generate_fail";
SELECT COUNT(*) FROM tde_db.test_tbl;

DROP TABLE tde_db.test_tbl;

# Cleanup
DROP DATABASE tde_db;
eval SET GLOBAL innodb_file_per_table=$innodb_file_per_table;

--disable_query_log
call mtr.add_suppression("\\[Warning\\].* Decrypting a page in doublewrite file failed");
call mtr.add_suppression("\\[Warning\\].* Encryption key missing:");
call mtr.add_suppression("\\[Warning\\].* Cannot calculate statistics for table .* because the \.ibd file is missing");
call mtr.add_suppression("\\[Warning\\].* Database page corruption or a failed file read of page");
call mtr.add_suppression("\\[Warning\\].* does not exist in the InnoDB internal data dictionary though MySQL is trying to drop it");
call mtr.add_suppression("\\[Warning\\].* Tablespace .*, name '.*', file '.*' is missing");
call mtr.add_suppression("\\[Warning\\].* Tablespace for table `tde_db`.`t1` is set as discarded");
call mtr.add_suppression("\\[ERROR\\].* Encryption algorithm support missing:");
call mtr.add_suppression("\\[ERROR\\].* The table .* doesn't have a corresponding tablespace, it was discarded.");
--enable_query_log
