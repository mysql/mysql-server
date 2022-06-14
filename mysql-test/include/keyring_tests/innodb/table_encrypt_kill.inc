#------------------------------------------------------------------------------
# InnoDB transparent tablespace data encryption
# This test case will test basic recovery.
# Test does following
#  - runs command in parallel on encrypt table
#  - server is killed
#  - server restarted with same options
#------------------------------------------------------------------------------

#suppress warning
-- disable_query_log
call mtr.add_suppression("does not exist in the InnoDB internal data dictionary though MySQL is trying to drop it");
call mtr.add_suppression("Operating system error number");
call mtr.add_suppression("The error means the system cannot find the path specified");
call mtr.add_suppression("If you are installing InnoDB, remember that you must create directories yourself, InnoDB does not create them");
call mtr.add_suppression("Could not find a valid tablespace file");
call mtr.add_suppression("Ignoring tablespace");
call mtr.add_suppression("Failed to find tablespace for table");
call mtr.add_suppression("Cannot create keyring directory");
call mtr.add_suppression("\\[Warning\\] .* Please refer to .* for how to resolve the issue");
call mtr.add_suppression("\\[Warning\\] .* Database page corruption or a failed file read of page");
call mtr.add_suppression("\\[Warning\\] .* Cannot calculate statistics for table");
call mtr.add_suppression("\\[ERROR\\] .* Table tde_db/t_encrypt.* in the InnoDB data dictionary has tablespace id .*, but tablespace with that id or name does not exist");
-- enable_query_log

let $innodb_file_per_table = `SELECT @@innodb_file_per_table`;

#------------------------------------------------------------------------------
# Initial setup

--disable_warnings
DROP DATABASE IF EXISTS tde_db;
DROP TABLE IF EXISTS tde_db. t_encrypt;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings

SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;

# Create a table with encryption
CREATE TABLE tde_db.t_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) ENCRYPTION="Y"  ENGINE = InnoDB;

CREATE TABLE tde_db.t_non_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;


