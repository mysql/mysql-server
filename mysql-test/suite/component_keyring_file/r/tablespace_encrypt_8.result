# ----------------------------------------------------------------------
# Setup
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
# Re-starting mysql server with manifest file
# ----------------------------------------------------------------------
#########################################################################
# RESTART 1 : WITH KEYRING
#########################################################################
SET SESSION debug= '+d,ddl_btree_build_insert_return_interrupt';

#########
# SETUP #
#########
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
CREATE TABLE t1(class INT, id INT, title VARCHAR(100)) encryption='N';
CREATE TABLESPACE encrypt_ts ADD DATAFILE 'encrypt_ts.ibd' encryption='N';
ALTER TABLE t1 TABLESPACE=encrypt_ts, ALGORITHM=INPLACE;
ERROR 70100: Query execution was interrupted
ALTER TABLESPACE encrypt_ts ENCRYPTION='Y';
#########################################################################
# RESTART 2 : WITH KEYRING PLUGIN
#########################################################################
ALTER TABLESPACE encrypt_ts ENCRYPTION='N';
###########
# Cleanup #
###########
DROP TABLE t1;
DROP TABLESPACE encrypt_ts;
DROP PROCEDURE populate_t1;
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Restarting server without the manifest file
# ----------------------------------------------------------------------
