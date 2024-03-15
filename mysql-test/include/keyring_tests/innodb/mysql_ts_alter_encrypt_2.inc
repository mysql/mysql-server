################################################################################
# InnoDB transparent tablespace data encryption for mysql.tablespace.
# For mysql.tablespace, this test will test
#    1 - Alter mysql.tablespace with
#        - and without loading keyring
#        - valid/invalid encryption option
#        -ENCRYPTION='n' to ENCRYPTION='y'
#    2 - Actual encryption taking place or not
#    3 - Create/Alter table using mysql.tablespace
#    4 - Alter table part of innodb_file_per_table/general/innodb_system/mysql ts to mysql ts
#    5 - Create view using mysql.tablespace
#    6 - Delete/truncate/drop DD table part of mysql tablespace
#    7 - Changing encryption attribute of tables in mysql tablespace
#    8 - Restart with same keyring option and access mysql tables
#   10 - Monitor progress of encryption in performance_schema table
#   11 - Rename/drop mysql tablespace
#   12 - Create tablespace with name as mysql
################################################################################

--disable_query_log
call mtr.add_suppression("Error generating data for Data ID");
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] \\[[^]]*\\] Check keyring fail, please check the keyring is loaded.");
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] \\[[^]]*\\] Encryption can't find master key, please check the keyring is loaded.");
--enable_query_log

--echo #########
--echo # SETUP #
--echo #########
--echo

# Take backup of instance manifest
--source include/keyring_tests/helper/instance_backup_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--echo #########################################################################
--echo # START : WITHOUT KEYRING
--echo #########################################################################
# Alter mysql.tablespace with encryption, should fail since keyring is not
# loaded.

--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER TABLESPACE mysql ENCRYPTION='Y';

--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER TABLESPACE mysql ENCRYPTION='N';

--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo #########################################################################
--echo # RESTART 1 : WITH KEYRING
--echo #########################################################################
# Restore instance manifest from backup
--source include/keyring_tests/helper/instance_restore_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--echo --------------------------------------------------
--echo By Default, mysql tablespace should be unencrypted
--echo --------------------------------------------------
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';
# mysql.ibd file should be actually unencrypted
--source include/if_encrypted.inc

ALTER TABLESPACE mysql ENCRYPTION='Y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';
# mysql.ibd file should be actually encrypted
--source include/if_encrypted.inc

--echo -----------------------------------------------------
--echo ALTER mysql TABLESPACE WITH INVALID ENCRYPTION OPTION
--echo -----------------------------------------------------
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='R';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='TRUE';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='True';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='true';
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=TRUE;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=True;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=true;
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='FALSE';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='False';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='false';
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=FALSE;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=False;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=false;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=0;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=1;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=null;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=-1;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=n;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=N;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=y;
--error ER_PARSE_ERROR
ALTER TABLESPACE mysql ENCRYPTION=Y;
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='1';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='1True';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='@';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='null';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION='';
--error ER_INVALID_ENCRYPTION_OPTION
ALTER TABLESPACE mysql ENCRYPTION="";

--echo ----------------------------------------------------
--echo ALTER MYSQL TABLESPACE WITH VALID ENCRYPTION OPTION
--echo ----------------------------------------------------
# ALTER mysql TABLESPACE WITH ENCRYPTION='Y'.
ALTER TABLESPACE mysql ENCRYPTION='Y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';

# ALTER mysql TABLESPACE WITH ENCRYPTION='y'.
ALTER TABLESPACE mysql ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';

# ALTER mysql TABLESPACE WITH ENCRYPTION='n'.
ALTER TABLESPACE mysql ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';

# ALTER mysql TABLESPACE WITH ENCRYPTION='N'.
ALTER TABLESPACE mysql ENCRYPTION='N';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';

#Below SQL uses double quotes
# ALTER mysql TABLESPACE WITH ENCRYPTION="Y".
ALTER TABLESPACE mysql ENCRYPTION="Y";
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';

# ALTER mysql TABLESPACE WITH ENCRYPTION="y".
ALTER TABLESPACE mysql ENCRYPTION="y";
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';

# ALTER mysql TABLESPACE WITH ENCRYPTION="n".
ALTER TABLESPACE mysql ENCRYPTION="n";
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';

# ALTER mysql TABLESPACE WITH ENCRYPTION="N".
ALTER TABLESPACE mysql ENCRYPTION="N";
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';


