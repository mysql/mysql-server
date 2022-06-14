#------------------------------------------------------------------------------
# InnoDB transparent tablespace data encryption
#
# This test create encrypt , non encrypt tables and try to access them after
# restarting with different combinitions such as
# - restart with same server option (acccess all tables)
# - restart without keyring options(encrypt table not accessible , rest are)
# - restart without keyring option but explicilty load (access all)
# - restart with keyring option but using new key_file_data
# (old encrypt table not accessible , rest are. New encrypt
# table creation possible)
#
#------------------------------------------------------------------------------
# InnoDB transparent tablespace data encryption

# Suppress warnings
--disable_query_log
call mtr.add_suppression("Could not find the data corresponding to Data ID:");
call mtr.add_suppression("Error generating data for Data ID:");
call mtr.add_suppression("Can't generate new master key for tablespace encryption, please check the keyring is loaded.");
call mtr.add_suppression("Operating system error number");
call mtr.add_suppression("The error means the system cannot find the path specified");
call mtr.add_suppression("If you are installing InnoDB, remember that you must create directories yourself, InnoDB does not create them");
call mtr.add_suppression("Could not find a valid tablespace file");
call mtr.add_suppression("Ignoring tablespace");
call mtr.add_suppression("Failed to find tablespace for table");
call mtr.add_suppression("Error while loading keyring content. The keyring might be malformed");
call mtr.add_suppression("\\[Warning\\] .* Please refer to .* for how to resolve the issue");
call mtr.add_suppression("\\[ERROR\\] .* Encryption can't find master key, please check the keyring is loaded");
call mtr.add_suppression("\\[ERROR\\] .* Encryption information in datafile: .* can't be decrypted, please confirm that keyring is loaded");
call mtr.add_suppression("\\[ERROR\\] .* Table tde_db/t_encrypt.* in the InnoDB data dictionary has tablespace id .*, but tablespace with that id or name does not exist");
call mtr.add_suppression("\\[ERROR\\] .* Tablespace [0-9]+, name 'tde_db.*t_encrypt.*', unable to open file '.*tde_db.*t_encrypt.*.ibd' - Data structure corruption");
call mtr.add_suppression("\\[ERROR\\] .* Check keyring fail, please check the keyring is loaded.");
--enable_query_log

let $innodb_file_per_table = `SELECT @@innodb_file_per_table`;

#------------------------------------------------------------------------------
# Initial setup for test which create encrypt and non encrypt tables
--disable_warnings
DROP DATABASE IF EXISTS tde_db;
DROP TABLE IF EXISTS tde_db. t_encrypt;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings
SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;

