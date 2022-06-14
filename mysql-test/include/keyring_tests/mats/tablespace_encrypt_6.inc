#------------------------------------------------------------------------------
# InnoDB transparent tablespace data encryption for general tablespace
#
# This test create tables in encrypted/non-encrypted genral tablespace and
# try to access them after restarting with different combinitions such as
# - restart with same server option (acccess all tables)
# - restart without keyring options(encrypt table not accessible , rest are)
# - restart with keyring option but using new key_file_data
# (old encrypt table not accessible , rest are. New encrypt
# table creation possible)
#
############################################################################

# Waiting time when (re)starting the server
--let $explicit_default_wait_counter=10000

# Suppress warnings
-- disable_query_log
call mtr.add_suppression("Could not find the data corresponding to Data ID:");
call mtr.add_suppression("\\[ERROR\\].*ibd can't be decrypted, please confirm that keyring is loaded");
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] \\[[^]]*\\] Encryption can't find master key, please check the keyring is loaded.");
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] \\[[^]]*\\] Check keyring fail, please check the keyring is loaded.");
-- enable_query_log

--echo #########################################################################
--echo # RESTART 1 : WITH  KEYRING
--echo #########################################################################

--echo #########
--echo # SETUP #
--echo #########

--disable_warnings
CREATE TABLESPACE encrypt_ts ADD DATAFILE 'encrypt_ts.ibd' ENCRYPTION ='Y' ENGINE=InnoDB;
CREATE TABLESPACE encrypt_ts1 ADD DATAFILE 'encrypt_ts1.ibd' ENCRYPTION ='N' ENGINE=InnoDB;
DROP DATABASE IF EXISTS tde_db;
DROP TABLE IF EXISTS tde_db. t_encrypt;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings

DELIMITER |;
CREATE PROCEDURE tde_db.init_setup()
begin
   /*  Create table in encrypted tablespace */
   CREATE TABLE tde_db.t_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts ENCRYPTION='Y' ENGINE = InnoDB;

   /*  Create table in non-encrypted tablesapce */
   CREATE TABLE tde_db.t_non_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts1  ENGINE = InnoDB;

   /*  insert into encrypted table */
   INSERT INTO tde_db.t_encrypt(c3,c4,c7) VALUES(CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
   INSERT INTO tde_db.t_encrypt(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   INSERT INTO tde_db.t_encrypt(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   INSERT INTO tde_db.t_encrypt(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   INSERT INTO tde_db.t_encrypt(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   INSERT INTO tde_db.t_encrypt(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   INSERT INTO tde_db.t_encrypt(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_encrypt */';
   SELECT COUNT(*) FROM tde_db.t_encrypt;
   SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;

   /*  insert into non encrypted table */
   INSERT INTO tde_db.t_non_encrypt(c2,c3,c4,c7) SELECT c2,c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_non_encrypt */';
   SELECT COUNT(*) FROM tde_db.t_non_encrypt;
   SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;

   ALTER INSTANCE ROTATE INNODB MASTER KEY;

   CREATE TABLE tde_db.t_encrypt_2(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts ENCRYPTION='Y' ENGINE = InnoDB;
   CREATE TABLE tde_db.t_non_encrypt_2(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts1 ENGINE = InnoDB;
   /*  insert into encrypted table 2 */
   INSERT INTO tde_db.t_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_encrypt_2 */';
   SELECT COUNT(*) FROM tde_db.t_encrypt_2;
   SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
   /* insert into NON encrypted table 2 */
   INSERT INTO tde_db.t_non_encrypt_2(c2,c3,c4,c7) SELECT c2,c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_non_encrypt_2 */';
   SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
   SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_2 LIMIT 10;
end|
DELIMITER ;|

--echo #-----------------------------------------------------------------------
--echo # Initialize tables
call tde_db.init_setup();
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

# Take backup of key data and replace it with empty file
--source include/keyring_tests/helper/local_keyring_file_backup.inc 
# Then reload the keyring so that it reinitializes itself with empty file
ALTER INSTANCE RELOAD KEYRING;

--echo # Select non encrypted tables : Pass
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # Select encrypted tables : No Error (-master key is cached)
SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;

--echo # Error on "ALTER INSTANCE ..."
--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo # new table creation in encrypted tablespace is allowed after uninstall
CREATE TABLE tde_db.t_encrypt_3(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts ENCRYPTION='Y' ENGINE = InnoDB;
--echo # new table in non-encrypted tablespace is allowed after uninstall
CREATE TABLE tde_db.t_non_encrypt_3(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts1  ENGINE = InnoDB;

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2 , tde_db.t_encrypt_3 ;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 , tde_db.t_non_encrypt_3;

--echo #########################################################################
--echo # Test 1 : Restart with same keyring option
--echo #          - all tables accessible
--echo #########################################################################
# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';
--echo # Initialize tables
call tde_db.init_setup();
--echo # restart with same keyring option
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--echo # All tables must be accessible
SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_2 LIMIT 10;

--echo # Insert into old encrypted tables
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
INSERT INTO tde_db.t_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # Insert into old non encrypted tables
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
INSERT INTO tde_db.t_non_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # Update into old encrypted tables
UPDATE tde_db.t_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # Update into old non encrypted tables
UPDATE tde_db.t_non_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
--echo # Delete from old encrypted tables
DELETE FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # Delete from old non encrypted tables
DELETE FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;

--echo # Create new tables
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts ENCRYPTION='Y' ENGINE = InnoDB;
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts1 ENGINE = InnoDB;


INSERT INTO tde_db.t_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
INSERT INTO tde_db.t_non_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_4 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_4 LIMIT 10;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2 , tde_db.t_encrypt_4;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 , tde_db.t_non_encrypt_4;


--echo #########################################################################
--echo # Test 2 : Restart without  keyring option
--echo #           - old encrypted tables accessible
--echo #           - unencryption tables are accessible
--echo #########################################################################
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';
--echo # Initialize tables
call tde_db.init_setup();
--echo # restart without keyring
--source include/keyring_tests/helper/instance_backup_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--echo # Encrypted tables must not be accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # Unencrypted tables must be accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;

--echo # Creating table in encrypted tablespace must not be possible as tablespace
--echo # would be missing
--error ER_TABLESPACE_MISSING
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts ENCRYPTION='Y' ENGINE = InnoDB;

--echo # Creating table in unencrypted tablespace must be possible
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts1 ENGINE = InnoDB;

INSERT INTO tde_db.t_non_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo # Encrypted tables must not be accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt_2;

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 ,tde_db.t_non_encrypt_4;

--echo #########################################################################
--echo # Test 3 : Restart and reload keyring using command
--echo #          - All tables must be accessible
--echo #########################################################################
--source include/keyring_tests/helper/instance_restore_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

--echo # Initalize tables
call tde_db.init_setup();

--source include/keyring_tests/helper/local_keyring_file_backup.inc 
--echo # restart without keyring 
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;

--echo # All tables must be accessible
SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_2 LIMIT 10;

--echo # Insert into old encrypted tables
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
INSERT INTO tde_db.t_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # Insert into old non encrypted tables
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
INSERT INTO tde_db.t_non_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # Update into old encrypted tables
UPDATE tde_db.t_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # Update into old non encrypted tables
UPDATE tde_db.t_non_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
--echo # Delete from old encrypted tables
DELETE FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # Delete from old non encrypted tables
DELETE FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;

--echo # Create new tables
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts ENCRYPTION='Y' ENGINE = InnoDB;
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts1 ENGINE = InnoDB;


INSERT INTO tde_db.t_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
INSERT INTO tde_db.t_non_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_4 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_4 LIMIT 10;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2 , tde_db.t_encrypt_4;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 , tde_db.t_non_encrypt_4;

--source include/keyring_tests/helper/local_keyring_file_backup.inc 
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;

#Once keyring is uninstalled, we should still be able to use existing
#tables in encrypted tablespaces as their keys are cached. That's why, even after
#keyring is uninstalled, we can do DMLs on encrypted tables and create
#tables in encrypted tablespaces.

CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts ENCRYPTION='Y' ENGINE = InnoDB;

--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER INSTANCE ROTATE INNODB MASTER KEY;

DROP TABLE tde_db.t_encrypt_4;

# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;

--echo #########################################################################
--echo # Test 4 : Restart with new keyring_data_file
--echo #            - Old encrypted tables not accessible
--echo #            - Non encrypted tables accessible
--echo #            - Creation of new tables in encrypted/unencrypted tablespace
--echo #              is also posible
--echo #########################################################################

let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--echo # init tables
call tde_db.init_setup();

# Take backup of key data and replace it with empty file
--source include/keyring_tests/helper/local_keyring_file_backup.inc 
--echo # restart with with different keyring file
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--echo # old encrypted tables not accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # NON encrypted tables accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;

--echo # new encrypted tablespace possible
CREATE TABLESPACE encrypt_ts2 ADD DATAFILE 'encrypt_ts2.ibd' ENCRYPTION ='Y' ENGINE=InnoDB;

--echo # new table in new encrypted tablespace possible
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE encrypt_ts2 ENCRYPTION='Y' ENGINE = InnoDB;

--echo # new table in non encrypted tablespace possible
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) TABLESPACE=encrypt_ts1 ENGINE = InnoDB;

INSERT INTO tde_db.t_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
INSERT INTO tde_db.t_non_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo # Old encrypted tables in encrypted tablespace must not be accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # Old unencrypted table must be accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # New encrypted tables must be accessible
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
--echo # New unencrypted tables must be accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

--echo ###########
--echo # CLEANUP #
--echo ###########

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2 ,tde_db.t_encrypt_4;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 ,tde_db.t_non_encrypt_4;
DROP DATABASE tde_db;
DROP TABLESPACE encrypt_ts;
DROP TABLESPACE encrypt_ts1;
DROP TABLESPACE encrypt_ts2;

# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