--echo -----------------------------------------------------
--echo Create/Alter table using mysql.tablespace
--echo -----------------------------------------------------
# This should fail as mysql is a reserved tablespace
--error ER_RESERVED_TABLESPACE_NAME
CREATE TABLE t1(i int) TABLESPACE mysql;
--error ER_RESERVED_TABLESPACE_NAME
CREATE TEMPORARY TABLE t1(i int) TABLESPACE mysql;

# Alter table part of innodb_file_per_table ts to mysql ts
CREATE TABLE t1(i int);
--error ER_RESERVED_TABLESPACE_NAME
ALTER TABLE t1 TABLESPACE mysql;
DROP TABLE t1;

# Alter table part of general ts to mysql ts
CREATE TABLESPACE encrypt_ts ADD DATAFILE 'encrypt_ts.ibd' engine=INNODB;
CREATE TABLE t1(i int) TABLESPACE encrypt_ts;
--error ER_RESERVED_TABLESPACE_NAME
ALTER TABLE t1 TABLESPACE mysql;
DROP TABLE t1;

# Alter table part of innodb_system to mysql ts
CREATE TABLE t1(i int) TABLESPACE innodb_system;
--error ER_RESERVED_TABLESPACE_NAME
ALTER TABLE t1 TABLESPACE mysql;
DROP TABLE t1;

# Alter table part of mysql ts to mysql ts
# This should be successful
ALTER TABLE mysql.component TABLESPACE mysql;

# Alter temporary table to mysql ts
CREATE TEMPORARY TABLE t1(i int);
--error ER_RESERVED_TABLESPACE_NAME
ALTER TABLE t1 TABLESPACE mysql;
DROP TABLE t1;

--echo -----------------------------------------------------
--echo Create view using mysql.tablespace
--echo -----------------------------------------------------
# This should throw parse error as view does not support tablespace attribute
CREATE TABLE t1(i int);
INSERT INTO t1 VALUES(1);
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
--error ER_PARSE_ERROR
CREATE VIEW v1 AS SELECT * FROM t1 TABLESPACE mysql;
DROP TABLE t1;

--echo ------------------------------------------------------------
--echo Alter DD table part of mysql.tablespace to other tablespaces
--echo ------------------------------------------------------------
# This should fail as access to DD table is not allowed
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE innodb_file_per_table ENCRYPTION='Y';
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE innodb_file_per_table;
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE innodb_temporary ENCRYPTION='Y';
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE innodb_temporary;
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE innodb_system ENCRYPTION='Y';
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE innodb_system;
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE encrypt_ts ENCRYPTION='Y';
--error ER_NO_SYSTEM_TABLE_ACCESS
ALTER TABLE mysql.events TABLESPACE encrypt_ts;
DROP TABLESPACE encrypt_ts;

--echo -------------------------------------------------------
--echo  Delete/truncate/drop DD table part of mysql tablespace
--echo -------------------------------------------------------
# Access should be denied
--error ER_NO_SYSTEM_TABLE_ACCESS
DELETE FROM mysql.events;
--error ER_NO_SYSTEM_TABLE_ACCESS
TRUNCATE TABLE mysql.events;
--error ER_NO_SYSTEM_TABLE_ACCESS
DROP TABLE mysql.events;

--echo --------------------------------------------------
--echo Alter encryption of table part of mysql tablespace
--echo --------------------------------------------------
# Changing encryption at table level should not be permitted
--error ER_INVALID_ENCRYPTION_REQUEST
ALTER TABLE mysql.component ENCRYPTION='Y';
--error ER_INVALID_ENCRYPTION_REQUEST
ALTER TABLE mysql.component ENCRYPTION='y';
ALTER TABLE mysql.component ENCRYPTION='N';
ALTER TABLE mysql.component ENCRYPTION='n';

--echo ---------------------------------------------------------------------
--echo Metadata for a table in mysql ts should not show encryption attribute
--echo ---------------------------------------------------------------------
SHOW CREATE TABLE mysql.plugin;


--echo -------------------
--echo Drop mysql database
--echo -------------------
--error ER_NO_SYSTEM_SCHEMA_ACCESS
DROP DATABASE mysql;

--echo -----------------------------------------------------
--echo Other DDL operation not allowed on 'mysql' tablespace
--echo -----------------------------------------------------
--error ER_WRONG_TABLESPACE_NAME
CREATE TABLESPACE mysql ADD DATAFILE 'mysql.ibd' ENGINE=INNODB;

--error ER_WRONG_TABLESPACE_NAME
DROP TABLESPACE mysql;

