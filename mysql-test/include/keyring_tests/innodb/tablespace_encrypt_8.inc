################################################################################
# InnoDB transparent tablespace data encryption for general shared tablespace.
# This test case will test
#    - Situation to encrypt a tablespace which has half initialized page.
################################################################################
# Waiting time when (re)starting the server
--let $explicit_default_wait_counter=10000

--echo #########################################################################
--echo # RESTART 1 : WITH KEYRING
--echo #########################################################################
let $restart_parameters = restart: $PLUGIN_DIR_OPT --performance-schema-consumer-events-stages-current=ON --performance-schema-events-stages-history-size=1000;
--source include/restart_mysqld_no_echo.inc

SET SESSION debug= '+d,ddl_btree_build_insert_return_interrupt';

--echo
--echo #########
--echo # SETUP #
--echo #########

# Create Insert Procedure
DELIMITER |;
CREATE PROCEDURE populate_t1()
BEGIN
        DECLARE i int DEFAULT 1;

        START TRANSACTION;
        WHILE (i <= 10000) DO
                INSERT INTO t1 VALUES (i, i, CONCAT('a', i));
                SET i = i + 1;
        END WHILE;
        COMMIT;
END|
DELIMITER ;|

# Create an encrypted table in tablespace
CREATE TABLE t1(class INT, id INT, title VARCHAR(100)) encryption='N';

-- disable_query_log
CALL populate_t1();
-- enable_query_log

# Create an encrypted tablespace
CREATE TABLESPACE encrypt_ts ADD DATAFILE 'encrypt_ts.ibd' encryption='N';

--error ER_QUERY_INTERRUPTED
ALTER TABLE t1 TABLESPACE=encrypt_ts, ALGORITHM=INPLACE;

ALTER TABLESPACE encrypt_ts ENCRYPTION='Y';

--echo #########################################################################
--echo # RESTART 2 : WITH KEYRING PLUGIN
--echo #########################################################################
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

ALTER TABLESPACE encrypt_ts ENCRYPTION='N';

--echo ###########
--echo # Cleanup #
--echo ###########
DROP TABLE t1;
DROP TABLESPACE encrypt_ts;
DROP PROCEDURE populate_t1;