DELIMITER |;
CREATE PROCEDURE tde_db.populate_t_encrypt()
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 5000) DO
                insert into tde_db.t_encrypt(c2,c3,c4,c7) VALUES(i,CONCAT(REPEAT('a',200),LPAD(CAST(i AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
                set i = i + 1;
        end while;
end|
CREATE PROCEDURE tde_db.populate_t_non_encrypt()
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 5000) DO
                insert into tde_db.t_non_encrypt(c2,c3,c4,c7) VALUES(i,CONCAT(REPEAT('a',200),LPAD(CAST(i AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
                set i = i + 1;
        end while;
end|
CREATE PROCEDURE tde_db.update_t_encrypt()
begin
        declare i int default 1;
        declare ustr varchar(1000);
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 5000) DO
                SET @sql_text =  CONCAT (' UPDATE tde_db.t_encrypt SET c3 =',  'CONCAT(REPEAT(a,200),LPAD(CAST(',i, 'AS CHAR),4,0) ORDER BY RAND() LIMIT 1');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                DEALLOCATE PREPARE stmt;
                set i = i + 1;
        end while;
end|

CREATE PROCEDURE tde_db.delete_t_encrypt()
begin
        declare i int default 1;
        declare ustr varchar(1000);
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 5000) DO
                SET @sql_text =  CONCAT (' DELETE FROM tde_db.t_encrypt LIMIT 1');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                DEALLOCATE PREPARE stmt;
                set i = i + 1;
        end while;
end|


CREATE PROCEDURE tde_db.read_t_encrypt()
begin
        declare i int default 1;
        while (i <= 5000) DO
                SELECT * FROM (SELECT * FROM tde_db.t_encrypt ORDER BY RAND() LIMIT 1) AS A WHERE A.c2 < 0 ;
                set i = i + 1;
        end while;
end|
CREATE PROCEDURE tde_db.alter_t_encrypt()
begin
        declare i int default 1;
        declare has_error int default 0;
        while (i <= 5000) DO
                ALTER INSTANCE ROTATE INNODB MASTER KEY;
                set i = i + 1;
        end while;
end|
CREATE PROCEDURE tde_db.create_t_encrypt(encrypt VARCHAR(5), tcnt INT)
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1050 SET has_error = 1;
        SET i = tcnt ;
        while (i <= 5000) DO
                SET @sql_text = CONCAT('CREATE TABLE ',CONCAT('tde_db.t_encrypt_',encrypt,'_',i),' (c1 INT) ENCRYPTION="',encrypt,'"',' ENGINE=InnoDB');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                DEALLOCATE PREPARE stmt;
                set i = i + 1;
        end while;
end|
DELIMITER ;|


SHOW CREATE TABLE tde_db.t_encrypt;

--echo # Case1: insert on encrypt and non encrypt table in parallel during kill
--echo # In connection con1 - Running insert on encrypt table
connect (con1,localhost,root,,);
send call tde_db.populate_t_encrypt();
--echo # In connection con2 - Running insert on encrypt table
connect (con2,localhost,root,,);
send call tde_db.populate_t_encrypt();
--echo # In connection con3 - Running insert into non encrypt table
connect (con3,localhost,root,,);
send call tde_db.populate_t_non_encrypt();


--connection default
--echo # kill and restart the server
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--sleep 3
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "$restart_parameters" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect

--disconnect con1
--disconnect con2
--disconnect con3
--connection default
--disable_result_log
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
INSERT INTO tde_db.t_encrypt(c2,c3,c4,c7) VALUES(10000,CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
INSERT INTO tde_db.t_non_encrypt(c2,c3,c4,c7) VALUES(10000,CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
--enable_result_log
SELECT 1;


--echo # Case2: insert/update/delete on encrypt in parallel during kill
DROP TABLE tde_db.t_encrypt;
DROP TABLE tde_db.t_non_encrypt;
CREATE TABLE tde_db.t_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) ENCRYPTION="Y"  ENGINE = InnoDB;

CREATE TABLE tde_db.t_non_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;

--echo # In connection con1 - Running insert on encrypt table
connect (con1,localhost,root,,);
send call tde_db.populate_t_encrypt();
--echo # In connection con2 - Running update on encrypt table
connect (con2,localhost,root,,);
send call tde_db.update_t_encrypt();
--echo # In connection con3 - Running delete on encrypt table
connect (con3,localhost,root,,);
send call tde_db.delete_t_encrypt();


--connection default
--echo # kill and restart the server
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--sleep 3

--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "$restart_parameters" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect


--disconnect con1
--disconnect con2
--disconnect con3
--connection default
--disable_result_log
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
INSERT INTO tde_db.t_encrypt(c2,c3,c4,c7) VALUES(10000,CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
INSERT INTO tde_db.t_non_encrypt(c2,c3,c4,c7) VALUES(10000,CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
--enable_result_log
SELECT 1;

--echo # Case3: select,create and insert on encrypt in parallel during kill
DROP TABLE tde_db.t_encrypt;
DROP TABLE tde_db.t_non_encrypt;
CREATE TABLE tde_db.t_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       ) ENCRYPTION="Y"  ENGINE = InnoDB;

CREATE TABLE tde_db.t_non_encrypt(c2 INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                       c3 CHAR(255) Default 'No text',
                       c4 JSON,
                       c5 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_a')) STORED,
                       c6 INT GENERATED ALWAYS AS (JSON_EXTRACT(c4,'$.key_b')) VIRTUAL,
                       c7 POINT NOT NULL SRID 0,
                       spatial INDEX idx2 (c7)
                       )  ENGINE = InnoDB;

--echo # In connection con1 - Running insert on encrypt table
connect (con1,localhost,root,,);
send call tde_db.populate_t_encrypt();
--echo # In connection con2 - Running select on encrypt table
connect (con2,localhost,root,,);
send call tde_db.read_t_encrypt();


--connection default
--echo # kill and restart the server

let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--sleep 1
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR $KEYRING_PLUGIN_OPT --plugin-dir=KEYRING_PLUGIN_PATH $KEYRING_PLUGIN keyring_file.so
--exec echo "$restart_parameters" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect


--disconnect con1
--disconnect con2
--connection default
--disable_result_log
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
INSERT INTO tde_db.t_encrypt(c2,c3,c4,c7) VALUES(10000,CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
INSERT INTO tde_db.t_non_encrypt(c2,c3,c4,c7) VALUES(10000,CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
--enable_result_log
SELECT 1;



#--echo # Case5: insert,create,alter on encrypt in parallel during kill
#--echo # In connection con1 - Running insert on encrypt table
#connect (con1,localhost,root,,);
#send call tde_db.populate_t_encrypt();
#--echo # In connection con2 - Running alter instance
#connect (con2,localhost,root,,);
#send call tde_db.alter_t_encrypt();
#--echo # In connection con3 - Running create on encrypt table
#connect (con3,localhost,root,,);
#send call tde_db.create_t_encrypt("Y",500);

#--connection default
#--echo # kill and restart the server
#let $restart_parameters = restart: $PLUGIN_DIR_OPT;
#--sleep 3
#--source include/kill_and_restart_mysqld.inc
#--disconnect con1
#--disconnect con2
#--disconnect con3
#--connection default
#--disable_result_log
#SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_encrypt LIMIT 10;
#INSERT INTO tde_db.t_encrypt(c3,c4,c7) VALUES(CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
#SELECT c2,right(c3,20),c4,c5,c6,ST_AsText(c7) FROM tde_db.t_non_encrypt LIMIT 10;
#INSERT INTO tde_db.t_non_encrypt(c3,c4,c7) VALUES(CONCAT(REPEAT('a',200),LPAD(CAST(1 AS CHAR),4,'0')),'{ "key_a": 1, "key_b": 2, "key_c": 3 }',ST_GeomFromText('POINT(383293632 1754448)'));
#--enable_result_log
#SELECT 1;

DROP DATABASE tde_db;

# Cleanup
eval SET GLOBAL innodb_file_per_table=$innodb_file_per_table;