--error ER_WRONG_TABLESPACE_NAME
ALTER TABLESPACE mysql RENAME TO xyz;

--echo #########################################################################
--echo # Restart with same keyring option
--echo #   - tables in mysql ts should be accessible
--echo #########################################################################
ALTER TABLESPACE mysql ENCRYPTION='Y';
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';
SELECT help_keyword_id FROM mysql.help_keyword ORDER BY help_keyword_id LIMIT 2;
ALTER TABLESPACE mysql ENCRYPTION='N';
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
ALTER TABLESPACE mysql ENCRYPTION='Y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';
ALTER INSTANCE ROTATE INNODB MASTER KEY;
ALTER TABLESPACE mysql ENCRYPTION='N';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES where NAME='mysql';
--echo # Set Encryption process to wait after page 5 so that we can monitor
--echo # progress in performance_schema table
SET DEBUG_SYNC = 'alter_encrypt_tablespace_wait_after_page5 SIGNAL s1 WAIT_FOR s2';

let $con_default_thread_id= `select THREAD_ID from performance_schema.threads
                                where PROCESSLIST_ID = connection_id()`;

--send
ALTER TABLESPACE mysql ENCRYPTION='Y';
--echo # Monitoring connection
connect(con1, localhost, root,,);
--connection con1
SET DEBUG_SYNC = 'now WAIT_FOR s1';

--echo # Wait for Encryption progress monitoring to appear in PFS table
let $wait_condition = SELECT COUNT(*) = 1
        FROM performance_schema.events_stages_current
        WHERE EVENT_NAME='stage/innodb/alter tablespace (encryption)';
--source include/wait_condition.inc

--echo # Wait for some progress to appear in PFS table
let $wait_condition = SELECT WORK_COMPLETED > 0
    FROM performance_schema.events_stages_current
    WHERE EVENT_NAME='stage/innodb/alter tablespace (encryption)';
--source include/wait_condition.inc

# Verify the expected progress seen in PFS table
select WORK_COMPLETED
    FROM performance_schema.events_stages_current
    WHERE EVENT_NAME = 'stage/innodb/alter tablespace (encryption)';

# Let the encryption processing continue
SET DEBUG_SYNC = 'now SIGNAL s2';

--echo # Default connection
--connection default
--reap

--echo # Once done, select count from PFS tables
SELECT COUNT(*)
    FROM performance_schema.events_stages_current
    WHERE EVENT_NAME='stage/innodb/alter tablespace (encryption)';

SELECT COUNT(*)
    FROM performance_schema.events_stages_history
    WHERE EVENT_NAME='stage/innodb/alter tablespace (encryption)';

SELECT COUNT(*)
    FROM performance_schema.events_stages_history_long
    WHERE EVENT_NAME='stage/innodb/alter tablespace (encryption)';

SELECT COUNT(*)
    FROM performance_schema.events_stages_summary_global_by_event_name
    WHERE EVENT_NAME = 'stage/innodb/alter tablespace (encryption)' AND
    COUNT_STAR>0;

--disable_query_log
eval SELECT COUNT(*)
    FROM performance_schema.events_stages_summary_by_thread_by_event_name
    WHERE EVENT_NAME = 'stage/innodb/alter tablespace (encryption)' AND
    THREAD_ID=$con_default_thread_id;
--enable_query_log

SELECT COUNT(*)
    FROM performance_schema.events_stages_summary_by_user_by_event_name
    WHERE EVENT_NAME = 'stage/innodb/alter tablespace (encryption)' AND
    COUNT_STAR>0;

SELECT COUNT(*)
    FROM performance_schema.events_stages_summary_by_host_by_event_name
    WHERE EVENT_NAME = 'stage/innodb/alter tablespace (encryption)' AND
    COUNT_STAR>0;

SELECT COUNT(*)
    FROM performance_schema.events_stages_summary_by_account_by_event_name
    WHERE EVENT_NAME = 'stage/innodb/alter tablespace (encryption)' AND
    COUNT_STAR>0;

--echo # Check that Encryption done successfully.
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
    WHERE NAME='mysql';
SELECT help_keyword_id FROM mysql.help_keyword ORDER BY help_keyword_id LIMIT 2;

--echo ###########
--echo # Cleanup #
--echo ###########
--disconnect con1
 # Unencrypt tablespace
ALTER TABLESPACE mysql ENCRYPTION='N';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';

remove_file $MYSQLTEST_VARDIR/tmpfile.txt;