DELIMITER |;
CREATE PROCEDURE tde_db.init_setup()
begin
   /*  Create encrypt table with encryption */
   CREATE TABLE tde_db.t_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) ENCRYPTION="Y"  ENGINE = InnoDB;

   /*  Create table without encryption */
   CREATE TABLE tde_db.t_non_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;

   /*  insert into encrypt table */
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

   /*  insert into non encrypt table */
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
                       )  ENCRYPTION="Y" ENGINE = InnoDB;
   CREATE TABLE tde_db.t_non_encrypt_2(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;
   /*  insert into encrypt table 2 */
   INSERT INTO tde_db.t_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_encrypt_2 */';
   SELECT COUNT(*) FROM tde_db.t_encrypt_2;
   SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
   /* insert into NON encrypt table 2 */
   INSERT INTO tde_db.t_non_encrypt_2(c2,c3,c4,c7) SELECT c2,c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_non_encrypt_2 */';
   SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
   SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_2 LIMIT 10;
end|
DELIMITER ;|

--echo #-----------------------------------------------------------------------
--echo # init tables
call tde_db.init_setup();
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

# Take backup of key data and replace it with empty file
--source include/keyring_tests/helper/local_keyring_file_backup.inc 
# Then reload the keyring so that it reinitializes itself with empty file
ALTER INSTANCE RELOAD KEYRING;

--echo # Select non encrypt table : Pass
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # Select encrypt table : No Error (master key is cached)
SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;

--echo # Error on "ALTER INSTANCE ..."
--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo # new encrypt table creation is blocked after uninstall
--error ER_CANNOT_FIND_KEY_IN_KEYRING
CREATE TABLE tde_db.t_encrypt_3(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENCRYPTION="Y" ENGINE = InnoDB;
--echo # new non encrypt table
CREATE TABLE tde_db.t_non_encrypt_3(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2 ;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 , tde_db.t_non_encrypt_3;

# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;

--echo #-----------------------------------------------------------------------
--echo # Test 1 : Restart with same keyring option , all tables accesible
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';
--echo # init tables
call tde_db.init_setup();
--echo # restart with same keyring option
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
# All tables accessible
SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_2 LIMIT 10;

--echo # insert into old encrypt tables
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
INSERT INTO tde_db.t_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # insert into old non encrypt tables
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
INSERT INTO tde_db.t_non_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # update into old encrypt tables
UPDATE tde_db.t_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # update into old non encrypt tables
UPDATE tde_db.t_non_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
--echo # delete into old encrypt tables
DELETE FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # delete into old non encrypt tables
DELETE FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;

--echo # new table
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENCRYPTION="Y" ENGINE = InnoDB;
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;


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


--echo #-----------------------------------------------------------------------
--echo # Test 2 : Restart without keyring option - old encrypt table not
--echo # accessible but rest are.
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';
--echo # init tables
call tde_db.init_setup();


--source include/keyring_tests/helper/instance_backup_manifest.inc
--echo # restart without keyring 
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--echo # encrypt table not accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # NON encrypt table are accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;

--echo # encrypt table not possible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENCRYPTION="Y" ENGINE = InnoDB;

--echo # non encrypt table possible
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;

INSERT INTO tde_db.t_non_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo # encrypt table not accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt_2;

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 ,tde_db.t_non_encrypt_4;

DROP DATABASE tde_db;



#------------------------------------------------------------------------------
--source include/keyring_tests/helper/instance_restore_manifest.inc
--echo # Initial setup
--echo # Starting server with keyring
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--disable_warnings
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings
SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;

DELIMITER |;
CREATE PROCEDURE tde_db.init_setup()
begin
   /*  Create encrypt table with encryption */
   CREATE TABLE tde_db.t_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) ENCRYPTION="Y"  ENGINE = InnoDB;

   /*  Create NON encrypt table with encryption */
   CREATE TABLE tde_db.t_non_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;

   /*  insert into encrypt table */
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

   /*  insert into non encrypt table */
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
                       )  ENCRYPTION="Y" ENGINE = InnoDB;
   CREATE TABLE tde_db.t_non_encrypt_2(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;
   /*  insert into encrypt table 2 */
   INSERT INTO tde_db.t_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_encrypt_2 */';
   SELECT COUNT(*) FROM tde_db.t_encrypt_2;
   SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
   /* insert into NON encrypt table 2 */
   INSERT INTO tde_db.t_non_encrypt_2(c2,c3,c4,c7) SELECT c2,c3,c4,c7 FROM tde_db.t_encrypt;
   SELECT '/*  select tde_db.t_non_encrypt_2 */';
   SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
   SELECT c2 ,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_2 LIMIT 10;
end|
DELIMITER ;|

#------------------------------------------------------------------------------
--echo # Test 3 : Restart without empty keyring but later restore
--echo # Access all tables
--echo #-----------------------------------------------------------------------
--echo # init tables
call tde_db.init_setup();

--source include/keyring_tests/helper/local_keyring_file_backup.inc 
--echo # restart without keyring 
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;

# All tables accessible
SELECT COUNT(*) FROM tde_db.t_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt_2 LIMIT 10;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt_2 LIMIT 10;

--echo # insert into old encrypt tables
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
INSERT INTO tde_db.t_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # insert into old non encrypt tables
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
INSERT INTO tde_db.t_non_encrypt_2(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # update into old encrypt tables
UPDATE tde_db.t_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # update into old non encrypt tables
UPDATE tde_db.t_non_encrypt_2 SET c2 = 1000 WHERE c2 = 1;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
--echo # delete into old encrypt tables
DELETE FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_encrypt_2 WHERE c2 = 1000 ;
--echo # delete into old non encrypt tables
DELETE FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2 WHERE c2 = 1000 ;

--echo # new table
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENCRYPTION="Y" ENGINE = InnoDB;
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;


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

# Take backup of key data and replace it with empty file
--source include/keyring_tests/helper/local_keyring_file_backup.inc 
# Then reload the keyring so that it reinitializes itself with empty file
ALTER INSTANCE RELOAD KEYRING;


--error ER_CANNOT_FIND_KEY_IN_KEYRING
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENCRYPTION="Y" ENGINE = InnoDB;

--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER INSTANCE ROTATE INNODB MASTER KEY;

# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;

--echo #-----------------------------------------------------------------------
--echo # Test 4 : Restart with new keyring_data_file
--echo # Old encrypt table not accessible , non encrypt tables accessible
--echo # And creation of new encrypt,non encrypt table is also posible
#------------------------------------------------------------------------------
--echo # restart with keyring to load initial data 
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

--echo # init tables
call tde_db.init_setup();
--echo # restart with with different keyring file

# Take backup of key data and replace it with empty file
--source include/keyring_tests/helper/local_keyring_file_backup.inc 
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--echo # encrypt table not accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # NON encrypt table are accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;

--echo # new encrypt table is possible
CREATE TABLE tde_db.t_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENCRYPTION="Y" ENGINE = InnoDB;

--echo # non encrypt table possible
CREATE TABLE tde_db.t_non_encrypt_4(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON ,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;

INSERT INTO tde_db.t_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
INSERT INTO tde_db.t_non_encrypt_4(c3,c4,c7) SELECT c3,c4,c7 FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo # old encrypt table not accessible
--error ER_CANNOT_FIND_KEY_IN_KEYRING, ER_NO_SUCH_TABLE
SELECT COUNT(*) FROM tde_db.t_encrypt;
--error ER_CANNOT_FIND_KEY_IN_KEYRING, ER_NO_SUCH_TABLE
SELECT COUNT(*) FROM tde_db.t_encrypt_2;
--echo # NON encrypt old table are accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt;
SELECT COUNT(*) FROM tde_db.t_non_encrypt_2;
--echo # new encrypt table accessible
SELECT COUNT(*) FROM tde_db.t_encrypt_4;
--echo # new NON encrypt table accessible
SELECT COUNT(*) FROM tde_db.t_non_encrypt_4;

DROP TABLE tde_db.t_encrypt , tde_db.t_encrypt_2 ,tde_db.t_encrypt_4;
DROP TABLE tde_db.t_non_encrypt , tde_db.t_non_encrypt_2 ,tde_db.t_non_encrypt_4;

DROP DATABASE tde_db;

# Restore original keyring file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
# Reload keyring
ALTER INSTANCE RELOAD KEYRING;

# Cleanup
eval SET GLOBAL innodb_file_per_table=$innodb_file_per_table;
